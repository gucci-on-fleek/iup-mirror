/** \file
 * \brief Bindig of iupcb to Lua 3.
 *
 * See Copyright Notice in iup.h
 * $Id: luacb.c,v 1.1 2008-11-21 03:00:12 scuri Exp $
 */
 
#include <stdlib.h>

#include <lua.h>

#include "iup.h"
#include "iuplua.h"
#include "iupcb.h"

#include "iuplua.h"
#include "il.h"
#include "il_controls.h"


static int CB_drag(Ihandle *handle, unsigned char r, unsigned char g, unsigned char b)
{
  iuplua_call_start(handle, "drag");
  lua_pushnumber(r);
  lua_pushnumber(g);
  lua_pushnumber(b);
  return iuplua_call();
}

static int CB_change(Ihandle *handle, unsigned char r, unsigned char g, unsigned char b)
{
  iuplua_call_start(handle, "change");
  lua_pushnumber(r);
  lua_pushnumber(g);
  lua_pushnumber(b);
  return iuplua_call();
}

static void CreateColorBrowser(void) 
{
  int tag = (int)lua_getnumber(lua_getglobal("iuplua_tag"));
  lua_pushusertag(IupColorBrowser(), tag);
}

int cblua_open(void) 
{
  lua_register("iupCreateColorBrowser", CreateColorBrowser);
  lua_register("iup_colorbrowser_drag_cb", (lua_CFunction)CB_drag);
  lua_register("iup_colorbrowser_change_cb", (lua_CFunction)CB_change);

#ifdef IUPLUA_USELOH
#ifdef TEC_BIGENDIAN
#ifdef TEC_64
#include "loh/luacb_be64.loh"
#else
#include "loh/luacb_be32.loh"
#endif  
#else
#ifdef TEC_64
#ifdef WIN64
#include "loh/luacb_le64w.loh"
#else
#include "loh/luacb_le64.loh"
#endif  
#else
#include "loh/luacb.loh"
#endif  
#endif  
#else
  iuplua_dofile("luacb.lua");
#endif

  return 1;
}
