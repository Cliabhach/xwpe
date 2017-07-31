#ifndef WE_MENUE_H
#define WE_MENUE_H

#include "config.h"
#include "we_block.h"
#include "we_edit.h"
#include "we_mouse.h"
#include "we_opt.h"
#include "we_debug.h"

/*   we_menue.c   */
int WpeHandleMainmenu (int n, We_window * f);
int WpeHandleSubmenu (int xa, int ya, int xe, int ye,
                      int nm, OPTK * fopt, We_window * f);

#endif
