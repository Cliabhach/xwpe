/*
    This file is part of xwpe.

    xwpe is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 2 of the License

	xwpe is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with xwpe.  If not, see http://www.gnu.org/licenses/.
*/

/** /file we_window_mgr.c */
/** This program supports window management functions. */

#include "config.h"
#include "model.h"
#include "we_control.h"

/* Find the last text window in edit control */
int e_find_last_text_window(we_window_t *window)
{
    int i = 0;
    we_control_t *control = window->edit_control;

    for (i = control->mxedt; i > 0 && !DTMD_ISTEXT (control->window[i]->dtmd); i--)
        ;
    return i;
}
