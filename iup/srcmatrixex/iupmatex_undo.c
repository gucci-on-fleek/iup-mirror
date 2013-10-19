/** \file
 * \brief IupMatrix Expansion Library.
 *
 * See Copyright Notice in "iup.h"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "iup.h"
#include "iupcbs.h"
#include "iupcontrols.h"
#include "iupmatrixex.h"

#include "iup_object.h"
#include "iup_childtree.h"
#include "iup_register.h"
#include "iup_attrib.h"
#include "iup_str.h"
#include "iup_assert.h"
#include "iup_matrixex.h"


static IattribSetFunc iMatrixSetUndoRedoAttrib = NULL;

typedef struct _IundoData {
  int cell_count;
  const char* name;
  Itable* data_table;
} IundoData;

static void iMatrixExUndoDataInit(IundoData* undo_data, const char* name)
{
  undo_data->cell_count = 0;
  undo_data->name = name;
  undo_data->data_table = iupTableCreate(IUPTABLE_STRINGINDEXED);

  if (name)
  {
    char str[50] = "IUP_";
    strcat(str, name);
    name = IupGetLanguageString(str);
    if (name)
      undo_data->name = name;
  }
}

static void iMatrixExUndoDataClear(IundoData* undo_data)
{
  iupTableDestroy(undo_data->data_table);
  memset(undo_data, 0, sizeof(IundoData));
}

static void iMatrixExUndoDataAddCell(IundoData* undo_data, int lin, int col, const char* value)
{
  char id[40];
  sprintf(id, "%d:%d", lin, col);
  if (!value)
    iupTableSet(undo_data->data_table, id, (void*)"", IUPTABLE_POINTER);  /* store NULL pointers so they can be restored later */
  else
    iupTableSet(undo_data->data_table, id, (void*)value, IUPTABLE_STRING);
  undo_data->cell_count++;
}

static int iMatrixExUndoDataSwap(ImatExData* matex_data, IundoData* undo_data)
{
  char* id = iupTableFirst(undo_data->data_table);
  while (id)
  {
    char *value, *old_value;
    int lin=1, col=1;
    iupStrToIntInt(id, &lin, &col, ':');

    value = (char*)iupTableGetCurr(undo_data->data_table);

    old_value = iupStrDup(iupMatrixExGetCellValue(matex_data->ih, lin, col, 0));

    iupMatrixExSetCellValue(matex_data->ih, lin, col, value);

    if (!old_value)
      iupTableSetCurr(undo_data->data_table, (void*)"", IUPTABLE_POINTER);
    else
    {
      iupTableSetCurr(undo_data->data_table, (void*)old_value, IUPTABLE_STRING);
      free(old_value);
    }

    if (!iupMatrixExBusyInc(matex_data))
      return 0;

    id = iupTableNext(undo_data->data_table);
  }

  return 1;
}

static void iMatrixExUndoStackInit(ImatExData* matex_data)
{
  if (!matex_data->undo_stack) 
    matex_data->undo_stack = iupArrayCreate(40, sizeof(IundoData));
}

static void iMatrixUndoStackAdd(ImatExData* matex_data, const char* name)
{
  int i, undo_stack_count = iupArrayCount(matex_data->undo_stack);
  IundoData* undo_stack_data = (IundoData*)iupArrayGetData(matex_data->undo_stack);

  /* Remove all Redo data */
  for (i=matex_data->undo_stack_pos; i<undo_stack_count; i++)
    iMatrixExUndoDataClear(&(undo_stack_data[i]));

  iMatrixExUndoDataInit(&(undo_stack_data[matex_data->undo_stack_pos]), name);
}

void iupMatrixExUndoPushBegin(ImatExData* matex_data, const char* name)
{
  if (!matex_data->undo_stack_hold)
  {
    iMatrixUndoStackAdd(matex_data, name);
    matex_data->undo_stack_hold = 1;
  }
}

void iupMatrixExUndoPushEnd(ImatExData* matex_data)
{
  if (matex_data->undo_stack_hold)
  {
    matex_data->undo_stack_pos++;
    matex_data->undo_stack_hold = 0;
  }
}

static int iMatrixSetUndoPushCellAttrib(Ihandle* ih, int lin, int col, const char* value)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  IundoData* undo_stack_data = (IundoData*)iupArrayInc(matex_data->undo_stack);

  if (matex_data->undo_stack_hold)
    iMatrixExUndoDataAddCell(&(undo_stack_data[matex_data->undo_stack_pos]), lin, col, value);
  else
  {
    iMatrixUndoStackAdd(matex_data, "SETCELL");
    iMatrixExUndoDataAddCell(&(undo_stack_data[matex_data->undo_stack_pos]), lin, col, value);
    matex_data->undo_stack_pos++;
  }

  return 0;
}

static int iMatrixSetUndoClearAttrib(Ihandle* ih, const char* value)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  int undo_stack_count = matex_data->undo_stack? iupArrayCount(matex_data->undo_stack): 0;
  if (undo_stack_count)
  {
    int i, undo_stack_count = iupArrayCount(matex_data->undo_stack);
    IundoData* undo_stack_data = (IundoData*)iupArrayGetData(matex_data->undo_stack);
    for (i=0; i<undo_stack_count; i++)
      iMatrixExUndoDataClear(&(undo_stack_data[i]));
    iupArrayRemove(matex_data->undo_stack, 0, undo_stack_count);
    matex_data->undo_stack_pos = 0;
  }
  (void)value;
  return 0;
}

static char* iMatrixGetUndoAttrib(Ihandle* ih)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  int undo_stack_count = iupArrayCount(matex_data->undo_stack);
  if (matex_data->undo_stack && undo_stack_count)
    return iupStrReturnBoolean(matex_data->undo_stack_pos>0);
  return NULL; 
}

static char* iMatrixGetUndoCountAttrib(Ihandle* ih)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  int undo_stack_count = iupArrayCount(matex_data->undo_stack);
  if (matex_data->undo_stack && undo_stack_count)
    return iupStrReturnInt(undo_stack_count);
  return NULL; 
}

static char* iMatrixGetUndoNameAttrib(Ihandle* ih, int id)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  int undo_stack_count = iupArrayCount(matex_data->undo_stack);
  if (matex_data->undo_stack && undo_stack_count)
  {
    IundoData* undo_stack_data = (IundoData*)iupArrayGetData(matex_data->undo_stack);

    if (id < 0 || id >= undo_stack_count)
      return NULL;

    return iupStrReturnStr(undo_stack_data[id].name);
  }
  return NULL; 
}

static int iMatrixSetUndoAttrib(Ihandle* ih, const char* value)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  int undo_stack_count = matex_data->undo_stack? iupArrayCount(matex_data->undo_stack): 0;
  if (undo_stack_count && matex_data->undo_stack_pos>0)
  {
    int i, count = 1, total_cell_count = 0;
    IundoData* undo_stack_data = (IundoData*)iupArrayGetData(matex_data->undo_stack);

    iupStrToInt(value, &count);
    if (count > matex_data->undo_stack_pos)
      count = matex_data->undo_stack_pos;

    for (i=0; i<count; i++)
      total_cell_count += undo_stack_data[matex_data->undo_stack_pos-1 - i].cell_count;

    iMatrixSetUndoRedoAttrib(ih, "NO");  /* Disable Undo/Redo systrem during restore */
    iupMatrixExBusyStart(matex_data, total_cell_count, "UNDO");

    for (i=0; i<count; i++)
    {
      if (!iMatrixExUndoDataSwap(matex_data, &(undo_stack_data[matex_data->undo_stack_pos-1 - i])))
      {
        matex_data->undo_stack_pos -= i;
        iMatrixSetUndoRedoAttrib(ih, "Yes");
        return 0;
      }
    }

    iupMatrixExBusyEnd(matex_data);
    iMatrixSetUndoRedoAttrib(ih, "Yes");
    matex_data->undo_stack_pos -= i;
  }
  return 0;
}

static char* iMatrixGetRedoAttrib(Ihandle* ih)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  int undo_stack_count = matex_data->undo_stack? iupArrayCount(matex_data->undo_stack): 0;
  if (undo_stack_count)
    return iupStrReturnBoolean(matex_data->undo_stack_pos<undo_stack_count);
  return NULL; 
}

static int iMatrixSetRedoAttrib(Ihandle* ih, const char* value)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  int undo_stack_count = matex_data->undo_stack? iupArrayCount(matex_data->undo_stack): 0;
  if (undo_stack_count && matex_data->undo_stack_pos<undo_stack_count)
  {
    int i, count = 1, total_cell_count = 0, undo_stack_count = iupArrayCount(matex_data->undo_stack);
    IundoData* undo_stack_data = (IundoData*)iupArrayGetData(matex_data->undo_stack);

    iupStrToInt(value, &count);
    if (count > undo_stack_count - matex_data->undo_stack_pos)
      count = undo_stack_count - matex_data->undo_stack_pos;

    for (i=0; i<count; i++)
      total_cell_count += undo_stack_data[matex_data->undo_stack_pos + i].cell_count;

    iMatrixSetUndoRedoAttrib(ih, "NO");  /* Disable Undo/Redo systrem during restore */
    iupMatrixExBusyStart(matex_data, total_cell_count, "REDO");

    for (i=0; i<count; i++)
    {
      if (!iMatrixExUndoDataSwap(matex_data, &(undo_stack_data[matex_data->undo_stack_pos + i])))
      {
        matex_data->undo_stack_pos += i;
        iMatrixSetUndoRedoAttrib(ih, "Yes");
        return 0;
      }
    }

    iupMatrixExBusyEnd(matex_data);
    iMatrixSetUndoRedoAttrib(ih, "Yes");
    matex_data->undo_stack_pos += i;
  }
  return 0;
}

static int iMatrixSetUndoPushBeginAttrib(Ihandle* ih, const char* value)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  iupMatrixExUndoPushBegin(matex_data, value);
  return 0;
}

static int iMatrixSetUndoPushEndAttrib(Ihandle* ih, const char* value)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  iupMatrixExUndoPushEnd(matex_data);
  (void)value;
  return 0;
}

static int iMatrixExSetUndoRedoAttrib(Ihandle* ih, const char* value)
{
  ImatExData* matex_data = (ImatExData*)iupAttribGet(ih, "_IUP_MATEX_DATA");
  if (iupStrBoolean(value))
    iMatrixExUndoStackInit(matex_data);
  else
    iMatrixSetUndoClearAttrib(ih, NULL);
  return iMatrixSetUndoRedoAttrib(ih, value);
}

void iupMatrixExRegisterUndo(Iclass* ic)
{
  /* Already defined in IupMatrix, redefined here */
  iupClassRegisterGetAttribute(ic, "UNDOREDO", NULL, &iMatrixSetUndoRedoAttrib, NULL, NULL, NULL);
  iupClassRegisterReplaceAttribFunc(ic, "UNDOREDO", NULL, iMatrixExSetUndoRedoAttrib);

  iupClassRegisterAttribute(ic, "UNDO", iMatrixGetUndoAttrib, iMatrixSetUndoAttrib, NULL, NULL, IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "REDO", iMatrixGetRedoAttrib, iMatrixSetRedoAttrib, NULL, NULL, IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "UNDOCLEAR", NULL, iMatrixSetUndoClearAttrib, NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttributeId(ic, "UNDONAME", iMatrixGetUndoNameAttrib, NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "UNDOCOUNT", iMatrixGetUndoCountAttrib, NULL, NULL, NULL, IUPAF_READONLY|IUPAF_NO_INHERIT);

  /* Internal attributes */
  iupClassRegisterAttributeId2(ic, "UNDOPUSHCELL", NULL, iMatrixSetUndoPushCellAttrib, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "UNDOPUSHBEGIN", NULL, iMatrixSetUndoPushBeginAttrib, NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);
  iupClassRegisterAttribute(ic, "UNDOPUSHEND", NULL, iMatrixSetUndoPushEndAttrib, NULL, NULL, IUPAF_WRITEONLY|IUPAF_NO_INHERIT);

  if (iupStrEqualNoCase(IupGetGlobal("LANGUAGE"), "ENGLISH"))
  {
    IupSetLanguageString("IUP_PASTECLIP", "Paste from Clipboard");
    IupSetLanguageString("IUP_PASTEDATA", "Paste from Buffer");
    IupSetLanguageString("IUP_PASTEFILE", "Paste from File (Import)");
    IupSetLanguageString("IUP_COPYCOLTO:ALL", "Copy To All Lines");
    IupSetLanguageString("IUP_COPYCOLTO:TOP", "Copy To Top");
    IupSetLanguageString("IUP_COPYCOLTO:BOTTOM", "Copy To Bottom");
    IupSetLanguageString("IUP_COPYCOLTO:MARKED", "Copy To Marked");
    IupSetLanguageString("IUP_COPYCOLTO:INTERVAL", "Copy To Interval");
    IupSetLanguageString("IUP_UNDONAME", "Undo");  /* To avoid conflict with the menu item string */
    IupSetLanguageString("IUP_REDONAME", "Redo");
    IupSetLanguageString("IUP_SETCELL", "Set Cell");
    IupSetLanguageString("IUP_EDITCELL", "Edit Cell");
  }
  else if (iupStrEqualNoCase(IupGetGlobal("LANGUAGE"), "PORTUGUESE"))
  {
    IupSetLanguageString("IUP_PASTECLIP", "Colar do Clipboard");
    IupSetLanguageString("IUP_PASTEDATA", "Colar de um Buffer");
    IupSetLanguageString("IUP_PASTEFILE", "Colar de Arquivo (Importar)");
    IupSetLanguageString("IUP_COPYCOLTO:ALL", "Copiar para Todas as Linhas");
    IupSetLanguageString("IUP_COPYCOLTO:TOP", "Copiar para Topo");
    IupSetLanguageString("IUP_COPYCOLTO:BOTTOM", "Copiar para fim");
    IupSetLanguageString("IUP_COPYCOLTO:MARKED", "Copiar para Selecionadas");
    IupSetLanguageString("IUP_COPYCOLTO:INTERVAL", "Copiar para Intervalo");
    IupSetLanguageString("IUP_UNDONAME", "Desfazer");
    IupSetLanguageString("IUP_REDONAME", "Refazer");
    IupSetLanguageString("IUP_SETCELL", "Modificar C�lula");
    IupSetLanguageString("IUP_EDITCELL", "Editar C�lula");

    if (IupGetInt(NULL, "UTF8MODE"))
    {
      IupSetLanguageString("IUP_SETCELL", "Modificar Célula");
      IupSetLanguageString("IUP_EDITCELL", "Editar Célula");
    }
  }
}

