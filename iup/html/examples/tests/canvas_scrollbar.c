#include <stdlib.h>
#include <stdio.h>
#include "iup.h"
#include "cd.h"
#include "cdiup.h"
#include "wd.h"


/* World:
   The canvas will be a window into that space.
   If canvas is smaller than the virtual space, scrollbars are active.
   The drawing is a red X connecting the corners of the world,
   plus a box inside the borders.

   Remember that:
   XMIN<=POSX<=XMAX-DX
*/
#define WORLD_W 600
#define WORLD_H 400
static int scale = 1;

static void update_scrollbar(Ihandle* ih, int canvas_w, int canvas_h)
{
  /* update page size, it is always the client size of the canvas,
     but must convert it to world coordinates.
     If you change canvas size or scale must call this function. */
  double ww, wh;
  if (scale > 0)
  {
    ww = (double)canvas_w/scale;
    wh = (double)canvas_h/scale;
  }
  else
  {
    ww = canvas_w*abs(scale);
    wh = canvas_h*abs(scale);
  }
  IupSetfAttribute(ih, "DX", "%g", ww);
  IupSetfAttribute(ih, "DY", "%g", wh);
}

static void update_viewport(Ihandle* ih, cdCanvas *canvas, float posx, float posy)
{
  int view_x, view_y, view_w, view_h;

  /* The CD viewport is the same area represented by the virtual space of the scrollbar,
     but not using the same coordinates. */

  /* posy is top-bottom, CD is bottom-top.
     invert posy reference (YMAX-DY - POSY) */
  posy = IupGetFloat(ih, "YMAX")-IupGetFloat(ih, "DY") - posy;
  if (posy < 0) posy = 0;

  if (scale > 0)
  {
    view_w = WORLD_W*scale;
    view_h = WORLD_H*scale;
    view_x = (int)(posx*scale);
    view_y = (int)(posy*scale);
  }
  else
  {
    view_w = WORLD_W/abs(scale);
    view_h = WORLD_H/abs(scale);
    view_x = (int)(posx/abs(scale));
    view_y = (int)(posy/abs(scale));
  }

  wdCanvasViewport(canvas, -view_x, view_w-1 - view_x, -view_y, view_h-1 - view_y);
}

/************************************************************************************/

static int action(Ihandle *ih, float posx, float posy)
{
  cdCanvas *canvas = (cdCanvas*)IupGetAttribute(ih, "_CD_CANVAS");

  printf("ACTION(posx=%.2f, posy=%.2f)\n", posx, posy);
  cdCanvasActivate(canvas);
  cdCanvasClear(canvas);

  cdCanvasForeground(canvas, CD_RED);
  wdCanvasLine(canvas, 0, 0, WORLD_W, WORLD_H);
  wdCanvasLine(canvas, 0, WORLD_H, WORLD_W, 0);
  wdCanvasArc(canvas, WORLD_W/2, WORLD_H/2+WORLD_H/10, WORLD_W/10, WORLD_H/10, 0, 360);

  wdCanvasLine(canvas, 0, 0, WORLD_W, 0);
  wdCanvasLine(canvas, 0, WORLD_H, WORLD_W, WORLD_H);
  wdCanvasLine(canvas, 0, 0, 0, WORLD_H);
  wdCanvasLine(canvas, WORLD_W, 0, WORLD_W, WORLD_H);

  return IUP_DEFAULT;
}

static int resize_cb(Ihandle *ih, int canvas_w, int canvas_h)
{
  cdCanvas *canvas = (cdCanvas*)IupGetAttribute(ih, "_CD_CANVAS");

printf("RESIZE_CB(%d, %d) RASTERSIZE=%s DRAWSIZE=%s \n", canvas_w, canvas_h, IupGetAttribute(ih, "RASTERSIZE"), IupGetAttribute(ih, "DRAWSIZE"));
  /* When *AUTOHIDE=Yes, this can hide a scrollbar and so change the canvas drawsize */
  update_scrollbar(ih, canvas_w, canvas_h);  
printf("                                DRAWSIZE=%s \n", IupGetAttribute(ih, "DRAWSIZE"));
  /* update the canvas size */
  IupGetIntInt(ih, "DRAWSIZE", &canvas_w, &canvas_h);
  update_scrollbar(ih, canvas_w, canvas_h);  

  /* update the application */
  cdCanvasActivate(canvas);
  update_viewport(ih, canvas, IupGetFloat(ih, "POSX"), IupGetFloat(ih, "POSY"));

  return IUP_DEFAULT;
}

static int scroll_cb(Ihandle *ih, int op, float posx, float posy)
{
  cdCanvas *canvas = (cdCanvas*)IupGetAttribute(ih, "_CD_CANVAS");
printf("SCROLL_CB(op=%d, posx=%.2f, posy=%.2f)\n", op, posx, posy);
  cdCanvasActivate(canvas);
  update_viewport(ih, canvas, posx, posy);
  IupRedraw(ih, 0);
  (void)op;
  return IUP_DEFAULT;
}

static int wheel_cb(Ihandle *ih,float delta,int x,int y,char* status)
{
  int canvas_w, canvas_h;
  cdCanvas *canvas = (cdCanvas*)IupGetAttribute(ih, "_CD_CANVAS");
  (void)x;
  (void)y;
  (void)status;

  if (scale+delta==0) /* skip 0 */
  {
    if (scale > 0) 
      scale = -1;
    else 
      scale = 1;
  }
  else
    scale += (int)delta;

  cdCanvasActivate(canvas);
  cdCanvasGetSize(canvas, &canvas_w, &canvas_h, NULL, NULL);
  update_scrollbar(ih, canvas_w, canvas_h);
  update_viewport(ih, canvas, IupGetFloat(ih, "POSX"), IupGetFloat(ih, "POSY"));
  IupRedraw(ih, 0);

  printf("SCROLLVISIBLE=%s\n", IupGetAttribute(ih, "SCROLLVISIBLE"));
  
  return IUP_DEFAULT;
}

static int map_cb(Ihandle *ih)
{
  /* canvas will be automatically saved in "_CD_CANVAS" attribute */
  cdCanvas *canvas = cdCreateCanvas(CD_IUP, ih);

  /* World size is fixed */
  wdCanvasWindow(canvas, 0, WORLD_W, 0, WORLD_H);

  /* handle scrollbar in world coordinates, so we only have to update DX/DY */
  IupSetAttribute(ih, "XMIN", "0");
  IupSetAttribute(ih, "YMIN", "0");
  IupSetfAttribute(ih, "XMAX", "%d", WORLD_W);
  IupSetfAttribute(ih, "YMAX", "%d", WORLD_H);

  return IUP_DEFAULT;
}

static int unmap_cb(Ihandle *ih)
{
  cdCanvas *canvas = (cdCanvas*)IupGetAttribute(ih, "_CD_CANVAS");
  cdKillCanvas(canvas);
  return IUP_DEFAULT;
}

void CanvasScrollbarTest(void)
{
  Ihandle *dlg, *cnv;

  cnv = IupCanvas(NULL);
  IupSetAttribute(cnv, "RASTERSIZE", "300x200"); /* initial size */
  IupSetAttribute(cnv, "SCROLLBAR", "YES");
  IupSetAttribute(cnv, "BORDER", "NO");
  //  IupSetAttribute(cnv, "EXPAND", "NO");

  IupSetCallback(cnv, "RESIZE_CB",  (Icallback)resize_cb);
  IupSetCallback(cnv, "ACTION",  (Icallback)action);
  IupSetCallback(cnv, "MAP_CB",  (Icallback)map_cb);
  IupSetCallback(cnv, "UNMAP_CB",  (Icallback)unmap_cb);
  IupSetCallback(cnv, "WHEEL_CB",  (Icallback)wheel_cb);
  IupSetCallback(cnv, "SCROLL_CB",  (Icallback)scroll_cb);
                   
  dlg = IupDialog(IupVbox(cnv, NULL));
  IupSetAttribute(dlg, "TITLE", "Scrollbar Test");
  IupSetAttribute(dlg, "MARGIN", "10x10");

  IupMap(dlg);
  IupSetAttribute(cnv, "RASTERSIZE", NULL);  /* release the minimum limitation */
 
  IupShowXY(dlg,IUP_CENTER,IUP_CENTER);
}

#ifndef BIG_TEST
int main(int argc, char* argv[])
{
  IupOpen(&argc, &argv);

  CanvasScrollbarTest();

  IupMainLoop();

  IupClose();

  return EXIT_SUCCESS;
}
#endif
