/******************************************************************************
 * Automatically generated file (iuplua5). Please don't change anything.                *
 *****************************************************************************/

#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

#include "iup.h"
#include "iuplua.h"
#include "il.h"


static int Radio(lua_State *L)
{
  Ihandle *ih = IupRadio(iuplua_checkihandle(L, 1));
  iuplua_plugstate(L, ih);
  iuplua_pushihandle_raw(L, ih);
  return 1;
}

int iupradiolua_open(lua_State * L)
{
  iuplua_register(L, Radio, "Radio");


#ifdef IUPLUA_USELH
#include "radio.lh"
#else
  iuplua_dofile(L, "radio.lua");
#endif

  return 0;
}

