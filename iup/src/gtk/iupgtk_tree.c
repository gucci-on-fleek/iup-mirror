/** \file
 * \brief Tree Control
 *
 * See Copyright Notice in iup.h
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdarg.h>

#include "iup.h"
#include "iupcbs.h"

#include "iup_object.h"
#include "iup_layout.h"
#include "iup_attrib.h"
#include "iup_str.h"
#include "iup_drv.h"
#include "iup_drvfont.h"
#include "iup_stdcontrols.h"
#include "iup_key.h"
#include "iup_image.h"
#include "iup_array.h"
#include "iup_tree.h"

#include "iup_drvinfo.h"
#include "iupgtk_drv.h"


/* IMPORTANT: 

  GtkTreeStore uses the "user_data" field of the GtkTreeIter 
  to store the node pointer that is position independent.
  So we use it as a reference to the node in the cache, just like in Motif and Windows.

  BUT if GTK change its implementation this must be changed also. See "gtk_tree_store.c".

  ABOUT SELECTIONS:

  From the GTK documentation on GtkTreeSelection

  "Additionally, you cannot change the selection of a row on the model 
   that is not currently displayed by the view without expanding its parents first."
*/

enum
{
  IUPGTK_TREE_IMAGE,  /* "pixbuf", "pixbuf-expander-closed" */
  IUPGTK_TREE_HAS_IMAGE,
  IUPGTK_TREE_IMAGE_EXPANDED, /* "pixbuf-expander-open" */
  IUPGTK_TREE_HAS_IMAGE_EXPANDED,
  IUPGTK_TREE_TITLE, /* "text" */
  IUPGTK_TREE_KIND,  /* "is-expander" */
  IUPGTK_TREE_COLOR, /* "foreground-gdk" */
  IUPGTK_TREE_FONT,  /* "font-desc" */
  IUPGTK_TREE_SELECT,
  IUPGTK_TREE_LAST_DATA /* used as a count */
};

static void gtkTreeRebuildNodeCache(Ihandle* ih, int id, GtkTreeIter iterItem);

/*****************************************************************************/
/* COPYING ITEMS (Branches and its children)                                 */
/*****************************************************************************/
/* Insert the copied item in a new location. Returns the new item. */
static void gtkTreeCopyItem(Ihandle* ih, GtkTreeModel* model, GtkTreeIter* iterItem, GtkTreeIter* iterParent, int position, GtkTreeIter *iterNewItem)
{
  GtkTreeStore* store = GTK_TREE_STORE(model);
  int kind;
  char* title;
  gboolean has_image, has_image_expanded;
  PangoFontDescription* font;
  GdkColor *color;
  GdkPixbuf* image, *image_expanded;

  gtk_tree_model_get(GTK_TREE_MODEL(store), iterItem, IUPGTK_TREE_IMAGE,      &image,
                                                      IUPGTK_TREE_HAS_IMAGE,      &has_image,
                                                      IUPGTK_TREE_IMAGE_EXPANDED,  &image_expanded,
                                                      IUPGTK_TREE_HAS_IMAGE_EXPANDED,  &has_image_expanded,
                                                      IUPGTK_TREE_TITLE,  &title,
                                                      IUPGTK_TREE_KIND,  &kind,
                                                      IUPGTK_TREE_COLOR, &color, 
                                                      IUPGTK_TREE_FONT, &font, 
                                                      -1);

  /* Add the new node */
  ih->data->node_count++;
  if (position == 2)
    gtk_tree_store_append(store, iterNewItem, iterParent);
  else if (position == 1)                                      /* copy as first child of expanded branch */
    gtk_tree_store_insert(store, iterNewItem, iterParent, 0);  /* iterParent is parent of the new item (firstchild of it) */
  else                                                                  /* copy as next brother of item or collapsed branch */
    gtk_tree_store_insert_after(store, iterNewItem, NULL, iterParent);  /* iterParent is sibling of the new item */

  gtk_tree_store_set(store, iterNewItem,  IUPGTK_TREE_IMAGE,      image,
                                          IUPGTK_TREE_HAS_IMAGE,  has_image,
                                          IUPGTK_TREE_IMAGE_EXPANDED,  image_expanded,
                                          IUPGTK_TREE_HAS_IMAGE_EXPANDED, has_image_expanded,
                                          IUPGTK_TREE_TITLE,  title,
                                          IUPGTK_TREE_KIND,  kind,
                                          IUPGTK_TREE_COLOR, color, 
                                          IUPGTK_TREE_FONT, font,
                                          IUPGTK_TREE_SELECT, 0,
                                          -1);
}

static void gtkTreeCopyChildren(Ihandle* ih, GtkTreeModel* model, GtkTreeIter *iterItemSrc, GtkTreeIter *iterItemDst)
{
  GtkTreeIter iterChildSrc;
  int hasItem = gtk_tree_model_iter_children(model, &iterChildSrc, iterItemSrc);  /* get the firstchild */
  while(hasItem)
  {
    GtkTreeIter iterNewItem;
    gtkTreeCopyItem(ih, model, &iterChildSrc, iterItemDst, 2, &iterNewItem);  /* append always */

    /* Recursively transfer all the items */
    gtkTreeCopyChildren(ih, model, &iterChildSrc, &iterNewItem);  

    /* Go to next sibling item */
    hasItem = gtk_tree_model_iter_next(model, &iterChildSrc);
  }
}

/* Copies all items in a branch to a new location. Returns the new branch node. */
static void gtkTreeCopyMoveNode(Ihandle* ih, GtkTreeModel* model, GtkTreeIter *iterItemSrc, GtkTreeIter *iterItemDst, GtkTreeIter* iterNewItem, int is_copy)
{
  int kind, position = 0; /* insert after iterItemDst */
  int id_new, count, id_src, id_dst;

  int old_count = ih->data->node_count;

  id_src = iupTreeFindNodeId(ih, iterItemSrc);
  id_dst = iupTreeFindNodeId(ih, iterItemDst);
  id_new = id_dst+1; /* contains the position for a copy operation */

  gtk_tree_model_get(model, iterItemDst, IUPGTK_TREE_KIND, &kind, -1);

  if (kind == ITREE_BRANCH)
  {
    GtkTreePath* path = gtk_tree_model_get_path(model, iterItemDst);
    if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(ih->handle), path))
      position = 1;  /* insert as first child of iterItemDst */
    else
    {
      int child_count = iupdrvTreeTotalChildCount(ih, iterItemDst);
      id_new += child_count;
    }
    gtk_tree_path_free(path);
  }

  /* move to the same place does nothing */
  if (!is_copy && id_new == id_src)
  {
    iterNewItem->stamp = 0;
    return;
  }

  gtkTreeCopyItem(ih, model, iterItemSrc, iterItemDst, position, iterNewItem);  

  gtkTreeCopyChildren(ih, model, iterItemSrc, iterNewItem);

  count = ih->data->node_count - old_count;
  iupTreeCopyMoveCache(ih, id_src, id_new, count, is_copy);

  if (!is_copy)
  {
    /* Deleting the node of its old position */
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");
    gtk_tree_store_remove(GTK_TREE_STORE(model), iterItemSrc);
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);

    /* restore count, because we remove src */
    ih->data->node_count = old_count;

    /* compensate position for a move */
    if (id_new > id_src)
      id_new -= count;
  }

  gtkTreeRebuildNodeCache(ih, id_new, *iterNewItem);
}

/*****************************************************************************/
/* FINDING ITEMS                                                             */
/*****************************************************************************/

static void gtkTreeIterInit(Ihandle* ih, GtkTreeIter* iterItem, InodeHandle* node_handle)
{
  iterItem->stamp = ih->data->stamp;
  iterItem->user_data = node_handle;
  iterItem->user_data2 = NULL;
  iterItem->user_data3 = NULL;
}

static int gtkTreeIsNodeSelected(GtkTreeModel* model, GtkTreeIter *iterItem)
{
  gboolean selected = 0;
  gtk_tree_model_get(model, iterItem, IUPGTK_TREE_SELECT, &selected, -1);
  return selected;
}

static void gtkTreeSelectNodeRaw(GtkTreeModel* model, GtkTreeIter *iterItem, int select)
{
  /* Cannot change the selection of a row on the model that is not currently displayed. 
     So we store the selection state here. And update the actual state when the node becames visible. */
  gtk_tree_store_set(GTK_TREE_STORE(model), iterItem, IUPGTK_TREE_SELECT, select, -1);
}

static void gtkTreeSelectNode(GtkTreeModel* model, GtkTreeSelection* selection, GtkTreeIter *iterItem, int select)
{
  if (select == -1)
    select = !gtkTreeIsNodeSelected(model, iterItem); /* toggle */

  gtkTreeSelectNodeRaw(model, iterItem, select);

  if (select)
    gtk_tree_selection_select_iter(selection, iterItem);
  else
    gtk_tree_selection_unselect_iter(selection, iterItem);
}

static void gtkTreeSelectAll(Ihandle* ih, GtkTreeModel* model, GtkTreeSelection* selection, int selected)
{
  int i;
  GtkTreeIter iterItem;

  for (i = 0; i < ih->data->node_count; i++)
  {
    gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
    gtkTreeSelectNodeRaw(model, &iterItem, selected);
  }

  if (selected)
    gtk_tree_selection_select_all(selection);
  else
    gtk_tree_selection_unselect_all(selection);
}

static void gtkTreeInvertAllNodeMarking(Ihandle* ih, GtkTreeModel* model, GtkTreeSelection* selection)
{
  int i;
  GtkTreeIter iterItem;

  for (i = 0; i < ih->data->node_count; i++)
  {
    gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
    gtkTreeSelectNode(model, selection, &iterItem, -1);
  }
}

static void gtkTreeSelectRange(Ihandle* ih, GtkTreeModel* model, GtkTreeSelection* selection, GtkTreeIter *iterItem1, GtkTreeIter *iterItem2, int clear)
{
  int i;
  int id1 = iupTreeFindNodeId(ih, iterItem1->user_data);
  int id2 = iupTreeFindNodeId(ih, iterItem2->user_data);
  GtkTreeIter iterItem;

  if (id1 > id2)
  {
    int tmp = id1;
    id1 = id2;
    id2 = tmp;
  }

  for (i = 0; i < ih->data->node_count; i++)
  {
    gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
    if (i < id1 || i > id2)
    {
      if (clear)
        gtkTreeSelectNode(model, selection, &iterItem, 0);
    }
    else
      gtkTreeSelectNode(model, selection, &iterItem, 1);
  }
}

static int gtkTreeIsNodeVisible(Ihandle* ih, GtkTreeModel* model, InodeHandle* node_handle, InodeHandle* *nodeLastParent)
{
  GtkTreeIter iterItem, iterParent;
  GtkTreePath* path;
  int is_visible;

  gtkTreeIterInit(ih, &iterItem, node_handle);

  if (!gtk_tree_model_iter_parent(model, &iterParent, &iterItem) ||
      iterParent.user_data == *nodeLastParent)
    return 1;

  path = gtk_tree_model_get_path(model, &iterParent);
  is_visible = gtk_tree_view_row_expanded(GTK_TREE_VIEW(ih->handle), path);
  gtk_tree_path_free(path);

  if (!is_visible)
    return 0;

  /* save last parent */
  *nodeLastParent = iterParent.user_data;
  return 1;
}

static void gtkTreeGetLastVisibleNode(Ihandle* ih, GtkTreeModel* model, GtkTreeIter *iterItem)
{
  int i;
  InodeHandle* nodeLastParent = NULL;

  for (i = ih->data->node_count-1; i >= 0; i--)
  {
    if (gtkTreeIsNodeVisible(ih, model, ih->data->node_cache[i].node_handle, &nodeLastParent))
    {
      gtkTreeIterInit(ih, iterItem, ih->data->node_cache[i].node_handle);
      return;
    }
  }

  gtkTreeIterInit(ih, iterItem, ih->data->node_cache[0].node_handle);  /* root is always visible */
}

static void gtkTreeGetNextVisibleNode(Ihandle* ih, GtkTreeModel* model, GtkTreeIter *iterItem, int count)
{
  int i, id;
  InodeHandle* nodeLastParent = NULL;

  id = iupTreeFindNodeId(ih, iterItem->user_data);
  id += count;

  for (i = id; i < ih->data->node_count; i++)
  {
    if (gtkTreeIsNodeVisible(ih, model, ih->data->node_cache[i].node_handle, &nodeLastParent))
    {
      gtkTreeIterInit(ih, iterItem, ih->data->node_cache[i].node_handle);
      return;
    }
  }

  gtkTreeIterInit(ih, iterItem, ih->data->node_cache[0].node_handle);  /* root is always visible */
}

static void gtkTreeGetPreviousVisibleNode(Ihandle* ih, GtkTreeModel* model, GtkTreeIter *iterItem, int count)
{
  int i, id;
  InodeHandle* nodeLastParent = NULL;

  id = iupTreeFindNodeId(ih, iterItem->user_data);
  id -= count;

  for (i = id; i >= 0; i--)
  {
    if (gtkTreeIsNodeVisible(ih, model, ih->data->node_cache[i].node_handle, &nodeLastParent))
    {
      gtkTreeIterInit(ih, iterItem, ih->data->node_cache[i].node_handle);
      return;
    }
  }

  gtkTreeGetLastVisibleNode(ih, model, iterItem);
}

static int gtkTreeFindNodeId(Ihandle* ih, GtkTreeIter* iterItem)
{
  return iupTreeFindNodeId(ih, iterItem->user_data);
}

static void gtkTreeCallNodeRemovedRec(Ihandle* ih, GtkTreeModel* model, GtkTreeIter *iterItem, IFns cb, int *id)
{
  GtkTreeIter iterChild;
  int hasItem;
  int old_id = *id;

  /* Check whether we have child items */
  /* remove from children first */
  hasItem = gtk_tree_model_iter_children(model, &iterChild, iterItem);  /* get the firstchild */
  while(hasItem)
  {
    /* go recursive to children */
    gtkTreeCallNodeRemovedRec(ih, model, &iterChild, cb, id);

    /* Go to next sibling item */
    hasItem = gtk_tree_model_iter_next(model, &iterChild);
  }

  /* actually do it for the node */
  ih->data->node_count--;
  (*id)++;

  cb(ih, (char*)ih->data->node_cache[old_id].userdata);
}

static void gtkTreeCallNodeRemoved(Ihandle* ih, GtkTreeModel* model, GtkTreeIter *iterItem)
{
  int old_count = ih->data->node_count;
  int id = iupTreeFindNodeId(ih, iterItem->user_data);
  int old_id = id;

  IFns cb = (IFns)IupGetCallback(ih, "NODEREMOVED_CB");
  if (cb) 
    gtkTreeCallNodeRemovedRec(ih, model, iterItem, cb, &id);
  else
  {
    int removed_count = iupdrvTreeTotalChildCount(ih, iterItem->user_data)+1;
    ih->data->node_count -= removed_count;
  }

  iupTreeDelFromCache(ih, old_id, old_count-ih->data->node_count);
}

static gboolean gtkTreeFindNodeFromString(Ihandle* ih, const char* name_id, GtkTreeIter *iterItem)
{
  InodeHandle* node_handle = iupTreeGetNodeFromString(ih, name_id);
  if (!node_handle)
    return FALSE;

  gtkTreeIterInit(ih, iterItem, node_handle);
  return TRUE;
}

/*****************************************************************************/
/* MANIPULATING IMAGES                                                       */
/*****************************************************************************/
static void gtkTreeUpdateImages(Ihandle* ih, GtkTreeModel* model, GtkTreeIter iterItem, int mode)
{
  GtkTreeIter iterChild;
  int hasItem = TRUE;
  int kind;

  while(hasItem)
  {
    gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_KIND, &kind, -1);

    if (kind == ITREE_BRANCH)
    {
      if (mode == ITREE_UPDATEIMAGE_EXPANDED)
      {
        gboolean has_image_expanded = FALSE;
        gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_HAS_IMAGE_EXPANDED, &has_image_expanded, -1);
        if (!has_image_expanded)
          gtk_tree_store_set(GTK_TREE_STORE(model), &iterItem, IUPGTK_TREE_IMAGE_EXPANDED, ih->data->def_image_expanded, -1);
      }
      else if(mode == ITREE_UPDATEIMAGE_COLLAPSED)
      {
        gboolean has_image = FALSE;
        gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_HAS_IMAGE, &has_image, -1);
        if (!has_image)
          gtk_tree_store_set(GTK_TREE_STORE(model), &iterItem, IUPGTK_TREE_IMAGE, ih->data->def_image_collapsed, -1);
      }

      if (gtk_tree_model_iter_has_child(model, &iterItem))
      {

        /* Recursively traverse child items */
        gtk_tree_model_iter_children(model, &iterChild, &iterItem);
        gtkTreeUpdateImages(ih, model, iterChild, mode);
      }
    }
    else 
    {
      if (mode == ITREE_UPDATEIMAGE_LEAF)
      {
        gboolean has_image = FALSE;
        gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_HAS_IMAGE, &has_image, -1);
        if (!has_image)
          gtk_tree_store_set(GTK_TREE_STORE(model), &iterItem, IUPGTK_TREE_IMAGE, ih->data->def_image_leaf, -1);
      }
    }

    /* Go to next sibling item */
    hasItem = gtk_tree_model_iter_next(model, &iterItem);
  }
}

static void gtkTreeExpandItem(Ihandle* ih, GtkTreePath* path, int expand)
{
  if (expand == -1)
    expand = !gtk_tree_view_row_expanded(GTK_TREE_VIEW(ih->handle), path); /* toggle */

  if (expand)
    gtk_tree_view_expand_row(GTK_TREE_VIEW(ih->handle), path, FALSE);
  else
    gtk_tree_view_collapse_row(GTK_TREE_VIEW(ih->handle), path);
}

int iupgtkGetColor(const char* value, GdkColor *color)
{
  unsigned char r, g, b;
  if (iupStrToRGB(value, &r, &g, &b))
  {
    iupgdkColorSet(color, r, g, b);
    return 1;
  }
  return 0;
}

/*****************************************************************************/
/* ADDING ITEMS                                                              */
/*****************************************************************************/
void iupdrvTreeAddNode(Ihandle* ih, const char* name_id, int kind, const char* title, int add)
{
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GtkTreeIter iterPrev, iterNewItem, iterParent;
  GtkTreePath* path;
  GdkColor color = {0L,0,0,0};
  int kindPrev;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterPrev))
    return;

  gtk_tree_model_get(GTK_TREE_MODEL(store), &iterPrev, IUPGTK_TREE_KIND, &kindPrev, -1);

  /* Add the new node */
  if (kindPrev == ITREE_BRANCH && add)
    gtk_tree_store_insert(store, &iterNewItem, &iterPrev, 0);  /* iterPrev is parent of the new item (firstchild of it) */
  else
    gtk_tree_store_insert_after(store, &iterNewItem, NULL, &iterPrev);  /* iterPrev is sibling of the new item */
  iupTreeAddToCache(ih, add, kindPrev, iterPrev.user_data, iterNewItem.user_data);

  iupgtkGetColor(iupAttribGetStr(ih, "FGCOLOR"), &color);

  if (!title)
    title = "";

  /* set the attributes of the new node */
  gtk_tree_store_set(store, &iterNewItem, IUPGTK_TREE_HAS_IMAGE, FALSE,
                                          IUPGTK_TREE_HAS_IMAGE_EXPANDED, FALSE,
                                          IUPGTK_TREE_TITLE, iupgtkStrConvertToUTF8(title),
                                          IUPGTK_TREE_KIND, kind,
                                          IUPGTK_TREE_COLOR, &color, 
                                          IUPGTK_TREE_SELECT, 0,
                                          -1);

  if (kind == ITREE_LEAF)
    gtk_tree_store_set(store, &iterNewItem, IUPGTK_TREE_IMAGE, ih->data->def_image_leaf, -1);
  else
    gtk_tree_store_set(store, &iterNewItem, IUPGTK_TREE_IMAGE, ih->data->def_image_collapsed,
                                            IUPGTK_TREE_IMAGE_EXPANDED, ih->data->def_image_expanded, -1);

  if (kindPrev == ITREE_BRANCH && add)
    iterParent = iterPrev;
  else
    gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &iterParent, &iterNewItem);

  /* If this is the first child of the parent, then handle the ADDEXPANDED attribute */
  if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), &iterParent) == 1)
  {
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iterParent);
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_BRANCH_CB", "1");
    if (ih->data->add_expanded)
      gtk_tree_view_expand_row(GTK_TREE_VIEW(ih->handle), path, FALSE);
    else
      gtk_tree_view_collapse_row(GTK_TREE_VIEW(ih->handle), path);
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_BRANCH_CB", NULL);
    gtk_tree_path_free(path);
  }
}

static void gtkTreeAddRootNode(Ihandle* ih)
{
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GtkTreePath* path;
  GtkTreeIter  iterRoot;
  GdkColor color = {0L,0,0,0};

  iupgtkGetColor(iupAttribGetStr(ih, "FGCOLOR"), &color);

  /* Add the new node */
  gtk_tree_store_append(store, &iterRoot, NULL);  /* root node */
  ih->data->stamp = iterRoot.stamp;
  ih->data->node_count = 1;
  ih->data->node_cache[0].node_handle = iterRoot.user_data;

  gtk_tree_store_set(store, &iterRoot, IUPGTK_TREE_IMAGE, ih->data->def_image_collapsed,
                                       IUPGTK_TREE_HAS_IMAGE, FALSE,
                                       IUPGTK_TREE_IMAGE_EXPANDED, ih->data->def_image_expanded,
                                       IUPGTK_TREE_HAS_IMAGE_EXPANDED, FALSE,
                                       IUPGTK_TREE_KIND, ITREE_BRANCH,
                                       IUPGTK_TREE_COLOR, &color, 
                                       IUPGTK_TREE_SELECT, 0,
                                       -1);

  /* MarkStart node */
  iupAttribSetStr(ih, "_IUPTREE_MARKSTART_NODE", (char*)iterRoot.user_data);

  /* Set the default VALUE */
  path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iterRoot);
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(ih->handle), path, NULL, FALSE);
  gtk_tree_path_free(path);
}

/*****************************************************************************/
/* AUXILIAR FUNCTIONS                                                        */
/*****************************************************************************/

static void gtkTreeChildRebuildCacheRec(Ihandle* ih, GtkTreeModel *model, GtkTreeIter *iterItem, int *id)
{
  GtkTreeIter iterChild;
  int hasItem = gtk_tree_model_iter_children(model, &iterChild, iterItem);  /* get the firstchild */
  while(hasItem)
  {
    (*id)++;
    ih->data->node_cache[*id].node_handle = iterChild.user_data;

    /* go recursive to children */
    gtkTreeChildRebuildCacheRec(ih, model, &iterChild, id);

    /* Go to next sibling item */
    hasItem = gtk_tree_model_iter_next(model, &iterChild);
  }
}

static void gtkTreeRebuildNodeCache(Ihandle* ih, int id, GtkTreeIter iterItem)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  ih->data->node_cache[id].node_handle = iterItem.user_data;
  gtkTreeChildRebuildCacheRec(ih, model, &iterItem, &id);
}

static void gtkTreeChildCountRec(GtkTreeModel *model, GtkTreeIter *iterItem, int *count)
{
  GtkTreeIter iterChild;
  int hasItem = gtk_tree_model_iter_children(model, &iterChild, iterItem);  /* get the firstchild */
  while(hasItem)
  {
    (*count)++;

    /* go recursive to children */
    gtkTreeChildCountRec(model, &iterChild, count);

    /* Go to next sibling item */
    hasItem = gtk_tree_model_iter_next(model, &iterChild);
  }
}

int iupdrvTreeTotalChildCount(Ihandle* ih, InodeHandle* node_handle)
{
  int count = 0;
  GtkTreeIter iterItem;
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  gtkTreeIterInit(ih, &iterItem, node_handle);
  gtkTreeChildCountRec(model, &iterItem, &count);
  return count;
}

InodeHandle* iupdrvTreeGetFocusNode(Ihandle* ih)
{
  GtkTreePath* path = NULL;
  gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &path, NULL);
  if (path)
  {
    GtkTreeIter iterItem;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
    gtk_tree_model_get_iter(model, &iterItem, path);
    gtk_tree_path_free(path);
    return iterItem.user_data;
  }

  return NULL;
}

static void gtkTreeOpenCloseEvent(Ihandle* ih)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  GtkTreePath* path;
  int kind;

  if (!gtkTreeFindNodeFromString(ih, "", &iterItem))
    return;

  path = gtk_tree_model_get_path(model, &iterItem);
  if (path)
  {
    gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_KIND, &kind, -1);

    if (kind == ITREE_LEAF)  /* leafs */
      gtk_tree_view_row_activated(GTK_TREE_VIEW(ih->handle), path, (GtkTreeViewColumn*)iupAttribGet(ih, "_IUPGTK_COLUMN"));     
    else  /* branches */
      gtkTreeExpandItem(ih, path, -1); /* toggle */

    gtk_tree_path_free(path);
  }
}

typedef struct _gtkTreeSelectMinMax
{
  Ihandle* ih;
  int id1, id2;
} gtkTreeSelectMinMax;

static gboolean gtkTreeSelected_Foreach_Func(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iterItem, gtkTreeSelectMinMax *minmax)
{
  int id = iupTreeFindNodeId(minmax->ih, iterItem->user_data);
  if (id < minmax->id1)
    minmax->id1 = id;
  if (id > minmax->id2)
    minmax->id2 = id;

  (void)model;
  (void)path;
  return FALSE; /* do not stop walking the store, call us with next row */
}

/*****************************************************************************/
/* CALLBACKS                                                                 */
/*****************************************************************************/
static void gtkTreeCallMultiSelectionCb(Ihandle* ih)
{
  IFnIi cbMulti = (IFnIi)IupGetCallback(ih, "MULTISELECTION_CB");
  IFnii cbSelec = (IFnii)IupGetCallback(ih, "SELECTION_CB");
  if (cbMulti || cbSelec)
  {
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));
    GtkTreeIter iterItem;
    int i = 0, countItems;
    gtkTreeSelectMinMax minmax;

    minmax.ih = ih;
    minmax.id1 = ih->data->node_count;
    minmax.id2 = -1;

    gtk_tree_selection_selected_foreach(selection, (GtkTreeSelectionForeachFunc)gtkTreeSelected_Foreach_Func, &minmax);
    if (minmax.id2 == -1)
      return;

    /* interactive selection of several nodes will NOT select hidden nodes,
       so make sure that they are selected. */
    for(i = minmax.id1; i <= minmax.id2; i++)
    {
      gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
      if (!gtkTreeIsNodeSelected(model, &iterItem))
        gtkTreeSelectNodeRaw(model, &iterItem, 1);
    }

    /* if last selected item is a branch, then select its children */
    iupTreeSelectLastCollapsedBranch(ih, &(minmax.id2));

    countItems = minmax.id2-minmax.id1+1;

    if (cbMulti)
    {
      int* id_rowItem = malloc(sizeof(int) * countItems);

      for(i = 0; i < countItems; i++)
        id_rowItem[i] = minmax.id1+i;

      cbMulti(ih, id_rowItem, countItems);

      free(id_rowItem);
    }
    else if (cbSelec)
    {
      for (i=0; i<countItems; i++)
        cbSelec(ih, minmax.id1+i, 1);
    }
  }
}


/*****************************************************************************/
/* GET AND SET ATTRIBUTES                                                    */
/*****************************************************************************/


static char* gtkTreeGetIndentationAttrib(Ihandle* ih)
{
  char* str = iupStrGetMemory(255);
#if GTK_CHECK_VERSION(2, 12, 0)
  int indent = gtk_tree_view_get_level_indentation(GTK_TREE_VIEW(ih->handle));
#else
  int indent = 0;
#endif
  sprintf(str, "%d", indent);
  return str;
}

static int gtkTreeSetIndentationAttrib(Ihandle* ih, const char* value)
{
#if GTK_CHECK_VERSION(2, 12, 0)
  int indent;
  if (iupStrToInt(value, &indent))
    gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(ih->handle), indent);
#endif
  return 0;
}

static int gtkTreeSetTopItemAttrib(Ihandle* ih, const char* value)
{
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GtkTreeIter iterItem;
  GtkTreePath* path;
  int kind;

  if (!gtkTreeFindNodeFromString(ih, value, &iterItem))
    return 0;

  path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iterItem);

  gtk_tree_model_get(GTK_TREE_MODEL(store), &iterItem, IUPGTK_TREE_KIND, &kind, -1);
  if (kind == ITREE_LEAF)
    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(ih->handle), path);
  else
  {
    int expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(ih->handle), path);
    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(ih->handle), path);
    if (!expanded) gtk_tree_view_collapse_row(GTK_TREE_VIEW(ih->handle), path);
  }

  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(ih->handle), path, NULL, FALSE, 0, 0);  /* scroll to visible */

  gtk_tree_path_free(path);
 
  return 0;
}

static int gtkTreeSetSpacingAttrib(Ihandle* ih, const char* value)
{
  if(!iupStrToInt(value, &ih->data->spacing))
    ih->data->spacing = 1;

  if(ih->data->spacing < 1)
    ih->data->spacing = 1;

  if (ih->handle)
  {
    GtkCellRenderer *renderer_img = (GtkCellRenderer*)iupAttribGet(ih, "_IUPGTK_RENDERER_IMG");
    GtkCellRenderer *renderer_txt = (GtkCellRenderer*)iupAttribGet(ih, "_IUPGTK_RENDERER_TEXT");
    g_object_set(G_OBJECT(renderer_img), "ypad", ih->data->spacing, NULL);
    g_object_set(G_OBJECT(renderer_txt), "ypad", ih->data->spacing, NULL);
    return 0;
  }
  else
    return 1; /* store until not mapped, when mapped will be set again */
}

static int gtkTreeSetExpandAllAttrib(Ihandle* ih, const char* value)
{
  if (iupStrBoolean(value))
    gtk_tree_view_expand_all(GTK_TREE_VIEW(ih->handle));
  else
    gtk_tree_view_collapse_all(GTK_TREE_VIEW(ih->handle));

  return 0;
}

static char* gtkTreeGetDepthAttrib(Ihandle* ih, const char* name_id)
{
  char* depth;
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;

  depth = iupStrGetMemory(10);
  sprintf(depth, "%d", gtk_tree_store_iter_depth(store, &iterItem));
  return depth;
}

static int gtkTreeSetMoveNodeAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  GtkTreeModel* model;
  GtkTreeIter iterItemSrc, iterItemDst, iterNewItem;
  GtkTreeIter iterParent, iterNextParent;

  if (!ih->handle)  /* do not do the action before map */
    return 0;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItemSrc))
    return 0;

  if (!gtkTreeFindNodeFromString(ih, value, &iterItemDst))
    return 0;

  /* If Drag item is an ancestor of Drop item then return */
  iterParent = iterItemDst;
  while(gtk_tree_model_iter_parent(model, &iterNextParent, &iterParent))
  {
    if (iterNextParent.user_data == iterItemSrc.user_data)
      return 0;
    iterParent = iterNextParent;
  }

  /* Move the node and its children to the new position */
  gtkTreeCopyMoveNode(ih, model, &iterItemSrc, &iterItemDst, &iterNewItem, 0);

  return 0;
}

static int gtkTreeSetCopyNodeAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  GtkTreeModel* model;
  GtkTreeIter iterItemSrc, iterItemDst, iterNewItem;
  GtkTreeIter iterParent, iterNextParent;

  if (!ih->handle)  /* do not do the action before map */
    return 0;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItemSrc))
    return 0;

  if (!gtkTreeFindNodeFromString(ih, value, &iterItemDst))
    return 0;

  /* If Drag item is an ancestor of Drop item then return */
  iterParent = iterItemDst;
  while(gtk_tree_model_iter_parent(model, &iterNextParent, &iterParent))
  {
    if (iterNextParent.user_data == iterItemSrc.user_data)
      return 0;
    iterParent = iterNextParent;
  }

  /* Copy the node and its children to the new position */
  gtkTreeCopyMoveNode(ih, model, &iterItemSrc, &iterItemDst, &iterNewItem, 1);

  return 0;
}

static char* gtkTreeGetColorAttrib(Ihandle* ih, const char* name_id)
{
  char* str;
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  GdkColor *color;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;

  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_COLOR, &color, -1);
  if (!color)
    return NULL;

  str = iupStrGetMemory(20);
  sprintf(str, "%d %d %d", iupCOLOR16TO8(color->red),
                           iupCOLOR16TO8(color->green),
                           iupCOLOR16TO8(color->blue));
  return str;
}

static int gtkTreeSetColorAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GtkTreeIter iterItem;
  GdkColor color;
  unsigned char r, g, b;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;

  if (!iupStrToRGB(value, &r, &g, &b))
    return 0;

  iupgdkColorSet(&color, r, g, b);
  gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_COLOR, &color, -1);

  return 0;
}

static char* gtkTreeGetParentAttrib(Ihandle* ih, const char* name_id)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  GtkTreeIter iterParent;
  char* str;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;

  if (!gtk_tree_model_iter_parent(model, &iterParent, &iterItem))
    return NULL;

  str = iupStrGetMemory(10);
  sprintf(str, "%d", gtkTreeFindNodeId(ih, &iterParent));
  return str;
}

static char* gtkTreeGetChildCountAttrib(Ihandle* ih, const char* name_id)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  char* str;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;

  str = iupStrGetMemory(10);
  sprintf(str, "%d", gtk_tree_model_iter_n_children(model, &iterItem));
  return str;
}

static char* gtkTreeGetKindAttrib(Ihandle* ih, const char* name_id)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  int kind;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;

  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_KIND, &kind, -1);

  if(!kind)
    return "BRANCH";
  else
    return "LEAF";
}

static char* gtkTreeGetStateAttrib(Ihandle* ih, const char* name_id)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;

  if (gtk_tree_model_iter_has_child(model, &iterItem))
  {
    GtkTreePath* path = gtk_tree_model_get_path(model, &iterItem);
    int expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(ih->handle), path);
    gtk_tree_path_free(path);

    if (expanded)
      return "EXPANDED";
    else
      return "COLLAPSED";
  }

  return NULL;
}

static int gtkTreeSetStateAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  GtkTreePath* path;
  int kind;

  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;

  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_KIND, &kind, -1);
  if (kind == ITREE_BRANCH)
  {
    path = gtk_tree_model_get_path(model, &iterItem);
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_BRANCH_CB", "1");
    gtkTreeExpandItem(ih, path, iupStrEqualNoCase(value, "EXPANDED"));
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_BRANCH_CB", NULL);
    gtk_tree_path_free(path);
  }

  return 0;
}

static char* gtkTreeGetTitle(GtkTreeModel* model, GtkTreeIter iterItem)
{
  char* title;
  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_TITLE, &title, -1);
  return iupgtkStrConvertFromUTF8(title);
}

static char* gtkTreeGetTitleAttrib(Ihandle* ih, const char* name_id)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;
  return gtkTreeGetTitle(model, iterItem);
}

static int gtkTreeSetTitleAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;
  if (!value)
    value = "";
  gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_TITLE, iupgtkStrConvertToUTF8(value), -1);
  return 0;
}

static int gtkTreeSetTitleFontAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  PangoFontDescription* fontdesc = NULL;
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;
  if (value)
    fontdesc = iupgtkGetPangoFontDesc(value);
  gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_FONT, fontdesc, -1);
  return 0;
}

static char* gtkTreeGetTitleFontAttrib(Ihandle* ih, const char* name_id)
{
  PangoFontDescription* fontdesc;
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return NULL;
  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_FONT, &fontdesc, -1);
  return pango_font_description_to_string(fontdesc);
}

static char* gtkTreeGetValueAttrib(Ihandle* ih)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreePath* path = NULL;
  char* str;

  gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &path, NULL);
  if (path)
  {
    GtkTreeIter iterItem;
    gtk_tree_model_get_iter(model, &iterItem, path);
    gtk_tree_path_free(path);

    str = iupStrGetMemory(16);
    sprintf(str, "%d", gtkTreeFindNodeId(ih, &iterItem));
    return str;
  }

  return "0"; /* default VALUE is root */
}

static char* gtkTreeGetMarkedNodesAttrib(Ihandle* ih)
{
  char* str = iupStrGetMemory(ih->data->node_count+1);
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  int i;

  for (i=0; i<ih->data->node_count; i++)
  {
    gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
    if (gtkTreeIsNodeSelected(model, &iterItem))
      str[i] = '+';
    else
      str[i] = '-';
  }

  str[ih->data->node_count] = 0;
  return str;
}

static int gtkTreeSetMarkedNodesAttrib(Ihandle* ih, const char* value)
{
  int count, i;
  GtkTreeModel* model;
  GtkTreeIter iterItem;
  GtkTreeSelection* selection;

  if (ih->data->mark_mode==ITREE_MARK_SINGLE || !value)
    return 0;

  count = strlen(value);
  if (count > ih->data->node_count)
    count = ih->data->node_count;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));

  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");

  for (i=0; i<count; i++)
  {
    gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
    if (value[i] == '+')
      gtkTreeSelectNode(model, selection, &iterItem, 1);
    else
      gtkTreeSelectNode(model, selection, &iterItem, 0);
  }

  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);

  return 0;
}

static int gtkTreeSetMarkAttrib(Ihandle* ih, const char* value)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));

  if (ih->data->mark_mode==ITREE_MARK_SINGLE)
    return 0;

  if(iupStrEqualNoCase(value, "BLOCK"))
  {
    GtkTreeIter iterItem1, iterItem2;
    GtkTreePath* pathFocus;
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &pathFocus, NULL);
    gtk_tree_model_get_iter(model, &iterItem1, pathFocus);
    gtk_tree_path_free(pathFocus);

    gtkTreeIterInit(ih, &iterItem2, iupAttribGet(ih, "_IUPTREE_MARKSTART_NODE"));

    gtkTreeSelectRange(ih, model, selection, &iterItem1, &iterItem2, 0);
  }
  else if(iupStrEqualNoCase(value, "CLEARALL"))
    gtkTreeSelectAll(ih, model, selection, 0);
  else if(iupStrEqualNoCase(value, "MARKALL"))
    gtkTreeSelectAll(ih, model, selection, 1);
  else if(iupStrEqualNoCase(value, "INVERTALL")) /* INVERTALL *MUST* appear before INVERT, or else INVERTALL will never be called. */
    gtkTreeInvertAllNodeMarking(ih, model, selection);
  else if(iupStrEqualPartial(value, "INVERT"))
  {
    /* iupStrEqualPartial allows the use of "INVERTid" form */
    GtkTreeIter iterItem;
    if (!gtkTreeFindNodeFromString(ih, &value[strlen("INVERT")], &iterItem))
      return 0;

    gtkTreeSelectNode(model, selection, &iterItem, -1);  /* toggle */
  }
  else
  {
    GtkTreeIter iterItem1, iterItem2;
    char str1[50], str2[50];
    if (iupStrToStrStr(value, str1, str2, '-')!=2)
      return 0;

    if (!gtkTreeFindNodeFromString(ih, str1, &iterItem1))
      return 0;
    if (!gtkTreeFindNodeFromString(ih, str2, &iterItem2))
      return 0;

    gtkTreeSelectRange(ih, model, selection, &iterItem1, &iterItem2, 0);
  }

  return 1;
} 

static int gtkTreeSetValueAttrib(Ihandle* ih, const char* value)
{
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));
  GtkTreeIter iterItem;
  GtkTreePath* path;
  int kind;

  if (gtkTreeSetMarkAttrib(ih, value))
    return 0;

  if (iupStrEqualNoCase(value, "ROOT"))
    gtk_tree_model_get_iter_first(model, &iterItem);
  else if(iupStrEqualNoCase(value, "LAST"))
    gtkTreeGetLastVisibleNode(ih, model, &iterItem);
  else if(iupStrEqualNoCase(value, "PGUP"))
  {
    GtkTreePath* pathFocus;
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &pathFocus, NULL);
    gtk_tree_model_get_iter(model, &iterItem, pathFocus);
    gtk_tree_path_free(pathFocus);

    gtkTreeGetPreviousVisibleNode(ih, model, &iterItem, 10);
  }
  else if(iupStrEqualNoCase(value, "PGDN"))
  {
    GtkTreePath* pathFocus;
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &pathFocus, NULL);
    gtk_tree_model_get_iter(model, &iterItem, pathFocus);
    gtk_tree_path_free(pathFocus);

    gtkTreeGetNextVisibleNode(ih, model, &iterItem, 10);
  }
  else if(iupStrEqualNoCase(value, "NEXT"))
  {
    GtkTreePath* pathFocus;
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &pathFocus, NULL);
    gtk_tree_model_get_iter(model, &iterItem, pathFocus);
    gtk_tree_path_free(pathFocus);

    gtkTreeGetNextVisibleNode(ih, model, &iterItem, 1);
  }
  else if(iupStrEqualNoCase(value, "PREVIOUS"))
  {
    GtkTreePath* pathFocus;
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &pathFocus, NULL);
    gtk_tree_model_get_iter(model, &iterItem, pathFocus);
    gtk_tree_path_free(pathFocus);

    gtkTreeGetPreviousVisibleNode(ih, model, &iterItem, 1);
  }
  else
  {
    if (!gtkTreeFindNodeFromString(ih, value, &iterItem))
      return 0;
  }

  /* select */
  if (ih->data->mark_mode==ITREE_MARK_SINGLE)
  {
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");
    gtkTreeSelectNode(model, selection, &iterItem, 1);
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);
  }

  path = gtk_tree_model_get_path(model, &iterItem);

  /* make it visible */
  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_KIND, &kind, -1);
  if (kind == ITREE_LEAF)
    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(ih->handle), path);
  else
  {
    int expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(ih->handle), path);
    gtk_tree_view_expand_to_path(GTK_TREE_VIEW(ih->handle), path);
    if (!expanded) gtk_tree_view_collapse_row(GTK_TREE_VIEW(ih->handle), path);
  }

  gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(ih->handle), path, NULL, FALSE, 0, 0); /* scroll to visible */
  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");
  gtk_tree_view_set_cursor(GTK_TREE_VIEW(ih->handle), path, NULL, FALSE);  /* set focus */
  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);

  gtk_tree_path_free(path);

  iupAttribSetInt(ih, "_IUPTREE_OLDVALUE", gtkTreeFindNodeId(ih, &iterItem));

  return 0;
} 

static int gtkTreeSetMarkStartAttrib(Ihandle* ih, const char* name_id)
{
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;

  iupAttribSetStr(ih, "_IUPTREE_MARKSTART_NODE", (char*)iterItem.user_data);

  return 1;
}

static char* gtkTreeGetMarkedAttrib(Ihandle* ih, const char* name_id)
{
  GtkTreeModel* model;
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  if (gtkTreeIsNodeSelected(model, &iterItem))
    return "YES";
  else
    return "NO";
}

static int gtkTreeSetMarkedAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  GtkTreeModel* model;
  GtkTreeSelection* selection;
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;

  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  if (iupStrBoolean(value))
    gtkTreeSelectNode(model, selection, &iterItem, 1);
  else
    gtkTreeSelectNode(model, selection, &iterItem, 0);

  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);

  return 0;
}

static int gtkTreeSetDelNodeAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  if (!ih->handle)  /* do not do the action before map */
    return 0;
  if (iupStrEqualNoCase(value, "SELECTED"))  /* selected here means the reference node */
  {
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
    GtkTreeIter iterItem;
    GtkTreeIter iterParent;

    if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
      return 0;

    if (!gtk_tree_model_iter_parent(model, &iterParent, &iterItem)) /* the root node can't be deleted */
      return 0;

    gtkTreeCallNodeRemoved(ih, model, &iterItem);

    /* deleting the reference node (and it's children) */
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");
    gtk_tree_store_remove(GTK_TREE_STORE(model), &iterItem);
    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);
  }
  else if(iupStrEqualNoCase(value, "CHILDREN"))  /* children of the reference node */
  {
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
    GtkTreeIter iterItem, iterChild;
    int hasChildren;

    if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
      return 0;

    hasChildren = gtk_tree_model_iter_children(model, &iterChild, &iterItem);

    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");

    /* deleting the reference node children */
    while(hasChildren)
    {
      gtkTreeCallNodeRemoved(ih, model, &iterChild);
      hasChildren = gtk_tree_store_remove(GTK_TREE_STORE(model), &iterChild);
    }

    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);
  }
  else if(iupStrEqualNoCase(value, "MARKED"))  /* Delete the array of marked nodes */
  {
    int i;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
    GtkTreeIter iterItem;

    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");

    for(i = 1; i < ih->data->node_count; /* increment only if not removed */)
    {
      gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
      if (gtkTreeIsNodeSelected(model, &iterItem))
      {
        gtkTreeCallNodeRemoved(ih, model, &iterItem);
        gtk_tree_store_remove(GTK_TREE_STORE(model), &iterItem);
      }
      else
        i++;
    }

    iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);
  }

  return 0;
}

static int gtkTreeSetRenameAttrib(Ihandle* ih, const char* value)
{  
  if (ih->data->show_rename)
  {
    GtkTreePath* path;
    GtkTreeViewColumn *focus_column;
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &path, &focus_column);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(ih->handle), path, focus_column, TRUE);
    gtk_tree_path_free(path);
  }

  (void)value;
  return 0;
}

static int gtkTreeSetImageExpandedAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  int kind;
  GtkTreeStore*  store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GdkPixbuf* pixExpand = iupImageGetImage(value, ih, 0);
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;

  gtk_tree_model_get(GTK_TREE_MODEL(store), &iterItem, IUPGTK_TREE_KIND, &kind, -1);

  if (kind == ITREE_BRANCH)
  {
    if (pixExpand)
      gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_IMAGE_EXPANDED, pixExpand, 
                                           IUPGTK_TREE_HAS_IMAGE_EXPANDED, TRUE, -1);
    else
      gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_IMAGE_EXPANDED, ih->data->def_image_expanded, 
                                           IUPGTK_TREE_HAS_IMAGE_EXPANDED, FALSE, -1);
  }

  return 1;
}

static int gtkTreeSetImageAttrib(Ihandle* ih, const char* name_id, const char* value)
{
  GtkTreeStore* store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle)));
  GdkPixbuf* pixImage = iupImageGetImage(value, ih, 0);
  GtkTreeIter iterItem;
  if (!gtkTreeFindNodeFromString(ih, name_id, &iterItem))
    return 0;

  if (pixImage)
  {
    gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_IMAGE, pixImage, 
                                         IUPGTK_TREE_HAS_IMAGE, TRUE, -1);
  }
  else
  {
    int kind;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iterItem, IUPGTK_TREE_KIND, &kind, -1);
    if (kind == ITREE_BRANCH)
      gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_IMAGE, ih->data->def_image_collapsed, 
                                           IUPGTK_TREE_HAS_IMAGE, FALSE, -1);
    else
      gtk_tree_store_set(store, &iterItem, IUPGTK_TREE_IMAGE, ih->data->def_image_leaf, 
                                           IUPGTK_TREE_HAS_IMAGE, FALSE, -1);
  }

  return 1;
}

static int gtkTreeSetImageBranchExpandedAttrib(Ihandle* ih, const char* value)
{
  GtkTreeIter iterRoot;
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  ih->data->def_image_expanded = iupImageGetImage(value, ih, 0);

  gtk_tree_model_get_iter_first(model, &iterRoot);

  /* Update all images, starting at root node */
  gtkTreeUpdateImages(ih, model, iterRoot, ITREE_UPDATEIMAGE_EXPANDED);

  return 1;
}

static int gtkTreeSetImageBranchCollapsedAttrib(Ihandle* ih, const char* value)
{
  GtkTreeIter iterRoot;
  GtkTreeModel*  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  ih->data->def_image_collapsed = iupImageGetImage(value, ih, 0);

  gtk_tree_model_get_iter_first(model, &iterRoot);

  /* Update all images, starting at root node */
  gtkTreeUpdateImages(ih, model, iterRoot, ITREE_UPDATEIMAGE_COLLAPSED);

  return 1;
}

static int gtkTreeSetImageLeafAttrib(Ihandle* ih, const char* value)
{
  GtkTreeIter iterRoot;
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  ih->data->def_image_leaf = iupImageGetImage(value, ih, 0);

  gtk_tree_model_get_iter_first(model, &iterRoot);

  /* Update all images, starting at root node */
  gtkTreeUpdateImages(ih, model, iterRoot, ITREE_UPDATEIMAGE_LEAF);

  return 1;
}

static int gtkTreeSetBgColorAttrib(Ihandle* ih, const char* value)
{
  unsigned char r, g, b;

  GtkScrolledWindow* scrolled_window = (GtkScrolledWindow*)iupAttribGet(ih, "_IUP_EXTRAPARENT");
  if (scrolled_window)
  {
    /* ignore given value, must use only from parent for the scrollbars */
    char* parent_value = iupBaseNativeParentGetBgColor(ih);

    if (iupStrToRGB(parent_value, &r, &g, &b))
    {
      GtkWidget* sb;

      if (!GTK_IS_SCROLLED_WINDOW(scrolled_window))
        scrolled_window = (GtkScrolledWindow*)iupAttribGet(ih, "_IUPGTK_SCROLLED_WINDOW");

      iupgtkBaseSetBgColor((GtkWidget*)scrolled_window, r, g, b);

#if GTK_CHECK_VERSION(2, 8, 0)
      sb = gtk_scrolled_window_get_hscrollbar(scrolled_window);
      if (sb) iupgtkBaseSetBgColor(sb, r, g, b);

      sb = gtk_scrolled_window_get_vscrollbar(scrolled_window);
      if (sb) iupgtkBaseSetBgColor(sb, r, g, b);
#endif
    }
  }

  if (!iupStrToRGB(value, &r, &g, &b))
    return 0;

  {
    GtkCellRenderer* renderer_txt = (GtkCellRenderer*)iupAttribGet(ih, "_IUPGTK_RENDERER_TEXT");
    GtkCellRenderer* renderer_img = (GtkCellRenderer*)iupAttribGet(ih, "_IUPGTK_RENDERER_IMG");
    GdkColor color;
    iupgdkColorSet(&color, r, g, b);
    g_object_set(G_OBJECT(renderer_txt), "cell-background-gdk", &color, NULL);
    g_object_set(G_OBJECT(renderer_img), "cell-background-gdk", &color, NULL);
  }

  iupdrvBaseSetBgColorAttrib(ih, value);   /* use given value for contents */

  /* no need to update internal image cache in GTK */

  return 1;
}

static int gtkTreeSetFgColorAttrib(Ihandle* ih, const char* value)
{
  unsigned char r, g, b;
  if (!iupStrToRGB(value, &r, &g, &b))
    return 0;

  iupgtkBaseSetFgColor(ih->handle, r, g, b);

  {
    GtkCellRenderer* renderer_txt = (GtkCellRenderer*)iupAttribGet(ih, "_IUPGTK_RENDERER_TEXT");
    GdkColor color;
    iupgdkColorSet(&color, r, g, b);
    g_object_set(G_OBJECT(renderer_txt), "foreground-gdk", &color, NULL);
    g_object_get(G_OBJECT(renderer_txt), "foreground-gdk", &color, NULL);
    color.blue = 0;
  }

  return 1;
}

void iupdrvTreeUpdateMarkMode(Ihandle *ih)
{
  GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));
  gtk_tree_selection_set_mode(selection, (ih->data->mark_mode==ITREE_MARK_SINGLE)? GTK_SELECTION_SINGLE: GTK_SELECTION_MULTIPLE);

  if (ih->data->mark_mode==ITREE_MARK_MULTIPLE && !ih->data->show_dragdrop)
  {
#if GTK_CHECK_VERSION(2, 10, 0)
      gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(ih->handle), TRUE);
#endif
  }
}


/***********************************************************************************************/


static void gtkTreeSetRenameCaretPos(GtkCellEditable *editable, const char* value)
{
  int pos = 1;

  if (iupStrToInt(value, &pos))
  {
    if (pos < 1) pos = 1;
    pos--; /* IUP starts at 1 */

    gtk_editable_set_position(GTK_EDITABLE(editable), pos);
  }
}

static void gtkTreeSetRenameSelectionPos(GtkCellEditable *editable, const char* value)
{
  int start = 1, end = 1;

  if (iupStrToIntInt(value, &start, &end, ':') != 2) 
    return;

  if(start < 1 || end < 1) 
    return;

  start--; /* IUP starts at 1 */
  end--;

  gtk_editable_select_region(GTK_EDITABLE(editable), start, end);
}

/*****************************************************************************/
/* SIGNALS                                                                   */
/*****************************************************************************/

static void gtkTreeCellTextEditingStarted(GtkCellRenderer *cell, GtkCellEditable *editable, const gchar *path_string, Ihandle *ih)
{
  char* value;
  GtkTreeIter iterItem;
  PangoFontDescription* fontdesc = NULL;
  GdkColor *color = NULL;
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  IFni cbShowRename;

  gtk_tree_model_get_iter_from_string(model, &iterItem, path_string);

  cbShowRename = (IFni)IupGetCallback(ih, "SHOWRENAME_CB");
  if (cbShowRename && cbShowRename(ih, gtkTreeFindNodeId(ih, &iterItem))==IUP_IGNORE)
  {
    /* TODO: non of these worked:
    gtk_cell_renderer_stop_editing(cell, TRUE);
    gtk_cell_editable_editing_done(editable);  */
    gtk_editable_set_editable(GTK_EDITABLE(editable), FALSE);
    return;
  }

  value = iupAttribGetStr(ih, "RENAMECARET");
  if (value)
    gtkTreeSetRenameCaretPos(editable, value);

  value = iupAttribGetStr(ih, "RENAMESELECTION");
  if (value)
    gtkTreeSetRenameSelectionPos(editable, value);

  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_FONT, &fontdesc, -1);
  if (fontdesc)
    gtk_widget_modify_font(GTK_WIDGET(editable), fontdesc);

  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_COLOR, &color, -1);
  if (color)
    iupgtkBaseSetFgGdkColor(GTK_WIDGET(editable), color);

  (void)cell;
}

static void gtkTreeCellTextEdited(GtkCellRendererText *cell, gchar *path_string, gchar *new_text, Ihandle* ih)
{
  GtkTreeModel* model;
  GtkTreeIter iterItem;
  IFnis cbRename;

  if (!new_text)
    new_text = "";

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  if (!gtk_tree_model_get_iter_from_string(model, &iterItem, path_string))
    return;

  cbRename = (IFnis)IupGetCallback(ih, "RENAME_CB");
  if (cbRename)
  {
    if (cbRename(ih, gtkTreeFindNodeId(ih, &iterItem), iupgtkStrConvertFromUTF8(new_text)) == IUP_IGNORE)
      return;
  }

  /* It is the responsibility of the application to update the model and store new_text at the position indicated by path. */
  gtk_tree_store_set(GTK_TREE_STORE(model), &iterItem, IUPGTK_TREE_TITLE, new_text, -1);

  (void)cell;
}

static int gtkTreeCallDragDropCb(Ihandle* ih, GtkTreeIter *iterDrag, GtkTreeIter *iterDrop, int *is_ctrl)
{
  IFniiii cbDragDrop = (IFniiii)IupGetCallback(ih, "DRAGDROP_CB");
  int is_shift = 0;
  char key[5];
  iupdrvGetKeyState(key);
  if (key[0] == 'S')
    is_shift = 1;
  if (key[1] == 'C')
    *is_ctrl = 1;
  else
    *is_ctrl = 0;

  if (cbDragDrop)
  {
    int drag_id = gtkTreeFindNodeId(ih, iterDrag);
    int drop_id = gtkTreeFindNodeId(ih, iterDrop);
    return cbDragDrop(ih, drag_id, drop_id, is_shift, *is_ctrl);
  }

  return IUP_CONTINUE; /* allow to move by default if callback not defined */
}

static void gtkTreeDragDataReceived(GtkWidget *widget, GdkDragContext *context, gint x, gint y, 
                                    GtkSelectionData *selection_data, guint info, guint time, Ihandle* ih)
{
  GtkTreePath* pathDrag = (GtkTreePath*)iupAttribGet(ih, "_IUPTREE_DRAGITEM");
  GtkTreePath* pathDrop = (GtkTreePath*)iupAttribGet(ih, "_IUPTREE_DROPITEM");
  int accepted = FALSE;
  int is_ctrl;

  if (pathDrag && pathDrop)
  {
    GtkTreeIter iterDrag, iterDrop, iterParent, iterNextParent;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));

    gtk_tree_model_get_iter(model, &iterDrag, pathDrag);
    gtk_tree_model_get_iter(model, &iterDrop, pathDrop);

    iterParent = iterDrop;
    while(gtk_tree_model_iter_parent(model, &iterNextParent, &iterParent))
    {
      if (iterNextParent.user_data == iterDrag.user_data)
        goto gtkTreeDragDataReceived_FINISH;
      iterParent = iterNextParent;
    }

    accepted = TRUE;

    if (gtkTreeCallDragDropCb(ih, &iterDrag, &iterDrop, &is_ctrl) == IUP_CONTINUE)
    {
      GtkTreeIter iterNewItem;

      /* Copy or move the dragged item to the new position. */
      gtkTreeCopyMoveNode(ih, model, &iterDrag, &iterDrop, &iterNewItem, is_ctrl);

      /* set focus and selection */
      if (iterNewItem.stamp)
      {
        GtkTreePath *pathNew;
        GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));

        pathNew = gtk_tree_model_get_path(model, &iterNewItem);
        gtkTreeSelectNode(model, selection, &iterNewItem, 1);

        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(ih->handle), pathNew, NULL, FALSE, 0, 0);
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(ih->handle), pathNew, NULL, FALSE);

        gtk_tree_path_free(pathNew);
      }
    }
  }

gtkTreeDragDataReceived_FINISH:
  if (pathDrag) gtk_tree_path_free(pathDrag);
  if (pathDrop) gtk_tree_path_free(pathDrop);

  iupAttribSetStr(ih, "_IUPTREE_DRAGITEM", NULL);
  iupAttribSetStr(ih, "_IUPTREE_DROPITEM", NULL);

  gtk_drag_finish(context, accepted, (context->action == GDK_ACTION_MOVE), time);

  (void)widget;
  (void)info;
  (void)x;
  (void)y;
  (void)selection_data;
}

static gboolean gtkTreeDragDrop(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, Ihandle* ih)
{
  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  GdkAtom target = GDK_NONE;

  /* unset any highlight row */
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW(widget), NULL, GTK_TREE_VIEW_DROP_BEFORE);

  if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(ih->handle), x, y, &path, &pos))
  {
    if ((pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE || pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER) && 
        (gtk_tree_path_compare(path, (GtkTreePath*)iupAttribGet(ih, "_IUPTREE_DRAGITEM")) != 0))
    {
      target = gtk_drag_dest_find_target(widget, context, gtk_drag_dest_get_target_list(widget));
      if (target != GDK_NONE)
      {
        iupAttribSetStr(ih, "_IUPTREE_DROPITEM", (char*)path);
        gtk_drag_get_data(widget, context, target, time);
        return TRUE;
      }
    }
  }

  (void)widget;
  return FALSE;
}

static void gtkTreeDragLeave(GtkWidget *widget, GdkDragContext *context, guint time)
{
  /* unset any highlight row */
  gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), NULL, GTK_TREE_VIEW_DROP_BEFORE);
  (void)context;
  (void)time;
}

static gboolean gtkTreeDragMotion(GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, Ihandle* ih)
{
  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  GtkTreePath* pathDrag = (GtkTreePath*)iupAttribGet(ih, "_IUPTREE_DRAGITEM");
  if (pathDrag && gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(ih->handle), x, y, &path, &pos))
  {
    if ((pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE || pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER) && 
        (gtk_tree_path_compare(path, pathDrag) != 0))
    {
      gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), path, pos);
      gdk_drag_status(context, context->actions, time);
      return TRUE;
    }

    gtk_tree_path_free(path);
  }

  (void)widget;
  return FALSE;
}

static void gtkTreeDragBegin(GtkWidget *widget, GdkDragContext *context, Ihandle* ih)
{
  int x = iupAttribGetInt(ih, "_IUPTREE_DRAG_X");
  int y = iupAttribGetInt(ih, "_IUPTREE_DRAG_Y");
  GtkTreePath* path;
  GtkTreeViewDropPosition pos;
  if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(ih->handle), x, y, &path, &pos))
  {
    if ((pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE || pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER))
    {
      GdkPixmap* pixmap;
      iupAttribSetStr(ih, "_IUPTREE_DRAGITEM", (char*)path);

      pixmap = gtk_tree_view_create_row_drag_icon(GTK_TREE_VIEW(ih->handle), path);
      gtk_drag_source_set_icon(widget, gtk_widget_get_colormap(widget), pixmap, NULL);
      g_object_unref(pixmap);
      return;
    }
  }

  (void)context;
  (void)widget;
}

static gboolean gtkTreeSelectionFunc(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean selected, Ihandle* ih)
{
  GtkTreeIter iterItem;
  gtk_tree_model_get_iter(model, &iterItem, path);
  gtkTreeSelectNodeRaw(model, &iterItem, !selected);
  (void)ih;
  (void)selection;
  return TRUE;
}

static void gtkTreeSelectionChanged(GtkTreeSelection* selection, Ihandle* ih)
{
  IFnii cbSelec;
  int is_ctrl = 0;
  (void)selection;

  if (iupAttribGet(ih, "_IUPTREE_IGNORE_SELECTION_CB"))
    return;

  if (ih->data->mark_mode == ITREE_MARK_MULTIPLE)
  {
    char key[5];
    iupdrvGetKeyState(key);
    if (key[0] == 'S')
      return;
    else if (key[1] == 'C')
      is_ctrl = 1;

    if (iupAttribGetInt(ih, "_IUPTREE_EXTENDSELECT")==2 && !is_ctrl)
    {
      iupAttribSetStr(ih, "_IUPTREE_EXTENDSELECT", NULL);
      gtkTreeCallMultiSelectionCb(ih);
      return;
    }
  }

  cbSelec = (IFnii)IupGetCallback(ih, "SELECTION_CB");
  if (cbSelec)
  {
    int curpos = -1, is_selected = 0;
    GtkTreeIter iterFocus;
    GtkTreePath* pathFocus;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));

    gtk_tree_view_get_cursor(GTK_TREE_VIEW(ih->handle), &pathFocus, NULL);
    if (pathFocus)
    {
      gtk_tree_model_get_iter(model, &iterFocus, pathFocus);
      gtk_tree_path_free(pathFocus);
      curpos = gtkTreeFindNodeId(ih, &iterFocus);
      is_selected = gtkTreeIsNodeSelected(model, &iterFocus);
    }

    if (curpos == -1)
      return;

    if (is_ctrl) 
      cbSelec(ih, curpos, is_selected);
    else
    {
      int oldpos = iupAttribGetInt(ih, "_IUPTREE_OLDVALUE");
      if(oldpos != curpos)
      {
        cbSelec(ih, oldpos, 0);  /* unselected */
        cbSelec(ih, curpos, 1);  /*   selected */

        iupAttribSetInt(ih, "_IUPTREE_OLDVALUE", curpos);
      }
    }
  }
}

static void gtkTreeUpdateSelectionChildren(Ihandle* ih, GtkTreeModel* model, GtkTreeSelection* selection, GtkTreeIter *iterItem)
{
  int expanded;
  GtkTreeIter iterChild;
  int hasItem = gtk_tree_model_iter_children(model, &iterChild, iterItem);  /* get the firstchild */
  while(hasItem)
  {
    if (gtkTreeIsNodeSelected(model, &iterChild))
      gtk_tree_selection_select_iter(selection, &iterChild);

    expanded = 0;
    if (gtk_tree_model_iter_has_child(model, &iterChild))
    {
      GtkTreePath* path = gtk_tree_model_get_path(model, &iterChild);
      expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(ih->handle), path);
      gtk_tree_path_free(path);
    }

    /* Recursive only if expanded */
    if (expanded)
      gtkTreeUpdateSelectionChildren(ih, model, selection, &iterChild);  

    /* Go to next sibling item */
    hasItem = gtk_tree_model_iter_next(model, &iterChild);
  }
}

static void gtkTreeRowExpanded(GtkTreeView* tree_view, GtkTreeIter *iterItem, GtkTreePath *path, Ihandle* ih)
{
  GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);
  GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", "1");
  gtkTreeUpdateSelectionChildren(ih, model, selection, iterItem);
  iupAttribSetStr(ih, "_IUPTREE_IGNORE_SELECTION_CB", NULL);
  (void)path;
}

static gboolean gtkTreeTestExpandRow(GtkTreeView* tree_view, GtkTreeIter *iterItem, GtkTreePath *path, Ihandle* ih)
{
  IFni cbBranchOpen = (IFni)IupGetCallback(ih, "BRANCHOPEN_CB");
  if (cbBranchOpen)
  {
    if (iupAttribGet(ih, "_IUPTREE_IGNORE_BRANCH_CB"))
      return FALSE;

    if (cbBranchOpen(ih, gtkTreeFindNodeId(ih, iterItem)) == IUP_IGNORE)
      return TRUE;  /* prevent the change */
  }

  (void)path;
  (void)tree_view;
  return FALSE;
}

static gboolean gtkTreeTestCollapseRow(GtkTreeView* tree_view, GtkTreeIter *iterItem, GtkTreePath *path, Ihandle* ih)
{
  IFni cbBranchClose = (IFni)IupGetCallback(ih, "BRANCHCLOSE_CB");
  if (cbBranchClose)
  {
    if (iupAttribGet(ih, "_IUPTREE_IGNORE_BRANCH_CB"))
      return FALSE;

    if (cbBranchClose(ih, gtkTreeFindNodeId(ih, iterItem)) == IUP_IGNORE)
      return TRUE;
  }

  (void)path;
  (void)tree_view;
  return FALSE;
}

static void gtkTreeRowActived(GtkTreeView* tree_view, GtkTreePath *path, GtkTreeViewColumn *column, Ihandle* ih)
{
  GtkTreeIter iterItem;
  GtkTreeModel* model;
  int kind;  /* used for nodes defined as branches, but do not have children */
  IFni cbExecuteLeaf  = (IFni)IupGetCallback(ih, "EXECUTELEAF_CB");
  if (!cbExecuteLeaf)
    return;

  model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
  gtk_tree_model_get_iter(model, &iterItem, path);
  gtk_tree_model_get(model, &iterItem, IUPGTK_TREE_KIND, &kind, -1);

  /* just to leaf nodes */
  if(gtk_tree_model_iter_has_child(model, &iterItem) == 0 && kind == ITREE_LEAF)
    cbExecuteLeaf(ih, gtkTreeFindNodeId(ih, &iterItem));

  (void)column;
  (void)tree_view;
}

static int gtkTreeConvertXYToPos(Ihandle* ih, int x, int y)
{
  GtkTreePath* path;
  if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(ih->handle), x, y, &path, NULL))
  {
    GtkTreeIter iterItem;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
    gtk_tree_model_get_iter(model, &iterItem, path);
    gtk_tree_path_free (path);
    return gtkTreeFindNodeId(ih, &iterItem);
  }
  return -1;
}

static Iarray* gtkTreeGetSelectedArrayId(Ihandle* ih)
{
  Iarray* selarray = iupArrayCreate(1, sizeof(int));
  int i;
  GtkTreeIter iterItem;
  GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));

  for (i = 0; i < ih->data->node_count; i++)
  {
    gtkTreeIterInit(ih, &iterItem, ih->data->node_cache[i].node_handle);
    if (gtkTreeIsNodeSelected(model, &iterItem))
    {
      int* id_hitem = (int*)iupArrayInc(selarray);
      int j = iupArrayCount(selarray);
      id_hitem[j-1] = i;
    }
  }

  return selarray;
}

static void gtkTreeCallMultiUnSelectionCb(Ihandle* ih)
{
  IFnIi cbMulti = (IFnIi)IupGetCallback(ih, "MULTIUNSELECTION_CB");
  IFnii cbSelec = (IFnii)IupGetCallback(ih, "SELECTION_CB");
  if (cbSelec || cbMulti)
  {
    Iarray* markedArray = gtkTreeGetSelectedArrayId(ih);
    int* id_hitem = (int*)iupArrayGetData(markedArray);
    int i, count = iupArrayCount(markedArray);

    if (count > 1)
    {
      if (cbMulti)
        cbMulti(ih, id_hitem, iupArrayCount(markedArray));
      else
      {
        for (i=0; i<count; i++)
          cbSelec(ih, id_hitem[i], 0);
      }
    }

    iupArrayDestroy(markedArray);
  }
}

static gboolean gtkTreeButtonEvent(GtkWidget *treeview, GdkEventButton *evt, Ihandle* ih)
{
  if (iupgtkButtonEvent(treeview, evt, ih) == TRUE)
    return TRUE;

  if (evt->type == GDK_BUTTON_PRESS && evt->button == 3)  /* right single click */
  {
    IFni cbRightClick  = (IFni)IupGetCallback(ih, "RIGHTCLICK_CB");
    if (cbRightClick)
    {
      int id = gtkTreeConvertXYToPos(ih, (int)evt->x, (int)evt->y);
      if (id != -1)
        cbRightClick(ih, id);
      return TRUE;
    }
  }
  else if (evt->type == GDK_2BUTTON_PRESS && evt->button == 1)  /* left double click */
  {
    GtkTreePath *path;

    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), (gint) evt->x, (gint) evt->y, &path, NULL, NULL, NULL))
    {
      GtkTreeIter iter;
      GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ih->handle));
      int kind;  /* used for nodes defined as branches, but do not have children */

      gtk_tree_model_get_iter(model, &iter, path);
      gtk_tree_model_get(model, &iter, IUPGTK_TREE_KIND, &kind, -1);

      if (kind == ITREE_BRANCH)
        gtkTreeExpandItem(ih, path, -1); /* toggle */

      gtk_tree_path_free(path);
    }
  }
  else if (evt->type == GDK_BUTTON_PRESS && evt->button == 1)  /* left single press */
  {
    iupAttribSetInt(ih, "_IUPTREE_DRAG_X", (int)evt->x);
    iupAttribSetInt(ih, "_IUPTREE_DRAG_Y", (int)evt->y);

    if (ih->data->mark_mode==ITREE_MARK_MULTIPLE && 
        !(evt->state & GDK_SHIFT_MASK) && !(evt->state & GDK_CONTROL_MASK))
    {
      gtkTreeCallMultiUnSelectionCb(ih);
      iupAttribSetStr(ih, "_IUPTREE_EXTENDSELECT", "1");
    }
  }
  else if (evt->type == GDK_BUTTON_RELEASE && evt->button == 1)  /* left single release */
  {
    if (ih->data->mark_mode==ITREE_MARK_MULTIPLE && (evt->state & GDK_SHIFT_MASK))
      gtkTreeCallMultiSelectionCb(ih); /* Multi Selection Callback */

    if (ih->data->mark_mode==ITREE_MARK_MULTIPLE && 
        !(evt->state & GDK_SHIFT_MASK) && !(evt->state & GDK_CONTROL_MASK))
    {
      if (iupAttribGet(ih, "_IUPTREE_EXTENDSELECT"))
        iupAttribSetStr(ih, "_IUPTREE_EXTENDSELECT", "2");
    }
  }
  
  return FALSE;
}

static gboolean gtkTreeKeyReleaseEvent(GtkWidget *widget, GdkEventKey *evt, Ihandle *ih)
{
  if (ih->data->mark_mode==ITREE_MARK_MULTIPLE && (evt->state & GDK_SHIFT_MASK))
  {
    if (evt->keyval == GDK_Up || evt->keyval == GDK_Down || evt->keyval == GDK_Home || evt->keyval == GDK_End)
      gtkTreeCallMultiSelectionCb(ih); /* Multi Selection Callback */
  }

  (void)widget;
  return TRUE;
}

static gboolean gtkTreeKeyPressEvent(GtkWidget *widget, GdkEventKey *evt, Ihandle *ih)
{
  if (iupgtkKeyPressEvent(widget, evt, ih) == TRUE)
    return TRUE;

  if (evt->keyval == GDK_F2)
  {
    gtkTreeSetRenameAttrib(ih, NULL);
    return TRUE;
  }
  else if (evt->keyval == GDK_Return || evt->keyval == GDK_KP_Enter)
  {
    gtkTreeOpenCloseEvent(ih);
    return TRUE;
  }

  return FALSE;
}

static void gtkTreeEnableDragDrop(Ihandle* ih)
{
  const GtkTargetEntry row_targets[] = {
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 }
  };

  gtk_drag_source_set(ih->handle, GDK_BUTTON1_MASK, row_targets, G_N_ELEMENTS(row_targets), GDK_ACTION_MOVE|GDK_ACTION_COPY);
  gtk_drag_dest_set(ih->handle, GDK_BUTTON1_MASK, row_targets, G_N_ELEMENTS(row_targets), GDK_ACTION_MOVE|GDK_ACTION_COPY);

  g_signal_connect(G_OBJECT(ih->handle),  "drag-begin", G_CALLBACK(gtkTreeDragBegin), ih);
  g_signal_connect(G_OBJECT(ih->handle), "drag-motion", G_CALLBACK(gtkTreeDragMotion), ih);
  g_signal_connect(G_OBJECT(ih->handle), "drag-leave", G_CALLBACK(gtkTreeDragLeave), NULL);
  g_signal_connect(G_OBJECT(ih->handle),   "drag-drop", G_CALLBACK(gtkTreeDragDrop), ih);
  g_signal_connect(G_OBJECT(ih->handle), "drag-data-received", G_CALLBACK(gtkTreeDragDataReceived), ih);
}

/*****************************************************************************/

static int gtkTreeMapMethod(Ihandle* ih)
{
  GtkScrolledWindow* scrolled_window = NULL;
  GtkTreeStore *store;
  GtkCellRenderer *renderer_img, *renderer_txt;
  GtkTreeSelection* selection;
  GtkTreeViewColumn *column;

  store = gtk_tree_store_new(IUPGTK_TREE_LAST_DATA, 
    GDK_TYPE_PIXBUF,                 /* IUPGTK_TREE_IMAGE */
    G_TYPE_BOOLEAN,                  /* IUPGTK_TREE_HAS_IMAGE */
    GDK_TYPE_PIXBUF,                 /* IUPGTK_TREE_IMAGE_EXPANDED */
    G_TYPE_BOOLEAN,                  /* IUPGTK_TREE_HAS_IMAGE_EXPANDED */
    G_TYPE_STRING,                   /* IUPGTK_TREE_TITLE */
    G_TYPE_INT,                      /* IUPGTK_TREE_KIND */
    GDK_TYPE_COLOR,                  /* IUPGTK_TREE_COLOR */
    PANGO_TYPE_FONT_DESCRIPTION,     /* IUPGTK_TREE_FONT */
    G_TYPE_BOOLEAN);                 /* IUPGTK_TREE_SELECT */

  ih->handle = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

  g_object_unref(store);

  if (!ih->handle)
    return IUP_ERROR;

  scrolled_window = (GtkScrolledWindow*)gtk_scrolled_window_new(NULL, NULL);
  iupAttribSetStr(ih, "_IUP_EXTRAPARENT", (char*)scrolled_window);

  /* Column and renderers */
  column = gtk_tree_view_column_new();
  iupAttribSetStr(ih, "_IUPGTK_COLUMN",   (char*)column);

  renderer_img = gtk_cell_renderer_pixbuf_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer_img, FALSE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(column), renderer_img, "pixbuf", IUPGTK_TREE_IMAGE,
                                                              "pixbuf-expander-open", IUPGTK_TREE_IMAGE_EXPANDED,
                                                            "pixbuf-expander-closed", IUPGTK_TREE_IMAGE, 
                                                            NULL);
  iupAttribSetStr(ih, "_IUPGTK_RENDERER_IMG", (char*)renderer_img);

  renderer_txt = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer_txt, TRUE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(column), renderer_txt, "text", IUPGTK_TREE_TITLE,
                                                                     "is-expander", IUPGTK_TREE_KIND,
                                                                     "font-desc", IUPGTK_TREE_FONT,
                                                                  "foreground-gdk", IUPGTK_TREE_COLOR, 
                                                                  NULL);
  iupAttribSetStr(ih, "_IUPGTK_RENDERER_TEXT", (char*)renderer_txt);

  if (ih->data->show_rename)
    g_object_set(G_OBJECT(renderer_txt), "editable", TRUE, NULL);

  g_object_set(G_OBJECT(renderer_txt), "xpad", 0, NULL);
  g_object_set(G_OBJECT(renderer_txt), "ypad", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(ih->handle), column);

  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ih->handle), FALSE);
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(ih->handle), FALSE);

#if GTK_CHECK_VERSION(2, 10, 0)
  if (iupAttribGetBoolean(ih, "HIDELINES"))
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(ih->handle), FALSE);
  else
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(ih->handle), TRUE);
#endif

#if GTK_CHECK_VERSION(2, 12, 0)
  if (iupAttribGetBoolean(ih, "HIDEBUTTONS"))
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(ih->handle), FALSE);
  else
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(ih->handle), TRUE);
#endif

  if (ih->data->show_dragdrop)
    gtkTreeEnableDragDrop(ih);

  gtk_container_add((GtkContainer*)scrolled_window, ih->handle);
  gtk_widget_show((GtkWidget*)scrolled_window);
  gtk_scrolled_window_set_shadow_type(scrolled_window, GTK_SHADOW_IN); 

  gtk_scrolled_window_set_policy(scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ih->handle));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_set_select_function(selection, (GtkTreeSelectionFunc)gtkTreeSelectionFunc, ih, NULL);

  gtk_tree_view_set_reorderable(GTK_TREE_VIEW(ih->handle), FALSE);

  /* callbacks */
  g_signal_connect(selection,            "changed", G_CALLBACK(gtkTreeSelectionChanged), ih);
  
  g_signal_connect(renderer_txt, "editing-started", G_CALLBACK(gtkTreeCellTextEditingStarted), ih);
  g_signal_connect(renderer_txt,          "edited", G_CALLBACK(gtkTreeCellTextEdited), ih);

  g_signal_connect(G_OBJECT(ih->handle), "enter-notify-event", G_CALLBACK(iupgtkEnterLeaveEvent), ih);
  g_signal_connect(G_OBJECT(ih->handle), "leave-notify-event", G_CALLBACK(iupgtkEnterLeaveEvent), ih);
  g_signal_connect(G_OBJECT(ih->handle), "focus-in-event",     G_CALLBACK(iupgtkFocusInOutEvent), ih);
  g_signal_connect(G_OBJECT(ih->handle), "focus-out-event",    G_CALLBACK(iupgtkFocusInOutEvent), ih);
  g_signal_connect(G_OBJECT(ih->handle), "show-help",          G_CALLBACK(iupgtkShowHelp), ih);
  g_signal_connect(G_OBJECT(ih->handle), "motion-notify-event",G_CALLBACK(iupgtkMotionNotifyEvent), ih);

  g_signal_connect(G_OBJECT(ih->handle),    "row-expanded",    G_CALLBACK(gtkTreeRowExpanded), ih);
  g_signal_connect(G_OBJECT(ih->handle),    "test-expand-row", G_CALLBACK(gtkTreeTestExpandRow), ih);
  g_signal_connect(G_OBJECT(ih->handle),  "test-collapse-row", G_CALLBACK(gtkTreeTestCollapseRow), ih);
  g_signal_connect(G_OBJECT(ih->handle),      "row-activated", G_CALLBACK(gtkTreeRowActived), ih);
  g_signal_connect(G_OBJECT(ih->handle),    "key-press-event", G_CALLBACK(gtkTreeKeyPressEvent), ih);
  g_signal_connect(G_OBJECT(ih->handle),  "key-release-event", G_CALLBACK(gtkTreeKeyReleaseEvent), ih);
  g_signal_connect(G_OBJECT(ih->handle), "button-press-event", G_CALLBACK(gtkTreeButtonEvent), ih);
  g_signal_connect(G_OBJECT(ih->handle), "button-release-event",G_CALLBACK(gtkTreeButtonEvent), ih);

  /* add to the parent, all GTK controls must call this. */
  iupgtkBaseAddToParent(ih);

  if (!iupAttribGetBoolean(ih, "CANFOCUS"))
    GTK_WIDGET_FLAGS(ih->handle) &= ~GTK_CAN_FOCUS;

  gtk_widget_realize((GtkWidget*)scrolled_window);
  gtk_widget_realize(ih->handle);

  /* Initialize the default images */
  ih->data->def_image_leaf = iupImageGetImage("IMGLEAF", ih, 0);
  ih->data->def_image_collapsed = iupImageGetImage("IMGCOLLAPSED", ih, 0);
  ih->data->def_image_expanded = iupImageGetImage("IMGEXPANDED", ih, 0);

  gtkTreeAddRootNode(ih);

  /* configure for DRAG&DROP of files */
  if (IupGetCallback(ih, "DROPFILES_CB"))
    iupAttribSetStr(ih, "DRAGDROP", "YES");

  IupSetCallback(ih, "_IUP_XY2POS_CB", (Icallback)gtkTreeConvertXYToPos);

  iupdrvTreeUpdateMarkMode(ih);

  return IUP_NOERROR;
}

static void gtkTreeUnMapMethod(Ihandle* ih)
{
  ih->data->node_count = 0;

  iupdrvBaseUnMapMethod(ih);
}

void iupdrvTreeInitClass(Iclass* ic)
{
  /* Driver Dependent Class functions */
  ic->Map = gtkTreeMapMethod;
  ic->UnMap = gtkTreeUnMapMethod;

  /* Visual */
  iupClassRegisterAttribute(ic, "BGCOLOR", NULL, gtkTreeSetBgColorAttrib, IUPAF_SAMEASSYSTEM, "TXTBGCOLOR", IUPAF_DEFAULT);
  iupClassRegisterAttribute(ic, "FGCOLOR", NULL, gtkTreeSetFgColorAttrib, IUPAF_SAMEASSYSTEM, "TXTFGCOLOR", IUPAF_DEFAULT);

  /* IupTree Attributes - GENERAL */
  iupClassRegisterAttribute(ic, "EXPANDALL",  NULL, gtkTreeSetExpandAllAttrib,  NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "INDENTATION",  gtkTreeGetIndentationAttrib, gtkTreeSetIndentationAttrib, NULL, NULL, IUPAF_DEFAULT);
  iupClassRegisterAttribute(ic, "DRAGDROP", NULL, iupgtkSetDragDropAttrib, NULL, NULL, IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "SPACING", iupTreeGetSpacingAttrib, gtkTreeSetSpacingAttrib, NULL, NULL, IUPAF_NOT_MAPPED);
  iupClassRegisterAttribute(ic, "TOPITEM", NULL, gtkTreeSetTopItemAttrib, NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);

  /* IupTree Attributes - IMAGES */
  iupClassRegisterAttributeId(ic, "IMAGE", NULL, gtkTreeSetImageAttrib, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "IMAGEEXPANDED", NULL, gtkTreeSetImageExpandedAttrib, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);

  iupClassRegisterAttribute(ic, "IMAGELEAF",            NULL, gtkTreeSetImageLeafAttrib, IUPAF_SAMEASSYSTEM, "IMGLEAF", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGEBRANCHCOLLAPSED", NULL, gtkTreeSetImageBranchCollapsedAttrib, IUPAF_SAMEASSYSTEM, "IMGCOLLAPSED", IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "IMAGEBRANCHEXPANDED",  NULL, gtkTreeSetImageBranchExpandedAttrib, IUPAF_SAMEASSYSTEM, "IMGEXPANDED", IUPAF_NO_INHERIT);

  /* IupTree Attributes - NODES */
  iupClassRegisterAttributeId(ic, "STATE",  gtkTreeGetStateAttrib,  gtkTreeSetStateAttrib, IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "DEPTH",  gtkTreeGetDepthAttrib,  NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "KIND",   gtkTreeGetKindAttrib,   NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "PARENT", gtkTreeGetParentAttrib, NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "COLOR",  gtkTreeGetColorAttrib,  gtkTreeSetColorAttrib, IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "NAME",   gtkTreeGetTitleAttrib,   gtkTreeSetTitleAttrib, IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "TITLE",   gtkTreeGetTitleAttrib,   gtkTreeSetTitleAttrib, IUPAF_NO_INHERIT);
  
  iupClassRegisterAttributeId(ic, "CHILDCOUNT",   gtkTreeGetChildCountAttrib,   NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "TITLEFONT", gtkTreeGetTitleFontAttrib, gtkTreeSetTitleFontAttrib, IUPAF_NO_INHERIT);

  /* IupTree Attributes - MARKS */
  iupClassRegisterAttributeId(ic, "MARKED",   gtkTreeGetMarkedAttrib,   gtkTreeSetMarkedAttrib,   IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute  (ic, "MARK",    NULL,    gtkTreeSetMarkAttrib,    NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute  (ic, "STARTING", NULL, gtkTreeSetMarkStartAttrib, NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute  (ic, "MARKSTART", NULL, gtkTreeSetMarkStartAttrib, NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute  (ic, "MARKEDNODES",    gtkTreeGetMarkedNodesAttrib,    gtkTreeSetMarkedNodesAttrib,    NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);

  iupClassRegisterAttribute  (ic, "VALUE",    gtkTreeGetValueAttrib,    gtkTreeSetValueAttrib,    NULL, NULL, IUPAF_NO_DEFAULTVALUE|IUPAF_NO_INHERIT);

  /* IupTree Attributes - ACTION */
  iupClassRegisterAttributeId(ic, "DELNODE", NULL, gtkTreeSetDelNodeAttrib, IUPAF_NOT_MAPPED|IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "RENAME",  NULL, gtkTreeSetRenameAttrib,  NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "MOVENODE",  NULL, gtkTreeSetMoveNodeAttrib,  IUPAF_NOT_MAPPED|IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "COPYNODE",  NULL, gtkTreeSetCopyNodeAttrib,  IUPAF_NOT_MAPPED|IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
}
