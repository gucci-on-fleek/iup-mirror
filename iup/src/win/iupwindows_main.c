/** \file
 * \brief Windows Driver WinMain
 *
 * See Copyright Notice in "iup.h"
 */

#include <windows.h>

#include <stdlib.h> /* declaration of __argc and __argv */

#include "iup.h"
              
              
#ifdef __WATCOMC__     /* force Watcom to link this module, called from IupOpen */
void iupwinMainDummy(void)
{
  return;
}
#else
extern int main(int, char **);
#endif

/* save this handle in DllMain only, use to load resources from the DLL. */
HINSTANCE iupwin_dll_hinstance = 0;

#ifdef IUP_DLL 
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  (void)fdwReason;
  (void)lpvReserved;

  iupwin_dll_hinstance = hinstDLL;

  return TRUE;
}
#else
/* this module is always linked in the makefile, 
   But it must not define WinMain if building the DLL */
int PASCAL WinMain (HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int ncmdshow)
{
  (void)hinst;  /* NOT used */
  (void)hprev;
  (void)cmdline;
  (void)ncmdshow;
  
#ifdef __WATCOMC__
  {
    extern int _argc;
    extern char** _argv;
    return IupMain(_argc, _argv);
  }              
#else
  {                        
    /* this seems to work for all the compilers we tested, except Watcom compilers */
    /* These are declared in <stdlib.h> */
    /* extern int __argc;               */
    /* extern char** __argv;            */
    return main(__argc, __argv);
  }
#endif
}
#endif
