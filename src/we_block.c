/** \file we_block.c                                      */
/* Copyright (C) 1993 Fred Kruse                          */
/* This is free software; you can redistribute it and/or  */
/* modify it under the terms of the                       */
/* GNU General Public License, see the file COPYING.      */

#include "config.h"
#include <string.h>
#include "keys.h"
#include "messages.h"
#include "model.h"
#include "edit.h"
#include "we_screen.h"
#include "we_term.h"
#include "we_block.h"
#include "we_progn.h"
#include "we_edit.h"
#include <ctype.h>

/*	delete block */
/**
 * e_block_del.
 *
 * window we_window_t the window struct used for all windows.
 *
 * window->s is the screen containing the marked block.
 * window->s->mark_begin (x, y) mark the beginning and should be sane values or
 *                         the function will return zero.
 * window->s->mark_end (x, y)   mark the end and should be sane values.
 *
 *
 */
int
e_block_del (we_window_t * window)
{
    we_buffer_t *buffer;
    we_screen_t *screen;
    int i, y, len;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    screen = window->screen;
    if ((screen->mark_end.y < screen->mark_begin.y) ||
            ((screen->mark_begin.y == screen->mark_end.y)
             && (screen->mark_end.x <= screen->mark_begin.x))) {
        return (0);
    }
    if (window->ins == (char) 8) {
        return (WPE_ESC);
    }
    if (screen->mark_begin.y == screen->mark_end.y) {
        e_del_nchar (buffer, screen, screen->mark_begin.x, screen->mark_begin.y,
                     screen->mark_end.x - screen->mark_begin.x);
        buffer->cursor.x = screen->mark_end.x = screen->mark_begin.x;
    } else {
        e_add_undo ('d', buffer, screen->mark_begin.x, screen->mark_begin.y, 0);
        window->save = buffer->control->maxchg + 1;

        /*********** start debugging code ************/
        y = screen->mark_begin.y;
        if (screen->mark_begin.x > 0) {
            y++;
        }
        len = y - screen->mark_end.y + 1;
        e_brk_recalc (window, y, len);		// recalculate breakpoints
        /*********** start debugging code ************/
    }
    if (window->c_sw) {
        window->c_sw = e_sc_txt (window->c_sw, window->buffer);
    }
    e_cursor (window, 1);
    e_write_screen (window, 1);
    return (0);
}

/*      dup selected block BD*/
int
e_block_dup (char *dup, we_window_t * window)
{
    we_buffer_t *buffer;
    we_screen_t *screen;
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    if (window->ins == 8) {
        return (0);
    }
    buffer = window->buffer;
    screen = window->screen;
    i = screen->mark_end.x - screen->mark_begin.x;
    /* Brian thinks that 80 characters is enough.  For the time being return
       nothing if longer than that. */
    if ((screen->mark_end.y != screen->mark_begin.y) || (i <= 0) || (i >= 80)) {
        return (0);
    }
    strncpy (dup, (const char *) &buffer->buflines[screen->mark_begin.y].s[screen->mark_begin.x], i);
    dup[i] = 0;
    return (i);
}

int
e_block_clear (we_buffer_t * buffer, we_screen_t * s)
{
    int i;
    int len = (s->mark_end.y - s->mark_begin.y - 1);

    if (s->mark_end.y == s->mark_begin.y) {
        e_del_nchar (buffer, s, s->mark_begin.x, s->mark_end.y,
                     s->mark_end.x - s->mark_begin.x);
        buffer->cursor.x = s->mark_end.x = s->mark_begin.x;
        buffer->cursor.y = s->mark_end.y = s->mark_begin.y;
        return (0);
    }
    for (i = s->mark_begin.y + 1; i < s->mark_end.y; i++) {
        free (buffer->buflines[i].s);
    }
    for (i = s->mark_begin.y + 1; i <= buffer->mxlines - len; i++) {
        buffer->buflines[i] = buffer->buflines[i + len];
    }
    (buffer->mxlines) -= len;
    (s->mark_end.y) -= len;
    e_del_nchar (buffer, s, 0, s->mark_end.y, s->mark_end.x);
    if (*(buffer->buflines[s->mark_begin.y].s + (len = buffer->buflines[s->mark_begin.y].len)) !=
            '\0') {
        len++;
    }
    e_del_nchar (buffer, s, s->mark_begin.x, s->mark_begin.y, len - s->mark_begin.x);
    buffer->cursor.x = s->mark_end.x = s->mark_begin.x;
    buffer->cursor.y = s->mark_end.y = s->mark_begin.y;
    return (0);
}

/*   write buffer to screen */
int
e_show_clipboard (we_window_t * window)
{
    we_control_t *control = window->edit_control;
    we_window_t *fo;
    int i, j;

    for (j = 1; j <= control->mxedt; j++)
        if (control->window[j] == control->window[0]) {
            e_switch_window (control->edt[j], window);
            return (0);
        }

    if (control->mxedt > max_edit_windows()) {
        e_error (e_msg[ERR_MAXWINS], ERROR_MSG, control->colorset);
        return (0);
    }
    for (j = 1; j <= max_edit_windows(); j++) {
        for (i = 1; i <= control->mxedt && control->edt[i] != j; i++);
        if (i > control->mxedt) {
            break;
        }
    }
    control->curedt = j;
    (control->mxedt)++;
    control->edt[control->mxedt] = j;

    window = control->window[control->mxedt] = control->window[0];
#ifdef PROG
    if (WpeIsProg ()) {
        for (i = control->mxedt - 1; i > 0; i--);
        if (i < 1) {
            window->a = e_set_pnt (0, 1);
            window->e = e_set_pnt (max_screen_cols() - 1, 2 * max_screen_lines() / 3);
        } else {
            window->a = e_set_pnt (control->window[i]->a.x + 1, control->window[i]->a.y + 1);
            window->e = e_set_pnt (control->window[i]->e.x, control->window[i]->e.y);
        }
    } else
#endif
    {
        if (control->mxedt < 2) {
            window->a = e_set_pnt (0, 1);
            window->e = e_set_pnt (max_screen_cols() - 1, max_screen_lines() - 2);
        } else {
            window->a =
                e_set_pnt (control->window[control->mxedt - 1]->a.x + 1,
                           control->window[control->mxedt - 1]->a.y + 1);
            window->e =
                e_set_pnt (control->window[control->mxedt - 1]->e.x, control->window[control->mxedt - 1]->e.y);
        }
    }
    window->winnum = control->curedt;

    if (control->mxedt > 1) {
        fo = control->window[control->mxedt - 1];
        e_ed_rahmen (fo, 0);
    }
    e_firstl (window, 1);
    e_zlsplt (window);
    e_write_screen (window, 1);
    e_cursor (window, 1);
    return (0);
}

/*   move block to buffer */
int
e_edt_del (we_window_t * window)
{
    e_edt_copy (window);
    e_block_del (window);
    return (0);
}

/* copy block to buffer */
int
e_edt_copy (we_window_t * window)
{
    we_buffer_t *buffer;
    we_buffer_t *b0 = window->edit_control->window[0]->buffer;
    int i, save;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    save = window->save;
    if ((window->screen->mark_end.y < window->screen->mark_begin.y) ||
            ((window->screen->mark_begin.y == window->screen->mark_end.y) &&
             (window->screen->mark_end.x <= window->screen->mark_begin.x))) {
        return (0);
    }
    for (i = 1; i < b0->mxlines; i++) {
        free (b0->buflines[i].s);
    }
    b0->mxlines = 1;
    *(b0->buflines[0].s) = WPE_WR;
    *(b0->buflines[0].s + 1) = '\0';
    b0->buflines[0].len = 0;
    e_copy_block (0, 0, buffer, b0, window);
    window->save = save;
    return (0);
}

/*            Copy block buffer into window  */
int
e_edt_einf (we_window_t * window)
{
    we_buffer_t *buffer;
    we_buffer_t *b0 = window->edit_control->window[0]->buffer;
    int i, y, len;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    if (window->ins == 8) {
        return (0);
    }
    global_disable_add_undo = 1;

    /**********************/
    y = buffer->cursor.y;
    if (buffer->cursor.x > 0) {
        y++;
    }
    /**********************/

    e_copy_block (buffer->cursor.x, buffer->cursor.y, b0, buffer, window);

    /**********************/
    len = b0->window->screen->mark_end.y - b0->window->screen->mark_begin.y;
    e_brk_recalc (window, y, len);		// recalculate breakpoints
    /**********************/

    global_disable_add_undo = 0;
    e_add_undo ('c', buffer, buffer->cursor.x, buffer->cursor.y, 0);
    e_sc_txt_2 (window);
    window->save = buffer->control->maxchg + 1;
    return (0);
}

/*   move block within window */
int
e_block_move (we_window_t * window)
{
    we_buffer_t *buffer;
    int i;
    we_point_t ka;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    ka.x = window->edit_control->window[window->edit_control->mxedt]->screen->mark_begin.x;
    if (buffer->cursor.y > window->edit_control->window[window->edit_control->mxedt]->screen->mark_begin.y) {
        ka.y = window->edit_control->window[window->edit_control->mxedt]->screen->mark_begin.y;
    } else {
        ka.y = window->edit_control->window[window->edit_control->mxedt]->screen->mark_end.y;
        if (buffer->cursor.y == window->edit_control->window[window->edit_control->mxedt]->screen->mark_begin.y &&
                buffer->cursor.x < window->edit_control->window[window->edit_control->mxedt]->screen->mark_begin.x)
            ka.x = window->edit_control->window[window->edit_control->mxedt]->screen->mark_end.x +
                   window->edit_control->window[window->edit_control->mxedt]->screen->mark_begin.x - buffer->cursor.x;
    }
    global_disable_add_undo = 1;
    e_move_block (buffer->cursor.x, buffer->cursor.y, buffer, buffer, window);
    global_disable_add_undo = 0;
    e_add_undo ('v', buffer, ka.x, ka.y, 0);
    window->save = buffer->control->maxchg + 1;
    return (0);
}

/*    move Block    */
void
e_move_block (int x, int y, we_buffer_t * bv, we_buffer_t * bz, we_window_t * window)
{
    we_screen_t *s = window->edit_control->window[window->edit_control->mxedt]->screen;
    we_screen_t *sv = bv->window->screen;
    we_screen_t *sz = bz->window->screen;
    int sw = (y < s->mark_begin.y) ? 0 : 1;
    int i;
    int n = s->mark_end.y - s->mark_begin.y - 1;
    int kax = s->mark_begin.x, kay = s->mark_begin.y, kex =
                                         s->mark_end.x, key = s->mark_end.y;
    we_string_t *str, *tmp;
    unsigned char *cstr;

    if (key < kay || (kay == key && kex <= kax)) {
        return;
    }
    if (window->ins == 8) {
        return;
    }
    if (bv == bz && y >= kay && y <= key && x >= kax && x <= kex) {
        return;
    }
    if (kay == key) {
        n = kex - kax;
        bz->cursor.x = x;
        bz->cursor.y = y;
        if ((cstr = malloc (window->edit_control->maxcol * sizeof (char))) == NULL) {
            e_error (e_msg[ERR_LOWMEM], ERROR_MSG, bz->colorset);
            return;
        }
        for (i = 0; i < n; i++) {
            cstr[i] = bv->buflines[key].s[kax + i];
        }
        e_ins_nchar (bz, sz, cstr, x, y, n);
        bv->cursor.x = kax;
        bv->cursor.y = kay + bz->cursor.y - y;
        e_del_nchar (bv, sv, sv->mark_begin.x, sv->mark_begin.y, n);
        if (bv == bz && kay == y && x > kex) {
            x -= n;
        }
        sz->mark_begin.x = x;
        sz->mark_end.x = bz->cursor.x = x + n;
        sz->mark_begin.y = sz->mark_end.y = bz->cursor.y = y;
        e_cursor (window, 1);
        e_write_screen (window, 1);
        free (cstr);
        return;
    }

    if (bv == bz && y == kay && x < kax) {
        n = kax - x;
        bv->cursor.x = x;
        bv->cursor.y = y;
        if ((cstr = malloc (window->edit_control->maxcol * sizeof (char))) == NULL) {
            e_error (e_msg[ERR_LOWMEM], ERROR_MSG, bz->colorset);
            return;
        }
        for (i = 0; i < n; i++) {
            cstr[i] = bv->buflines[y].s[x + i];
        }
        e_del_nchar (bv, sv, x, y, n);
        bz->cursor.x = kex;
        bz->cursor.y = key;
        e_ins_nchar (bz, sz, cstr, kex, key, n);
        bv->cursor.x = sv->mark_begin.x = x;
        bv->cursor.y = sv->mark_begin.y = y;
        sv->mark_end.x = kex;
        sv->mark_end.y = key;
        e_cursor (window, 1);
        e_write_screen (window, 1);
        free (cstr);
        return;
    }

    if (bv == bz && y == key && x > kex) {
        n = x - kex;
        bv->cursor.x = kex;
        bv->cursor.y = y;
        if ((cstr = malloc (window->edit_control->maxcol * sizeof (char))) == NULL) {
            e_error (e_msg[ERR_LOWMEM], ERROR_MSG, bz->colorset);
            return;
        }
        for (i = 0; i < n; i++) {
            cstr[i] = bv->buflines[y].s[kex + i];
        }
        e_del_nchar (bv, sv, kex, y, n);
        bz->cursor.x = kex;
        bz->cursor.y = key;
        e_ins_nchar (bz, sz, cstr, kax, kay, n);

        bv->cursor.x = sv->mark_end.x;
        bv->cursor.y = sv->mark_end.y;
        e_cursor (window, 1);
        e_write_screen (window, 1);
        free (cstr);
        return;
    }

    while (bz->mxlines + n > bz->mx.y - 2) {
        bz->mx.y += MAXLINES;
        if ((tmp = realloc (bz->buflines, bz->mx.y * sizeof (we_string_t))) == NULL) {
            e_error (e_msg[ERR_LOWMEM], SERIOUS_ERROR_MSG, bz->colorset);
        } else {
            bz->buflines = tmp;
        }
        if (bz->window->c_sw) {
            bz->window->c_sw = realloc (bz->window->c_sw, bz->mx.y * sizeof (int));
        }
    }
    if ((str = malloc ((n + 2) * sizeof (we_string_t))) == NULL) {
        e_error (e_msg[ERR_LOWMEM], ERROR_MSG, bz->colorset);
        return;
    }
    for (i = kay; i <= key; i++) {
        str[i - kay] = bv->buflines[i];
    }
    e_new_line (y + 1, bz);
    if (*(bz->buflines[y].s + bz->buflines[y].len) != '\0') {
        (bz->buflines[y].len)++;
    }
    for (i = x; i <= bz->buflines[y].len; i++) {
        *(bz->buflines[y + 1].s + i - x) = *(bz->buflines[y].s + i);
    }
    *(bz->buflines[y].s + x) = '\0';
    bz->buflines[y].len = bz->buflines[y].nrc = x;
    bz->buflines[y + 1].len = e_str_len (bz->buflines[y + 1].s);
    bz->buflines[y + 1].nrc = strlen ((const char *) bz->buflines[y + 1].s);
    for (i = bz->mxlines; i > y; i--) {
        bz->buflines[i + n] = bz->buflines[i];
    }
    (bz->mxlines) += n;
    for (i = 1; i <= n; i++) {
        bz->buflines[y + i] = str[i];
    }

    if (bz == bv) {
        if (y < kay) {
            kay += (n + 1);
            key += (n + 1);
        } else if (y > kay) {
            y -= n;
        }
    }

    for (i = kay + 1; i <= bv->mxlines - n; i++) {
        bv->buflines[i] = bv->buflines[i + n];
    }
    (bv->mxlines) -= n;

    if (*(str[0].s + (n = str[0].len)) != '\0') {
        n++;
    }
    e_ins_nchar (bz, sz, (str[key - kay].s), 0, y + key - kay, kex);
    e_ins_nchar (bz, sz, (str[0].s + kax), x, y, n - kax);
    e_del_nchar (bv, sv, 0, kay + 1, kex);
    e_del_nchar (bv, sv, kax, kay, n - kax);

    if (bz == bv && sw != 0) {
        y--;
    }

    sv->mark_begin.x = sv->mark_end.x = bv->cursor.x = kax;
    sv->mark_begin.y = sv->mark_end.y = bv->cursor.y = kay;

    sz->mark_begin.x = x;
    sz->mark_begin.y = y;
    bz->cursor.x = sz->mark_end.x = kex;
    bz->cursor.y = sz->mark_end.y = key - kay + y;

    window->save = window->edit_control->maxchg + 1;
    if (bv->window->c_sw) {
        bv->window->c_sw = e_sc_txt (bv->window->c_sw, bv->window->buffer);
    }
    if (bz->window->c_sw) {
        bz->window->c_sw = e_sc_txt (bz->window->c_sw, bz->window->buffer);
    }
    e_cursor (window, 1);
    e_write_screen (window, 1);
    free (str);
}

/*   copy block within window   */
int
e_block_copy (we_window_t * window)
{
    we_buffer_t *buffer;
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    if (window->ins == 8) {
        return (0);
    }
    window->save = 1;
    global_disable_add_undo = 1;
    e_copy_block (buffer->cursor.x, buffer->cursor.y, buffer, buffer, window);
    global_disable_add_undo = 0;
    e_add_undo ('c', buffer, buffer->cursor.x, buffer->cursor.y, 0);
    e_sc_txt_2 (window);
    window->save = buffer->control->maxchg + 1;
    return (0);
}

/*   copy block  */
void
e_copy_block (int x, int y, we_buffer_t * buffer_src, we_buffer_t * buffer_dst,
              we_window_t * window)
{
    we_buffer_t *buffer = window->edit_control->window[window->edit_control->mxedt]->buffer;
    we_screen_t *s_src = buffer_src->window->screen;
    we_screen_t *s_dst = buffer_dst->window->screen;
    int i, j, n = s_src->mark_end.y - s_src->mark_begin.y - 1;
    int kax = s_src->mark_begin.x, kay = s_src->mark_begin.y, kex =
            s_src->mark_end.x, key = s_src->mark_end.y;
    int kse = key, ksa = kay;
    we_string_t **str, *tmp;
    unsigned char *cstr;

    if (key < kay || (kay == key && kex <= kax)) {
        return;
    }
    if ((cstr = malloc (buffer_src->mx.x + 1)) == NULL) {
        e_error (e_msg[ERR_LOWMEM], ERROR_MSG, buffer_dst->colorset);
        return;
    }
    if (kay == key) {
        if (buffer_src == buffer_dst && y == kay && x >= kax && x < kex) {
            free (cstr);
            return;
        }
        n = kex - kax;
        buffer_dst->cursor.x = x;
        buffer_dst->cursor.y = y;
        for (i = 0; i < n; i++) {
            cstr[i] = buffer_src->buflines[key].s[kax + i];
        }
        e_ins_nchar (buffer_dst, s_dst, cstr, x, y, n);
        s_dst->mark_begin.x = x;
        s_dst->mark_end.x = buffer_dst->cursor.x = x + n;
        s_dst->mark_begin.y = s_dst->mark_end.y = buffer_dst->cursor.y = y;
        free (cstr);
        e_cursor (window, 1);
        e_write_screen (window, 1);
        return;
    }

    if (buffer_src == buffer_dst && ((y > kay && y < key) ||
                                     (y == kay && x >= kax) || (y == key
                                             && x < kex))) {
        free (cstr);
        return;
    }
    while (buffer_dst->mxlines + n > buffer_dst->mx.y - 2) {
        buffer_dst->mx.y += MAXLINES;
        if ((tmp =
                    realloc (buffer_dst->buflines,
                             buffer_dst->mx.y * sizeof (we_string_t))) == NULL) {
            e_error (e_msg[ERR_LOWMEM], SERIOUS_ERROR_MSG, buffer_dst->colorset);
        } else {
            buffer_dst->buflines = tmp;
        }
        if (buffer_dst->window->c_sw)
            buffer_dst->window->c_sw =
                realloc (buffer_dst->window->c_sw, buffer_dst->mx.y * sizeof (int));
    }
    if ((str = malloc ((n + 2) * sizeof (we_string_t *))) == NULL) {
        e_error (e_msg[ERR_LOWMEM], ERROR_MSG, buffer_dst->colorset);
        free (cstr);
        return;
    }
    e_new_line (y + 1, buffer_dst);
    if (buffer_dst == buffer_src && y < ksa) {
        kse += (n + 1);
        ksa += (n + 1);
    }
    if (*(buffer_dst->buflines[y].s + buffer_dst->buflines[y].len) != '\0') {
        (buffer_dst->buflines[y].len)++;
    }
    for (i = x; i <= buffer_dst->buflines[y].len; i++) {
        *(buffer_dst->buflines[y + 1].s + i - x) = *(buffer_dst->buflines[y].s + i);
    }
    *(buffer_dst->buflines[y].s + x) = '\0';
    buffer_dst->buflines[y].len = buffer_dst->buflines[y].nrc = x;
    buffer_dst->buflines[y + 1].len = e_str_len (buffer_dst->buflines[y + 1].s);
    buffer_dst->buflines[y + 1].nrc = strlen ((const char *) buffer_dst->buflines[y + 1].s);
    for (i = buffer_dst->mxlines; i > y; i--) {
        buffer_dst->buflines[i + n] = buffer_dst->buflines[i];
    }
    (buffer_dst->mxlines) += n;
    for (i = ksa; i <= kse; i++) {
        str[i - ksa] = &(buffer_src->buflines[i]);
    }
    for (i = 1; i <= n; i++) {
        buffer_dst->buflines[i + y].s = NULL;
    }
    for (i = 1; i <= n; i++) {
        if ((buffer_dst->buflines[i + y].s = malloc (buffer_dst->mx.x + 1)) == NULL) {
            e_error (e_msg[ERR_LOWMEM], SERIOUS_ERROR_MSG, buffer->colorset);
        }
        for (j = 0; j <= str[i]->len; j++) {
            *(buffer_dst->buflines[i + y].s + j) = *(str[i]->s + j);
        }
        if (*(str[i]->s + str[i]->len) != '\0') {
            *(buffer_dst->buflines[i + y].s + j) = '\0';
        }
        buffer_dst->buflines[i + y].len = str[i]->len;
        buffer_dst->buflines[i + y].nrc = str[i]->nrc;
    }

    for (i = 0; i < kex; i++) {
        cstr[i] = str[key - kay]->s[i];
    }
    e_ins_nchar (buffer_dst, s_dst, cstr, 0, y + key - kay, kex);
    if (*(str[0]->s + (n = str[0]->len)) != '\0') {
        n++;
    }
    for (i = 0; i < n - kax; i++) {
        cstr[i] = str[0]->s[i + kax];
    }
    cstr[n - kax] = '\0';
    e_ins_nchar (buffer_dst, s_dst, cstr, x, y, n - kax);
    s_dst->mark_begin.x = x;
    s_dst->mark_begin.y = y;
    buffer_dst->cursor.x = s_dst->mark_end.x = kex;
    buffer_dst->cursor.y = s_dst->mark_end.y = key - kay + y;

    e_cursor (window, 1);
    e_write_screen (window, 1);
    free (cstr);
    free (str);
}

/*   delete block marks   */
int
e_block_hide (we_window_t * window)
{
    we_screen_t *screen;
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    screen = window->screen;
    screen->mark_begin = e_set_pnt (0, 0);
    screen->mark_end = e_set_pnt (0, 0);
    e_write_screen (window, 1);
    return (0);
}

/*   mark begin of block   */
int
e_block_begin (we_window_t * window)
{
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    window->screen->mark_begin = window->buffer->cursor;
    e_write_screen (window, 1);
    return (0);
}

/*           Set end of block   */
int
e_block_end (we_window_t * window)
{
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    window->screen->mark_end = window->buffer->cursor;
    e_write_screen (window, 1);
    return (0);
}

/* goto begin of block   */
int
e_block_gt_beg (we_window_t * window)
{
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    window->buffer->cursor = window->screen->mark_begin;
    e_write_screen (window, 1);
    return (0);
}

/*   goto end of block   */
int
e_block_gt_end (we_window_t * window)
{
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    window->buffer->cursor = window->screen->mark_end;
    e_write_screen (window, 1);
    return (0);
}

/*   mark text line in block   */
int
e_block_mrk_all (we_window_t * window)
{
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    window->screen->mark_begin.x = 0;
    window->screen->mark_begin.y = 0;
    window->screen->mark_end.y = window->buffer->mxlines - 1;
    window->screen->mark_end.x = window->buffer->buflines[window->buffer->mxlines - 1].len;
    e_write_screen (window, 1);
    return (0);
}

/*   mark text line in block   */
int
e_block_mrk_line (we_window_t * window)
{
    int i;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    window->screen->mark_begin.x = 0;
    window->screen->mark_begin.y = window->buffer->cursor.y;
    if (window->buffer->cursor.y < window->buffer->mxlines - 1) {
        window->screen->mark_end.x = 0;
        window->screen->mark_end.y = window->buffer->cursor.y + 1;
    } else {
        window->screen->mark_end.x = window->buffer->buflines[window->buffer->cursor.y].len;
        window->screen->mark_end.y = window->buffer->cursor.y;
    }
    e_write_screen (window, 1);
    return (0);
}

/*	block: up- or downcase
mode=0 every character downcase
mode=1 every character upcase
mode=2 first character in each word upcase
mode=3 first character in each line upcase   */
int
e_block_changecase (we_window_t * window, int mode)
{
    we_buffer_t *buffer;
    we_screen_t *screen;
    int i, x, y, x_begin, x_end;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    screen = window->screen;

    for (y = screen->mark_begin.y; y <= screen->mark_end.y; y++) {
        if (y == screen->mark_begin.y) {
            x_begin = screen->mark_begin.x;
        } else {
            x_begin = 0;
        }
        if (y == screen->mark_end.y) {
            x_end = screen->mark_end.x;
        } else {
            x_end = buffer->buflines[y].len;
        }
        for (x = x_begin; x < x_end; x++)
            if (mode == 0) {
                buffer->buflines[y].s[x] = tolower (buffer->buflines[y].s[x]);
            } else if (mode == 1) {
                buffer->buflines[y].s[x] = toupper (buffer->buflines[y].s[x]);
            } else if ((mode == 2)
                       && ((x == 0) || (buffer->buflines[y].s[x - 1] == ' ')
                           || (buffer->buflines[y].s[x - 1] == '\t'))) {
                buffer->buflines[y].s[x] = toupper (buffer->buflines[y].s[x]);
            } else if ((mode == 3) && (x == 0)) {
                buffer->buflines[y].s[x] = toupper (buffer->buflines[y].s[x]);
            }
    }
    window->save++;
    e_write_screen (window, 1);
    return 0;
}

int
e_changecase_dialog (we_window_t * window)
{
    static int b_sw = 0;
    int ret;
    W_OPTSTR *o = e_init_opt_kst (window);
    if (!o) {
        return (-1);
    }
    o->xa = 21;
    o->xe = 52;
    o->ya = 4;
    o->ye = 15;

    o->bgsw = AltO;
    o->name = "Change Case";
    o->crsw = AltO;
    e_add_txtstr (4, 2, "Which mode:", o);

    e_add_pswstr (1, 5, 3, 0, AltD, 0, "Downcase          ", o);
    e_add_pswstr (0, 5, 4, 0, AltU, b_sw & 1, "Upcase            ", o);
    e_add_pswstr (0, 5, 5, 0, AltH, b_sw & 2, "Headline          ", o);
    e_add_pswstr (0, 5, 6, 0, AltE, b_sw & 3, "First character   ", o);

    e_add_bttstr (7, 9, 1, AltO, " Ok ", NULL, o);
    e_add_bttstr (18, 9, -1, WPE_ESC, "Cancel", NULL, o);
    ret = e_opt_kst (o);

    if (ret != WPE_ESC) {
        e_block_changecase (window, o->pstr[0]->num);
    }
    freeostr (o);
    return 0;
}

/*   unindent block   */
int
e_block_to_left (we_window_t * window)
{
    we_buffer_t *buffer;
    we_screen_t *s;
    int n = window->edit_control->tabn / 2, i, j, k, l, m, nn;
    unsigned char *tstr = malloc ((n + 2) * sizeof (char));

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    s = window->screen;
    for (i = 0; i <= n; i++) {
        tstr[i] = ' ';
    }
    tstr[n] = '\0';
    for (i = (!s->mark_begin.x) ? s->mark_begin.y : s->mark_begin.y + 1;
            i < s->mark_end.y || (i == s->mark_end.y && s->mark_end.x > 0); i++) {
        for (j = 0; j < buffer->buflines[i].len && isspace (buffer->buflines[i].s[j]); j++);
        for (l = j - 1, k = 0; l >= 0 && k < n; l--) {
            if (buffer->buflines[i].s[l] == ' ') {
                k++;
            } else if (buffer->buflines[i].s[l] == '\t') {
                for (nn = m = 0; m < l; m++) {
                    if (buffer->buflines[i].s[m] == ' ') {
                        nn++;
                    } else if (buffer->buflines[i].s[m] == '\t') {
                        nn += window->edit_control->tabn - (nn % window->edit_control->tabn);
                    }
                }
                k += window->edit_control->tabn - (nn % window->edit_control->tabn);
            }
        }
        l = j - l - 1;
        if (l > 0) {
            nn = s->mark_begin.x;
            e_del_nchar (buffer, s, j - l, i, l);
            if (k > n) {
                e_ins_nchar (buffer, s, tstr, j - l, i, k - n);
            }
            s->mark_begin.x = nn;
        }
    }
    free (tstr);
    e_write_screen (window, 1);
    return (0);
}

/*   indent block   */
int
e_block_to_right (we_window_t * window)
{
    we_buffer_t *buffer;
    we_screen_t *s;
    int n = window->edit_control->tabn / 2, i, j;
    unsigned char *tstr = malloc ((n + 1) * sizeof (char));

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    s = window->screen;
    for (i = 0; i < n; i++) {
        tstr[i] = ' ';
    }
    tstr[n] = '\0';
    for (i = (!s->mark_begin.x) ? s->mark_begin.y : s->mark_begin.y + 1;
            i < s->mark_end.y || (i == s->mark_end.y && s->mark_end.x > 0); i++) {
        for (j = 0; buffer->buflines[i].len && isspace (buffer->buflines[i].s[j]); j++);
        e_ins_nchar (buffer, s, tstr, j, i, n);
        if (i == s->mark_begin.y) {
            s->mark_begin.x = 0;
        }
    }
    free (tstr);
    e_write_screen (window, 1);
    return (0);
}

/*            Read block from file   */
int
e_block_read (we_window_t * window)
{
    if (window->ins == 8) {
        return (WPE_ESC);
    }
    WpeCreateFileManager (1, window->edit_control, "");
    window->save = window->edit_control->maxchg + 1;
    return (0);
}

/*   write block to file   */
int
e_block_write (we_window_t * window)
{
    WpeCreateFileManager (2, window->edit_control, "");
    return (0);
}

int
e_first_search (we_window_t * window)
{
    return e_repeat_search(window);
}

/**
 *    Find + Find again
 *
 *    Search in the current window starting on the current line for
 *    the specified search text or regular expression.
 *
 *    @return 0 if no match was found, 1 if a match was found.
 *
 *   */
int
e_repeat_search (we_window_t * window)
{
    we_screen_t *screen;
    we_buffer_t *buffer;
    FIND *find = &(window->edit_control->find);
    int i, j, iend, jend;
    int start_offset;
    size_t end_offset;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    screen = window->screen;
    if (find_global_scope(find->sw)) {
        jend = find_search_backward(find->sw) ? 0 : buffer->mxlines - 1;
        iend = find_search_backward(find->sw) ? 0 : buffer->buflines[buffer->mxlines - 1].len;
    } else {
        jend = find_search_backward(find->sw) ? screen->mark_begin.y : screen->mark_end.y;
        iend = find_search_backward(find->sw) ? screen->mark_begin.x : screen->mark_end.x;
    }
    start_offset = buffer->cursor.x;
    j = buffer->cursor.y;

    // Search the text line by line for matches
    for (;; start_offset = -1) {
        if (find_search_forward(find->sw) && j > jend) {
            break;
        } else if (find_search_backward(find->sw) && j < jend) {
            break;
        }
        if (find_search_backward(find->sw)) {
            // set search to previous line end if we are at line start
            if (start_offset == 0) {
                if (j == jend) {
                    break;		// end of file, break away
                } else {
                    j--;
                    start_offset = buffer->buflines[j].len;
                }
            }
        }
        // search through a line for the next match
        do {
            if (j == jend) {
                end_offset = iend;
            } else if (find_search_forward(find->sw)) {
                end_offset = buffer->buflines[j].len;
            } else {
                end_offset = 0;
            }
            if (find_search_backward(find->sw) && start_offset == -1) {
                start_offset = buffer->buflines[j].len;
            }
            if (!find_regular_expression(find->sw)) {
                start_offset =
                    e_strstr (start_offset, end_offset, buffer->buflines[j].s,
                              (unsigned char *) find->search,
                              &end_offset,
                              find_case_sensitive(find->sw));
            } else {
                start_offset = (start_offset >= 0) ? start_offset : 0;
                start_offset = e_rstrstr (start_offset, end_offset, buffer->buflines[j].s,
                                          (unsigned char *) find->search,
                                          &end_offset,
                                          find_case_sensitive(find->sw));
            }
            if (find_search_forward(find->sw) && j == jend && start_offset > iend) {
                break;
            } else if (find_search_backward(find->sw) && j == jend && start_offset < iend) {
                break;
            }
            if (start_offset >= 0
                    && (!find_word_boundary(find->sw)
                        || (isalnum (*(buffer->buflines[j].s + start_offset + find->sn)) == 0
                            && (start_offset == 0
                                || isalnum (*(buffer->buflines[j].s + start_offset - 1)) == 0)))) {
                find->sn = end_offset - start_offset;
                screen->fa.x = start_offset;
                buffer->cursor.y = screen->fa.y = screen->fe.y = j;
                screen->fe.x = start_offset + find->sn;
                buffer->cursor.x = start_offset
                                   = find_search_forward(find->sw) ? screen->fe.x : start_offset;
                if (!find_replace(find->sw)) {
                    e_cursor (window, 1);
                    screen->fa.y = j;
                    e_write_screen (window, 1);
                }
                return (1);
            } else if (start_offset >= 0 && find_word_boundary(find->sw)) {
                start_offset++;
            }
        } while (start_offset >= 0);
        // End 'search through aline for the next match' loop.
        if (find_search_forward(find->sw) && j > jend) {
            break;
        } else if (find_search_backward(find->sw) && j < jend) {
            break;
        }
        j = find_search_forward(find->sw) ? j + 1 : j - 1;
    }
    if (!find_replace(find->sw)) {
        e_message (0, e_msg[ERR_GETSTRING], window);
    }
    return (0);
}

/*   goto line  */
int
e_goto_line (we_window_t * window)
{
    int i, num;
    we_buffer_t *buffer;

    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    if ((num =
                e_num_kst ("Goto Line Number", buffer->cursor.y + 1, buffer->mxlines, window, 0,
                           AltG)) > 0) {
        buffer->cursor.y = num - 1;
    } else if (num == 0) {
        buffer->cursor.y = num;
    }
    e_cursor (window, 1);
    return (0);
}

int
e_find (we_window_t * window)
{
    we_screen_t *wind_screen;
    we_buffer_t *buffer;
    FIND *find = &(window->edit_control->find);
    int i, ret;
    char strTemp[80];
    W_OPTSTR *find_dialog = e_init_opt_kst (window);

    if (!find_dialog) {
        return (-1);
    }
    for (i = window->edit_control->mxedt;
            i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    wind_screen = window->screen;
    if (e_block_dup (strTemp, window)) {
        strcpy (find->search, strTemp);
        find->sn = strlen (find->search);
    }
    find_dialog->xa = 7;
    find_dialog->ya = 3;
    find_dialog->xe = 63;
    find_dialog->ye = 18;
    find_dialog->bgsw = 0;
    find_dialog->crsw = AltO;
    find_dialog->name = "Find";
    e_add_txtstr (32, 4, "Direction:", find_dialog);
    e_add_txtstr (4, 4, "Options:", find_dialog);
    e_add_txtstr (4, 9, "Scope:", find_dialog);
    e_add_txtstr (32, 9, "Begin:", find_dialog);
    e_add_wrstr (4, 2, 18, 2, 35, 128, 0, AltT, "Text to Find:", find->search,
                 &window->edit_control->sdf, find_dialog);
    e_add_sswstr (5, 5, 0, AltC, find->sw & 128 ? 1 : 0, "Case sensitive    ", find_dialog);
    e_add_sswstr (5, 6, 0, AltW, find->sw & 64 ? 1 : 0, "Whole words only  ", find_dialog);
    e_add_sswstr (5, 7, 0, AltR, find->sw & 32 ? 1 : 0, "Regular expression", find_dialog);
    e_add_pswstr (0, 33, 5, 6, AltD, 0, "ForwarD        ", find_dialog);
    e_add_pswstr (0, 33, 6, 0, AltB, find->sw & 4 ? 1 : 0, "Backward       ", find_dialog);
    e_add_pswstr (1, 5, 10, 0, AltG, 0, "Global            ", find_dialog);
    e_add_pswstr (1, 5, 11, 0, AltS, find->sw & 8 ? 1 : 0,
                  "Selected Text     ", find_dialog);
    e_add_pswstr (2, 33, 10, 0, AltF, 0, "From Cursor    ", find_dialog);
    e_add_pswstr (2, 33, 11, 0, AltE, find->sw & 2 ? 1 : 0, "Entire Scope   ", find_dialog);
    e_add_bttstr (16, 13, 1, AltO, " Ok ", NULL, find_dialog);
    e_add_bttstr (34, 13, -1, WPE_ESC, "Cancel", NULL, find_dialog);
    ret = e_opt_kst (find_dialog);
    if (ret != WPE_ESC) {
        find->sw = (find_dialog->pstr[0]->num << 2) + (find_dialog->pstr[1]->num << 3) +
                   (find_dialog->pstr[2]->num << 1) + (find_dialog->sstr[0]->num << 7) +
                   (find_dialog->sstr[1]->num << 6) + (find_dialog->sstr[2]->num << 5);
        strcpy (find->search, find_dialog->wstr[0]->txt);
        find->sn = strlen (find->search);
        if (find_entire_scope(find->sw)) {
            if (find_search_forward(find->sw)) {
                buffer->cursor.x = find_global_scope(find->sw) ? 0 : wind_screen->mark_begin.x;
                buffer->cursor.y = find_global_scope(find->sw) ? 0 : wind_screen->mark_begin.y;
            } else {
                buffer->cursor.x = find_global_scope(find->sw) ? buffer->buflines[buffer->mxlines - 1].len
                                   : wind_screen->mark_end.x;
                buffer->cursor.y = find_global_scope(find->sw) ? buffer->mxlines - 1
                                   : wind_screen->mark_end.y;
            }
        }
    }
    freeostr (find_dialog);
    if (ret != WPE_ESC) {
        e_first_search (window);
    }
    return (0);
}

/* The menu and menuhandling have been rewritten to support only 4 choices
  rather than 3 sets of 2 alternatives.
    1 Forward from Cursor
    2 Back from Cursor
    3 Global replace (forward from start of text)
    4 Selected Text (from start of selection, and only shown if a block _is_
      marked)
  The result window is either 'String NOT Found' or the number of
  occurrences found and the number actually replaced.
*/
int
e_replace (we_window_t *window)
{
    we_screen_t *screen;
    we_buffer_t *buffer;
    FIND *find = &(window->edit_control->find);
    int i, ret, c, rep = 0, found = 0;
    char strTemp[80];
    W_OPTSTR *replace_options = e_init_opt_kst (window);

    if (!replace_options) {
        return (-1);
    }
    for (i = window->edit_control->mxedt; i > 0 && !DTMD_ISTEXT (window->edit_control->window[i]->dtmd); i--);
    if (i <= 0) {
        return (0);
    }
    e_switch_window (window->edit_control->edt[i], window);
    window = window->edit_control->window[window->edit_control->mxedt];
    buffer = window->buffer;
    screen = window->screen;
    if (e_block_dup (strTemp, window)) {
        strcpy (find->search, strTemp);
        find->sn = strlen (find->search);
    }
    replace_options->xa = 7;
    replace_options->ya = 3;
    replace_options->xe = 67;
    replace_options->ye = 17;
    replace_options->bgsw = 0;
    replace_options->name = "Replace";
    replace_options->crsw = AltO;
    e_add_txtstr (4, 6, "Options:", replace_options);
    e_add_txtstr (32, 6, "Scope:", replace_options);
    e_add_wrstr (4, 2, 18, 2, 35, 128, 0, AltT, "Text to Find:", find->search,
                 &window->edit_control->sdf, replace_options);
    e_add_wrstr (4, 4, 18, 4, 35, 128, 0, AltN, "New Text:", find->replace,
                 &window->edit_control->rdf, replace_options);
    e_add_sswstr (5, 7, 0, AltC, find->sw & 128 ? 1 : 0, "Case sensitive    ", replace_options);
    e_add_sswstr (5, 8, 0, AltW, find->sw & 64 ? 1 : 0, "Whole words only  ", replace_options);
    e_add_sswstr (5, 9, 0, AltR, find->sw & 32 ? 1 : 0, "Regular expression", replace_options);
    e_add_sswstr (5, 10, 0, AltP, 1, "Prompt on Replace ", replace_options);
    e_add_pswstr (0, 33, 7, 0, AltF, 0, "Forward from Cursor", replace_options);
    e_add_pswstr (0, 33, 8, 0, AltB, find->sw & 4 ? 1 : 0,
                  "Back from Cursor   ", replace_options);
    e_add_pswstr (0, 33, 9, 0, AltG, find->sw & 2 ? 1 : 0,
                  "Global Replace     ", replace_options);
    if (screen->mark_end.y)
        e_add_pswstr (0, 33, 10, 0, AltS, find->sw & 10 ? 1 : 0,
                      "Selected Text      ", replace_options);
    e_add_bttstr (10, 12, 1, AltO, " Ok ", NULL, replace_options);
    e_add_bttstr (41, 12, -1, WPE_ESC, "Cancel", NULL, replace_options);
    e_add_bttstr (22, 12, 7, AltA, "Change All", NULL, replace_options);
    ret = e_opt_kst (replace_options);
    if (ret != WPE_ESC) {
        strcpy (find->search, replace_options->wstr[0]->txt);
        find->sn = strlen (find->search);
        strcpy (find->replace, replace_options->wstr[1]->txt);
        find->rn = strlen (find->replace);
        find->sw = 1 + (replace_options->sstr[0]->num << 7) + (replace_options->sstr[1]->num << 6)
                   + (replace_options->sstr[2]->num << 5) + (replace_options->sstr[3]->num << 4);
        switch (replace_options->pstr[0]->num) {
        case 2:
            find->sw |= 2;
            buffer->cursor.x = buffer->cursor.y = 0;
            break;
        case 1:
            find->sw |= 4;
            buffer->cursor.x = buffer->buflines[buffer->mxlines - 1].len;
            buffer->cursor.y = buffer->mxlines - 1;
            break;
        case 3:
            find->sw |= 10;
            buffer->cursor.x = screen->mark_begin.x;
            buffer->cursor.y = screen->mark_begin.y;
        default:
            break;
        }
    }
    freeostr (replace_options);
    if (ret != WPE_ESC) {
        while (e_repeat_search (window) && ((ret == AltA) || (!found))) {
            found++;
            if (window->a.y < 11) {
                screen->c.y = buffer->cursor.y - 1;
            }
            e_write_screen (window, 1);
            c = 'Y';
            if (find->sw & 16) {
                c = e_message (1, "String found:\nReplace this occurrence ?", window);
            }
            if (c == WPE_ESC) {
                break;
            }
            if (c == 'Y') {
                rep++;
                e_add_undo ('s', buffer, screen->fa.x, buffer->cursor.y, find->sn);
                global_disable_add_undo = 1;
                e_del_nchar (buffer, screen, screen->fa.x, buffer->cursor.y, find->sn);
                e_ins_nchar (buffer, screen, (unsigned char *) find->replace, screen->fa.x,
                             buffer->cursor.y, find->rn);
                global_disable_add_undo = 0;
                screen->fe.x = screen->fa.x + find->rn;
                buffer->cursor.x = !(find->sw & 4) ? screen->fe.x : screen->fa.x;
                e_write_screen (window, 1);
            }
        }
        if (found) {
            sprintf (strTemp, "Found %d\nReplaced %d", found, rep);
            e_message (0, strTemp, window);
        } else {
            e_message (0, e_msg[ERR_GETSTRING], window);
        }
    }
    return 0;
}
