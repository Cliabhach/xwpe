/** \file we_debug.c                                       */
/* Copyright (C) 1993 Fred Kruse                          */
/* This is free software; you can redistribute it and/or  */
/* modify it under the terms of the                       */
/* GNU General Public License, see the file COPYING.      */

#include "config.h"
#include <ctype.h>
#include <string.h>
#if defined HAVE_LIBNCURSES || HAVE_LIBCURSES
#include <curses.h>
#endif
#include "keys.h"
#include "messages.h"
#include "options.h"
#include "model.h"
#include "we_control.h"
#include "edit.h"
#include "we_debug.h"
#include "we_prog.h"
#include "WeString.h"

#ifndef NO_XWINDOWS
#include "WeXterm.h"
#endif

#ifdef DEBUGGER

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

//#ifndef TERMCAP
#if defined HAVE_LIBNCURSES || defined HAVE_LIBCURSES
/* Because term.h defines "buttons" it messes up we_gpm.c if in edit.h */
#include <term.h>
#endif

#define D_CBREAK -2
#define CTRLC CtrlC
#define SVLINES 12

int e_d_delbreak (we_window_t * window);
int e_d_error (char *s);

#define MAXOUT  2 * MAXSCOL * MAXSLNS

char *e_debugger, *e_deb_swtch = NULL, *e_d_save_schirm;
int e_d_swtch = 0, rfildes[2], ofildes, e_d_pid = 0;
int e_d_nbrpts = 0, e_d_zwtchs = 0, *e_d_ybrpts, *e_d_nrbrpts;

/* number of watch expressions in Watches window */
int e_d_nwtchs = 0;

/* e_d_nrwtchs[i] is the y coordinate (count starts at 0) of the first line of
   the i-th watch in the Watches window */
int *e_d_nrwtchs;

char **e_d_swtchs;		/* e_d_swtchs[i] is the i-th watch expression (a char*) */

int e_d_nstack;
char e_d_file[128], **e_d_sbrpts;
char *e_d_out = NULL;
int e_deb_type = 0, e_deb_mode = 0;
char e_d_out_str[SVLINES][256];
char *e_d_sp[SVLINES];
char e_d_tty[80];

extern int wfildes[2], efildes[2];
extern struct termios otermio, ntermio, ttermio;
extern struct e_s_prog e_sv_prog;
extern BUFFER *e_p_w_buffer;
extern char *att_no;
extern char *e_tmp_dir;

#ifndef HAVE_TPARM
char *tparm ();
char *tgoto ();
#endif

// Disabled DEFTPUTS because it is part if stdio.h
//#ifdef DEFTPUTS
//int tputs();
//#endif

char *npipe[5] = { NULL, NULL, NULL, NULL, NULL };

char *e_d_msg[] = { "Ctrl C pressed\nQuit Debugger ?",
                    "Quit Debugger ?",
                    "End of Debugging",
                    "Program is not running",
                    "No symbol in current context\n",
                    "No Source-Code",		/*  Number 5  */
                    "Start Debugger",
                    "Can\'t start Debugger",
                    "Starting program:",
                    "Can\'t start Program",
                    "Program exited",		/*   Number 10   */
                    "Program terminated",
                    "Program received signal",
                    "Unknown Break\nQuit Debugger ?",
                    "Can\'t find file %s",
                    "Can\'t create named pipe",	/*   Number 15   */
                    "Breakpoint",
                    "(sig ",
                    "program exited",
                    "signal",
                    "interrupt",			/* Number 20  */
                    "stopped in",
                    "BREAKPOINT",
                    "Child process terminated normally",
                    "software termination",
                    "Continuing.\n",		/* Number 25  */
                    "No stack.",
                    "no process"
                  };

extern char *e_p_msg[];

int
e_deb_inp (we_window_t * window)
{
    we_control_t *control = window->ed;
    int c = 0;

    window = control->window[control->mxedt];
    c = e_getch ();
    switch (c)
    {
    case 'p':
    case ('p' - 'a' + 1):
        e_deb_out (window);
        break;
    case 'o':
    case ('o' - 'a' + 1):
        e_deb_options (window);
        break;
    case 'b':
    case ('b' - 'a' + 1):
        e_breakpoint (window);
        break;
    case 'm':
    case ('m' - 'a' + 1):
        e_remove_breakpoints (window);
        break;
    case 'a':
    case 1:
        e_remove_all_watches (window);
        break;
    case 'd':
    case ('d' - 'a' + 1):
        e_delete_watches (window);
        break;
    case 'w':
    case ('w' - 'a' + 1):
        e_make_watches (window);
        break;
    case 'e':
    case ('e' - 'a' + 1):
        e_edit_watches (window);
        break;
    case 'k':
    case ('k' - 'a' + 1):
        e_deb_stack (window);
        break;
    case 't':
    case ('t' - 'a' + 1):
        e_deb_trace (window);
        break;
    case 's':
    case ('s' - 'a' + 1):
        e_deb_next (window);
        break;
    case 'r':
    case 'c':
    case ('r' - 'a' + 1):
    case ('c' - 'a' + 1):
        e_deb_run (window);
        break;
    case 'q':
    case ('q' - 'a' + 1):
        e_d_quit (window);
        break;
    case 'u':
    case ('u' - 'a' + 1):
        e_d_goto_cursor (window);
        break;
    case 'f':
    case ('f' - 'a' + 1):
        e_d_finish_func (window);
        break;
    default:
        return (c);
    }
    return (0);
}

int
e_d_q_quit (we_window_t * window)
{
    int ret = e_message (1, e_d_msg[ERR_QUITDEBUG], window);

    if (ret == 'Y')
        e_d_quit (window);
    return (0);
}

int
e_debug_switch (we_window_t * window, int c)
{
    switch (c)
    {
    case F7:
        e_deb_trace (window);
        break;
    case F8:
        e_deb_next (window);
        break;
    case CF10:
        e_deb_run (window);
        break;
    case CF2:
        e_d_q_quit (window);
        break;
    default:
        if (window->ed->edopt & ED_CUA_STYLE)
        {
            switch (c)
            {
            case F5:
                e_breakpoint (window);
                break;
            case CF5:
                e_make_watches (window);
                break;
            case CF3:
                e_deb_stack (window);
                break;
            default:
                return (c);
            }
        }
        else
        {
            switch (c)
            {
            case CF8:
                e_breakpoint (window);
                break;
            case CF7:
                e_make_watches (window);
                break;
            case CF6:
                e_deb_stack (window);
                break;
            default:
                return (c);
            }
        }
    }
    return (0);
}

/*  Input Routines   */
/**
 * function e_e_line_read
 *
 * Reads one line until new line, end of string ('\0') or EOF.
 *
 * FIXME: find out when, why and from what source this line is read.
 * TODO: Must the string s really be signed char *? Replaced with char *. check & test.
 *
 * Returns
 * 	-1 if the read is unsuccessful
 * 	1  if e_deb_type == 1 && the last char read == '*'
 * 	   if e_deb_type == 2 && the last char read == space && (dbx) prefixes the string
 * 	   if e_deb_type == 3 && the last char read == '>'
 * 	   if e_deb_type == 0 && the last char read == space && (dbg) prefixes the string
 * 	2  otherwise
 *
 *  FIXME: find out what the returns mean exactly
 */
int
e_e_line_read (int n, char *s, int max)
{
    int i, ret = 0;

    for (i = 0; i < max - 1; i++)
    {
        ret = read (n, s + i, 1);
        if (ret != 1 || s[i] == EOF || s[i] == '\n' || s[i] == '\0')
            break;
    }
    if (ret != 1 && i == 0)
        return (-1);
    s[i + 1] = '\0';
    if (e_deb_type == 1 && s[i] == '*')
        return (1);
    if (e_deb_type == 3 && s[i] == '>')
        return (1);
    else if (e_deb_type == 2 && i > 4 && s[i] == ' '
             && !strncmp ((const char *) s + i - 5, "(dbx)", 5))
        return (1);
    else if (e_deb_type == 0 && i > 4 && s[i] == ' '
             && !strncmp ((const char *) s + i - 5, "(gdb)", 5))
        return (1);
    return (2);
}

/**
 * function e_d_line_read
 *
 * FIXME: what does this line read routine do exactly? What does it return?
 * TODO: Must the string s really be signed char *? Replaced with char *. check & test.
 *
*/
int
e_d_line_read (int n, char *s, int max, int sw, int esw)
{
    static char wt = 0, esc_sv = 0, str[12];
    int i, j, ret = 0, kbdflgs;

    if (esw)
    {
        if ((ret = e_e_line_read (efildes[0], s, max)) >= 0)
            return (ret);
    }
    else
    {
        while (e_e_line_read (efildes[0], s, max) >= 0);
    }
    for (i = 0; i < max - 1; i++)
    {
        if (esc_sv)
        {
            s[i] = WPE_ESC;
            esc_sv = 0;
            continue;
        }
        kbdflgs = fcntl (n, F_GETFL, 0);
        fcntl (n, F_SETFL, kbdflgs | O_NONBLOCK);
        while ((ret = read (n, s + i, 1)) <= 0 && i == 0 && wt >= sw)
            if (e_d_getchar () == D_CBREAK)
                return (-1);
        /*	Read until no chars are left anymore
           	Return if buffer not empty
                return(-1) if CBREAK
        */
        fcntl (n, F_SETFL, kbdflgs & ~O_NONBLOCK);
        if (ret == -1)
            break;
        else if (ret != 1 || s[i] == EOF || s[i] == '\0')
            break;
        else if (s[i] == '\n' || s[i] == WPE_CR)
            break;
        else if (s[i] == WPE_ESC)
        {
            s[i] = '\0';
            esc_sv = 1;
            break;
        }
    }
    if (ret != 1)
    {
        s[i] = '\0';
        if (e_deb_type == 1 && i > 0 && s[i - 1] == '*')
        {
            str[0] = 0;
            wt = 0;
            return (1);
        }
        else if (e_deb_type == 3 && i > 0 && s[i - 1] == '>')
        {
            str[0] = 0;
            wt = 0;
            return (1);
        }
        else if (e_deb_type == 0)
        {
            if (i > 5 && !strncmp ((const char *) s + i - 6, "(gdb) ", 6))
            {
                str[0] = 0;
                wt = 0;
                return (1);
            }
            else if (i < 6)
            {
                for (j = 0; j <= i; j++)
                    str[6 + j] = s[j];
                if (!strncmp (str + i - 6, "(gdb) ", 6))
                {
                    str[0] = '\0';
                    wt = 0;
                    return (1);
                }
            }
        }
        else if (e_deb_type == 2)
        {
            if (i > 5 && !strncmp ((const char *) s + i - 6, "(dbx) ", 6))
            {
                str[0] = 0;
                wt = 0;
                return (1);
            }
            else if (i < 6)
            {
                for (j = 0; j <= i; j++)
                    str[6 + j] = s[j];
                if (!strncmp (str + i - 6, "(dbx) ", 6))
                {
                    str[0] = '\0';
                    wt = 0;
                    return (1);
                }
            }
        }
    }
    else
    {
        s[i + 1] = '\0';
    }
    if (i != 0)
        wt = 0;
    else
        wt++;
    if (i > 4)
        for (j = 0; j < 6; j++)
            str[j] = s[j + i - 5];
    return (0);
}

int
e_d_dum_read ()
{
    char str[256];
    int ret;

    while ((ret = e_d_line_read (wfildes[0], str, 128, 0, 0)) == 0 || ret == 2)
        if (ret == 2)
            e_d_error (str);
    return (ret);
}

/* Output Routines */
int
e_d_p_exec (we_window_t * window)
{
    we_control_t *control = window->ed;
    BUFFER *b;
    int ret, i, is, j;
    char str[512];

    for (i = control->mxedt; i > 0 && strcmp (control->window[i]->datnam, "Messages"); i--)
        ;
    if (i <= 0)
    {
        e_edit (control, "Messages");
        i = control->mxedt;
    }
    window = control->window[i];
    b = control->window[i]->b;
    if (b->buflines[b->mxlines - 1].len != 0)
        e_new_line (b->mxlines, b);
    for (j = 0, i = is = b->mxlines - 1;
            (ret = e_d_line_read (wfildes[0], str, 512, 0, 0)) == 0;)
    {
        if (ret == -1)
            return (ret);
        print_to_end_of_buffer (b, str, b->mx.x);
    }
    if (ret == 1)
    {
        e_new_line (i, b);
        for (j = 0; j < num_cols_on_screen_safe(window) - 2 && str[j] != '\n' &&
                str[j] != '\0'; j++)
            *(b->buflines[i].s + j) = str[j];
        b->cursor.y = i;
        b->cursor.x = b->buflines[i].len = j;
        b->buflines[i].nrc = j;
    }
    b->cursor.y = b->mxlines - 1;
    b->cursor.x = 0;

    e_rep_win_tree (control);
    return (ret);
}

/*    Help Routines   */
int
e_d_getchar ()
{
    int i = 1, fd;
    char c = 0, kbdflgs = 0;

    if (WpeIsXwin ())
        fd = rfildes[0];
    else
        fd = 0;

    /*   if(WpeIsXwin() || e_deb_mode)    */
    {
        kbdflgs = fcntl (fd, F_GETFL, 0);
        fcntl (fd, F_SETFL, kbdflgs | O_NONBLOCK);
    }
#ifndef NO_XWINDOWS
    if (WpeIsXwin ())
        c = (*e_u_change) (NULL);
#endif
    if (c || (i = read (fd, &c, 1)) == 1)
    {
        if (c == CTRLC)
        {
            /*	 if (WpeIsXwin() || e_deb_mode)    */
            fcntl (fd, F_SETFL, kbdflgs & ~O_NONBLOCK);
            e_d_switch_out (0);
            i =
                e_message (1, e_d_msg[ERR_CTRLCPRESS],
                           global_editor_control->window[global_editor_control->mxedt]);
            if (i == 'Y')
            {
                e_d_quit (global_editor_control->window[global_editor_control->mxedt]);
                return (D_CBREAK);
            }
            else
                return (c);
        }
        else if (write (rfildes[1], &c, 1) < 1)
        {
            printf ("[e_d_getchar] writing 1 character failed.\n");
        }
    }
    /*   if (WpeIsXwin() || e_deb_mode)   */
    fcntl (fd, F_SETFL, kbdflgs & ~O_NONBLOCK);
    return (i == 1 ? c : 0);
}

int
e_d_is_watch (int c, we_window_t * window)
{
    if (strcmp (window->datnam, "Watches"))
        return (0);
    if (c == EINFG)
        return (e_make_watches (window));
    else if (c == ENTF)
        return (e_delete_watches (window));
    else
        return (0);
}

/**
 * Remark: return changed to void: no return was given and no one tested return.
 *
 * */
void
e_d_quit_basic (we_window_t * window)
{
    UNUSED (window);
    int kbdflgs;

    if (!e_d_swtch)
        return;
    if (rfildes[1] >= 0)
    {
        char *cstr = 0;
        if (e_deb_type == 0 || e_deb_type == 3)
            cstr = "q\ny\n";
        else if (e_deb_type == 1)
            cstr = "q\n";
        else if (e_deb_type == 2)
            cstr = "quit\n";
        if (cstr)
        {
            int n = strlen (cstr);
            if (n != write (rfildes[1], cstr, n))
            {
                printf ("[e_d_is_watch] failed to write %i characters: %s.\n",
                        n, cstr);
            }
        }
    }
    kbdflgs = fcntl (0, F_GETFL, 0);
    fcntl (0, F_SETFL, kbdflgs & ~O_NONBLOCK);
    e_d_swtch = 0;
    if (e_d_pid)
    {
        kill (e_d_pid, 9);
        e_d_pid = 0;
    }
    if (!WpeIsXwin ())
    {
        if (e_d_out)
            free (e_d_out);
        e_d_out = NULL;
    }
    if (rfildes[1] >= 0)
        close (rfildes[1]);
    if (wfildes[0] >= 0)
        close (wfildes[0]);
    if (efildes[0] >= 0)
        close (efildes[0]);
    if (rfildes[0] >= 0)
        close (rfildes[0]);
    if (wfildes[1] >= 0)
        close (wfildes[1]);
    if (WpeIsXwin ())
    {
        remove (npipe[0]);
        remove (npipe[1]);
        remove (npipe[2]);
        remove (npipe[3]);
        remove (npipe[4]);
    }
    else
    {
        if (efildes[1] >= 0)
            close (efildes[1]);
        if (!e_deb_mode)
            e_g_sys_end ();
        else
        {
            e_d_switch_out (1);
            fk_locate (MAXSCOL, MAXSLNS);
#if !defined(HAVE_LIBNCURSES) && !defined(HAVE_LIBCURSES)
            e_putp ("\r\n");
            e_putp (att_no);
#endif
            e_d_switch_out (0);
        }
    }
}

int
e_d_quit (we_window_t * window)
{
    we_control_t *control = window->ed;
    int i;
    e_d_quit_basic (window);
    e_d_p_message (e_d_msg[ERR_ENDDEBUG], window, 1);
    WpeMouseChangeShape (WpeEditingShape);
    e_d_delbreak (window);
    for (i = control->mxedt; i > 0; i--)
        if (!strcmp (control->window[i]->datnam, "Messages"))
        {
            e_switch_window (control->edt[i], control->window[control->mxedt]);
            break;
        }
    if (i <= 0)
        e_edit (control, "Messages");
    return (0);
}

/*    Watches   */
int
e_d_add_watch (char *str, we_window_t * window)
{
    int ret;

    ret = e_add_arguments (str, "Add Watch", window, 0, AltA, &window->ed->wdf);
    if (ret != WPE_ESC)
    {
        window->ed->wdf = e_add_df (str, window->ed->wdf);
    }
    fk_cursor (1);
    return (ret);
}

int
e_remove_all_watches (we_window_t * window)
{
    we_control_t *control = window->ed;
    int i, n;

    if (e_d_nwtchs < 1)
        return (0);
    for (n = 0; n < e_d_nwtchs; n++)
        free (e_d_swtchs[n]);
    free (e_d_swtchs);
    free (e_d_nrwtchs);
    e_d_nwtchs = 0;
    for (i = control->mxedt; i > 0; i--)
    {
        if (!strcmp (control->window[i]->datnam, "Watches"))
        {
            e_switch_window (control->edt[i], control->window[control->mxedt]);
            e_close_window (control->window[control->mxedt]);
            break;
        }
    }
    e_close_buffer (e_p_w_buffer);
    e_p_w_buffer = NULL;
    return (0);
}

int
e_delete_watches (we_window_t * window)
{
    we_control_t *control = window->ed;
    BUFFER *b = control->window[control->mxedt]->b;
    int n;

    window = control->window[control->mxedt];
    if (e_d_nwtchs < 1 || strcmp (window->datnam, "Watches"))
        return (0);
    for (n = 0; n < e_d_nwtchs && e_d_nrwtchs[n] <= b->cursor.y; n++)
        ;
    free (e_d_swtchs[n - 1]);
    for (; n < e_d_nwtchs; n++)
        e_d_swtchs[n - 1] = e_d_swtchs[n];
    e_d_nwtchs--;
    e_d_swtchs = realloc (e_d_swtchs, e_d_nwtchs * sizeof (char *));
    e_d_nrwtchs = realloc (e_d_nrwtchs, e_d_nwtchs * sizeof (int));
    e_d_p_watches (window, 0);
    return (0);
}

int
e_make_watches (we_window_t * window)
{
    char str[128];
    int i, y;

    if ((window->ed->mxedt > 0) && (strcmp (window->datnam, "Watches") == 0))
    {
        /* sets y=number of watch we're inserting */
        for (y = 0; y < e_d_nwtchs && e_d_nrwtchs[y] < window->b->cursor.y; y++)
            ;
    }
    else
        y = e_d_nwtchs;
    if (window->ed->wdf && window->ed->wdf->nr_files > 0)
        strcpy (str, window->ed->wdf->name[0]);
    else
        str[0] = '\0';
    if (e_d_add_watch (str, window))
    {
        e_d_nwtchs++;
        if (e_d_nwtchs == 1)
        {
            e_d_swtchs = malloc (sizeof (char *));
            e_d_nrwtchs = malloc (sizeof (int));
        }
        else
        {
            e_d_swtchs = realloc (e_d_swtchs, e_d_nwtchs * sizeof (char *));
            e_d_nrwtchs = realloc (e_d_nrwtchs, e_d_nwtchs * sizeof (int));
        }

        /*
           move watch number y and following up one position so that we can insert
           at position y
         */
        for (i = e_d_nwtchs - 1; i > y; i--)
        {
            e_d_swtchs[i] = e_d_swtchs[i - 1];

            /* The following instruction is pointless as e_d_nrwtchs[i] is invalidated
               by inserting the new watch and has to be recomputed by e_d_p_watches()
             */
            e_d_nrwtchs[i] = e_d_nrwtchs[i - 1];
        }
        e_d_swtchs[y] = malloc (strlen (str) + 1);	/* insert...                    */
        strcpy (e_d_swtchs[y], str);	/*       ... new watch at pos y */
        e_d_p_watches (window, 1);
        return (0);
    }
    return (-1);
}

int
e_edit_watches (we_window_t * window)
{
    BUFFER *b = window->ed->window[window->ed->mxedt]->b;
    char str[128];
    int l;

    if (strcmp (window->datnam, "Watches"))
        return (0);
    for (l = 0; l < e_d_nwtchs && e_d_nrwtchs[l] <= b->cursor.y; l++)
        ;
    if (l == e_d_nwtchs && b->buflines[b->cursor.y].len == 0)
        return (e_make_watches (window));
    strcpy (str, e_d_swtchs[l - 1]);
    if (e_d_add_watch (str, window))
    {
        e_d_swtchs[l - 1] = realloc (e_d_swtchs[l - 1], strlen (str) + 1);
        strcpy (e_d_swtchs[l - 1], str);
        e_d_p_watches (window, 0);
        return (0);
    }
    return (-1);
}

/* Among other things, e_d_p_watches() must recompute e_d_nrwtchs when
   called from e_edit_watches(),
   but has code paths that don't do this ==> possible BUG
*/
int
e_d_p_watches (we_window_t * window, int sw)
{
    we_control_t *control = window->ed;
    BUFFER *b;
    int iw, k = 0, l, ret;
    char str1[256], *str;		/* is 256 always large enough? */
    char *str2;

    e_d_switch_out (0);
    if ((e_d_swtch > 2) && (e_d_p_stack (window, 0) == -1))
        return (-1);
    /* Find the watch window */
    for (iw = control->mxedt; iw > 0 && strcmp (control->window[iw]->datnam, "Watches"); iw--);
    if (iw == 0 && !e_d_nwtchs)
    {
        /* if no watches and the mysterious iw!=0 then just repaint window tree */
        e_rep_win_tree (control);
        return (0);
    }
    else if (iw == 0)
    {
        if (e_edit (control, "Watches"))
        {
            return (-1);
        }
        else
        {
            iw = control->mxedt;
        }
    }
    window = control->window[iw];
    b = control->window[iw]->b;

    /* free all lines of BUFFER b */
    e_p_red_buffer (b);
    free (b->buflines[0].s);
    b->mxlines = 0;

    for (l = 0; l < e_d_nwtchs; l++)
    {
        str = str1;

        /* Create appropriate command for the debugger */
        if (e_deb_type == 0 || e_deb_type == 3)
        {
            sprintf (str1, "p %s\n", e_d_swtchs[l]);
        }
        else if (e_deb_type == 1)
        {
            sprintf (str1, "%s/\n", e_d_swtchs[l]);
        }
        else if (e_deb_type == 2)
        {
            sprintf (str1, "print %s\n", e_d_swtchs[l]);
        }

        /* Send command to debugger */
        if (e_d_swtch)
        {
            int n = strlen (str1);
            if (n != write (rfildes[1], str1, n))
            {
                printf ("[e_d_p_watches]: write to debugger failed for '%s'.\n",
                        str1);
            }
        }

        /* If no debugger or no response, give message of no symbol in context */
        if (!e_d_swtch
                || (ret = e_d_line_read (wfildes[0], str1, 256, 0, 0)) == 1)
        {
            strcpy (str1, e_d_msg[ERR_NOSYMBOL]);
            k = 0;
        }
        else			/* Debugger successfully returned a value */
        {
            if (ret == -1)
            {
                return (ret);	/* BUG? e_d_nrwtchs not initialized if this return is taken */
            }
            str = malloc (strlen (str1) + 1);
            strcpy (str, str1);
            while ((ret = e_d_line_read (wfildes[0], str1, 256, 0, 0)) == 0
                    || ret == 2)
            {
                if (ret == -1)
                    return (ret);	/* BUG? e_d_nrwtchs not initialized if this return is taken */
                if (ret == 2)
                    e_d_error (str1);
                str = realloc (str, (k = strlen (str)) + strlen (str1) + 1);
                if (str[k - 1] == '\n')
                    str[k - 1] = ' ';
                strcat (str, str1);
            }

            /* Find the beginning of the information (depends on debugger output
               format) */
            if (e_deb_type == 0 || e_deb_type == 2 || e_deb_type == 3)
            {
                if (e_deb_type == 3 && str[0] == '0')
                {
                    for (k = 1; str[k] != '\0' && !isspace (str[k]); k++);
                }
                else
                    for (k = 0; str[k] != '\0' && str[k] != '='; k++);
                if (str[k] == '\0')
                {
                    if (str != str1)
                        free (str);
                    str = str1;
                    strcpy (str, e_d_msg[ERR_NOSYMBOL]);
                    k = 0;
                }
                for (k++; str[k] != '\0' && isspace (str[k]); k++);
            }
            else if (e_deb_type == 1)
            {
                for (k = 0; str[k] != '\0' && str[k] != ':'; k++);
                if (str[k] == '\0')
                    k = 0;
                else
                    k++;
            }
        }

        /* Print variable name */
        for (; str[k] != '\0' && isspace (str[k]); k++);
        str2 = malloc (strlen (e_d_swtchs[l]) + strlen (str + k) + 4);
        sprintf (str2, "%s: %s", e_d_swtchs[l], str + k);

        e_d_nrwtchs[l] = b->mxlines;
        print_to_end_of_buffer (b, str2, b->mx.x);

        /* Free any allocated string */
        free (str2);
        if (str != str1)
        {
            free (str);
        }
    }

    e_new_line (b->mxlines, b);
    fk_cursor (1);
    if (sw && iw != control->mxedt)
        e_switch_window (control->edt[iw], control->window[control->mxedt]);
    else
        e_rep_win_tree (control);
    return (0);
}

int
e_p_show_watches (we_window_t * window)
{
    int i;

    for (i = window->ed->mxedt; i > 0; i--)
        if (!strcmp (window->ed->window[i]->datnam, "Watches"))
        {
            e_switch_window (window->ed->edt[i], window->ed->window[window->ed->mxedt]);
            break;
        }
    if (i <= 0 && e_edit (window->ed, "Watches"))
    {
        return (-1);
    }
    return (0);
}

/***************************************/
/***  reinitialize watches from prj  ***/
int
e_d_reinit_watches (we_window_t * window, char *prj)
{
    int i, e, g, q, r;
    char *prj2;

    for (i = window->ed->mxedt; i > 0; i--)
    {
        if (!strcmp (window->ed->window[i]->datnam, "Watches"))
        {
            e_remove_all_watches (window->ed->window[window->ed->edt[i]]);
            break;
        }
    }
    g = strlen (prj);
    prj2 = malloc (sizeof (char) * (g + 1));
    strcpy (prj2, prj);
    q = 0;
    r = 0;
    while (q < g)
    {
        e = q;
        while (prj2[e] != ';' && e < g)
            e++;
        prj2[e] = '\0';
        q = e + 1;
        r++;
    }
    e_d_nwtchs = r;
    e_d_swtchs = (char **) malloc (e_d_nwtchs * sizeof (char *));
    e_d_nrwtchs = (int *) malloc (e_d_nwtchs * sizeof (int));

    for (e = 0, q = 0; e < r; e++)
    {
        e_d_swtchs[e] = malloc (strlen (prj2 + q) + 1);
        strcpy (e_d_swtchs[e], prj2 + q);
        q += strlen (prj2 + q) + 1;
    }
    free (prj2);
    e_d_p_watches (window, 1);
    return 0;
}

/***************************************/

/*  stack   */
int
e_deb_stack (we_window_t * window)
{
    e_d_switch_out (0);
    return (e_d_p_stack (window, 1));
}

int
e_d_p_stack (we_window_t * window, int sw)
{
    we_control_t *control = window->ed;
    BUFFER *b;
    we_screen_t *s;
    int is, i, j, k, l, ret;
    char str[256];

    if (e_d_swtch < 3)
        return (e_error (e_d_msg[ERR_NOTRUNNING], 0, window->colorset));
    for (i = 0; i < SVLINES; i++)
        e_d_out_str[i][0] = '\0';
    for (is = control->mxedt; is > 0 && strcmp (control->window[is]->datnam, "Stack"); is--)
        ;
    if (!sw && is == 0)
        return (0);
    if (is == 0)
    {
        if (e_edit (control, "Stack"))
            return (-1);
        else
            is = control->mxedt;
    }
    window = control->window[is];
    b = control->window[is]->b;
    s = control->window[is]->s;
    if (!e_d_swtch)
        return (0);
    int n;
    char *cstr = 0;
    if (e_deb_type == 0)
        cstr = "bt\n";
    else if (e_deb_type == 1 || e_deb_type == 3)
        cstr = "t\n";
    else if (e_deb_type == 2)
        cstr = "where\n";
    if (cstr)
    {
        n = strlen (cstr);
        if (n != write (rfildes[1], cstr, n))
        {
            printf ("[e_d_p_stack]: write of debugger command failed: %s",
                    cstr);
        }
    }
    while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 2)
        e_d_error (str);
    if (ret == -1)
        return (-1);
    i = j = 0;
    if (ret == 1)
    {
        e_d_error (e_d_msg[ERR_PROGEXIT]);
        return (e_d_quit (window));
    }
    while (ret != 1)
    {
        k = 0;
        do
        {
            if (i >= b->mxlines)
                e_new_line (i, b);
            if ((i > 0 && j == 0
                    && *(b->buflines[i - 1].s + b->buflines[i - 1].len - 1) == '\\')
                    || (!e_deb_type && j == 0 && (k > 0 || str[k] != '#')))
            {
                for (j = 0; j < 3; j++)
                    b->buflines[i].s[j] = ' ';
            }
            for (; isspace (str[k]); k++)
                ;
            for (;
                    j < num_cols_on_screen_safe(window) - 2 && str[k] != '\n'
                    && str[k] != '\0'; j++, k++)
                *(b->buflines[i].s + j) = str[k];
            if (str[k] != '\0')
            {
                if (str[k] != '\n')
                {
                    for (l = j - 1;
                            l > 2 && !isspace (b->buflines[i].s[l])
                            && b->buflines[i].s[l] != '='; l--)
                        ;
                    if (l > 2)
                    {
                        k -= (j - l - 1);
                        for (l++; l < j; l++)
                            b->buflines[i].s[l] = ' ';
                    }
                    *(b->buflines[i].s + j) = '\\';
                    *(b->buflines[i].s + j + 1) = '\n';
                    *(b->buflines[i].s + j + 2) = '\0';
                    j++;
                }
                else
                {
                    *(b->buflines[i].s + j) = '\n';
                    *(b->buflines[i].s + j + 1) = '\0';
                }
            }
            b->buflines[i].len = j;
            b->buflines[i].nrc = j + 1;
            if (j != 0 && str[k] != '\0')
            {
                i++;
                j = 0;
            }
            else
                j--;
        }
        while (str[k] != '\n' && str[k] != '\0');
        while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 2)
            e_d_error (str);
        if (ret == -1)
            return (-1);
    }
    for (; i < b->mxlines; i++)
        e_del_line (i, b, s);
    if (sw && is != control->mxedt)
        e_switch_window (control->edt[is], control->window[control->mxedt]);
    e_rep_win_tree (control);
    return (0);
}

int
e_make_stack (we_window_t * window)
{
    char file[128], str[128], *tmpstr = malloc (1);
    int i, ret, line = 0, dif;
    BUFFER *b = window->ed->window[window->ed->mxedt]->b;
    e_d_switch_out (0);
    if (e_deb_type != 1)
    {
        tmpstr[0] = '\0';
        if (e_deb_type == 0)
        {
            for (i = dif = 0; i <= b->cursor.y; i++)
                if (b->buflines[i].s[0] == '#')
                    dif = atoi ((char *) (b->buflines[i].s + 1));
            for (i = b->cursor.y; i >= 0 && b->buflines[i].s[0] != '#'; i--);
            if (i < 0)
                return (1);
            for (; i < b->mxlines; i++)
            {
                if (!
                        (tmpstr =
                             realloc (tmpstr, strlen (tmpstr) + b->buflines[i].len + 2)))
                {
                    e_error (e_msg[ERR_LOWMEM], 0, window->colorset);
                    return (-1);
                }
                strcat (tmpstr, (char *) b->buflines[i].s);
                if (i == b->mxlines - 1 || b->buflines[i + 1].s[0] == '#')
                    break;
                else if (tmpstr[strlen (tmpstr) - 2] == '\\')
                    tmpstr[strlen (tmpstr) - 2] = '\0';
                else if (tmpstr[strlen (tmpstr) - 1] == '\n')
                    tmpstr[strlen (tmpstr) - 1] = '\0';
            }
        }
        else
        {
            for (i = 1, dif = 0; i <= b->cursor.y; i++)
                if (b->buflines[i - 1].s[b->buflines[i - 1].len - 1] != '\\')
                    dif++;
            for (i = b->cursor.y;
                    i > 0 && b->buflines[i - 1].s[b->buflines[i - 1].len - 1] == '\\'; i--);
            if (i == 0 && b->buflines[i].len == 0)
                return (1);
            for (; i < b->mxlines; i++)
            {
                if (!
                        (tmpstr =
                             realloc (tmpstr, strlen (tmpstr) + b->buflines[i].len + 2)))
                {
                    e_error (e_msg[ERR_LOWMEM], 0, window->colorset);
                    return (-1);
                }
                strcat (tmpstr, (char *) b->buflines[i].s);
                if (i == b->mxlines - 1 || b->buflines[i].s[b->buflines[i].len - 1] != '\\')
                    break;
                else if (tmpstr[strlen (tmpstr) - 2] == '\\')
                    tmpstr[strlen (tmpstr) - 2] = '\0';
                else if (tmpstr[strlen (tmpstr) - 1] == '\n')
                    tmpstr[strlen (tmpstr) - 1] = '\0';
            }
        }

        if (e_deb_type == 3 && (line = e_make_line_num2 (tmpstr, file)) < 0)
            return (e_error (e_d_msg[ERR_NOSOURCE], 0, window->colorset));
        else if (e_deb_type != 3 && (line = e_make_line_num (tmpstr, file)) < 0)
            return (e_error (e_d_msg[ERR_NOSOURCE], 0, window->colorset));
        if (dif > e_d_nstack)
            sprintf (str, "%s %d\n",
                     e_deb_type != 3 ? "up" : "down", dif - e_d_nstack);
        else if (dif < e_d_nstack)
            sprintf (str, "%s %d\n",
                     e_deb_type != 3 ? "down" : "up", e_d_nstack - dif);
        if (dif != e_d_nstack)
        {
            int n = strlen (str);
            if (n != write (rfildes[1], str, n))
            {
                printf ("[e_make_stack]: write to debugger failed: %s.\n", str);
            }
            while ((ret = e_d_line_read (wfildes[0], str, 128, 0, 0)) == 0
                    || ret == 2)
                if (ret == 2)
                    e_d_error (str);
            if (ret == -1)
                return (ret);
            e_d_nstack = dif;
        }
    }
    else if (e_deb_type == 1)
    {
        for (i = b->cursor.y; i >= 0 && (line =
                                             e_make_line_num2 ((char *) b->buflines[i].s,
                                                     file)) < 0; i--);
    }
    if (e_d_p_watches (window, 0) == -1)
        return (-1);
    e_d_goto_break (file, line, window);
    return (0);
}

/*******************************************************/
/** resyncing schirm - screen output with breakpoints **/

int
e_brk_schirm (we_window_t * window)
{
    int i;
    int n;

    we_screen_t *s = window->s;
    s->brp = realloc (s->brp, sizeof (int));
    s->brp[0] = 0;
    for (i = 0; i < e_d_nbrpts; i++)
    {
        if (!strcmp (window->datnam, e_d_sbrpts[i]))
        {
            for (n = 1; n <= (s->brp[0]); n++)
                if (e_d_ybrpts[i] == (s->brp[n]))
                    break;
            if (n > s->brp[0])
            {
                /****  New break, not in schirm  ****/
                (s->brp[0])++;
                s->brp = realloc (s->brp, (s->brp[0] + 1) * sizeof (int));
                s->brp[s->brp[0]] = e_d_ybrpts[i] - 1;
            }
        }
    }
    return 0;
}

/*****************************************/

/*******************************************/
/***  reinitialize breakpoints from prj  ***/
int
e_d_reinit_brks (we_window_t * window, char *prj)
{
    int line, e, g, q, r;
    char *p, *name, *prj2;
    /***  remove breakpoints, schirms will be synced later  ***/

    e_remove_breakpoints (window);
    g = strlen (prj);
    prj2 = malloc (sizeof (char) * (g + 1));
    strcpy (prj2, prj);
    q = 0;
    r = 0;
    while (q < g)
    {
        e = q;
        while (prj2[e] != ';' && e < g)
            e++;
        prj2[e] = '\0';
        q = e + 1;
        r++;
    }
    /**** for sure ****/
    e_d_nbrpts = 0;

    /**** allocate memory for breakpoints ****/
    e_d_sbrpts = malloc (sizeof (char *) * r);
    e_d_ybrpts = malloc (sizeof (int) * r);
    e_d_nrbrpts = malloc (sizeof (int) * r);

    name = prj2;
    for (q = 0; q < r; q++)
    {
        p = strrchr (name, ':');
        e = strlen (name);
        if (p != NULL)
        {
            *p = '\0';
            {
                p++;
                line = atoi (p);
                if (line > 0)
                {
                    /**** hopefully we have filename and line number ****/

                    e_d_ybrpts[e_d_nbrpts] = line;
                    e_d_sbrpts[e_d_nbrpts] =
                        malloc (sizeof (char) * (strlen (name) + 1));
                    strcpy (e_d_sbrpts[e_d_nbrpts], name);
                    e_d_nbrpts++;

                    /**** needed to keep schirm in sync ****/

                    for (g = window->ed->mxedt; g > 0; g--)
                        if (!strcmp (window->ed->window[g]->datnam, name))
                        {
                            e_brk_schirm (window->ed->window[g]);
                        }
                }
            }
        }
        name += e + 1;
    }
    free (prj2);
    return 0;
}


/**** Recalculate breakpoints , because of line/block
    deleting/adding ****/
int
e_brk_recalc (we_window_t * window, int start, int len)
{
    we_control_t *control = window->ed;
    BUFFER *b;
    int n, rend, count, yline;
    int *br_lines;

    if ((len == 0) || (control == NULL))
        return 1;
    b = control->window[control->mxedt]->b;

    rend = start - 1 + abs (len);
    yline = b->cursor.y;

    /**** deleting removed breakpoints ****/
    if (len < 0)
    {
        for (n = 0; n < e_d_nbrpts; n++)
            if ((!strcmp (window->datnam, e_d_sbrpts[n])) &&
                    (e_d_ybrpts[n] <= (rend + 1)) && (e_d_ybrpts[n] >= (start + 1)))
            {
                b->cursor.y = e_d_ybrpts[n] - 1;
                e_make_breakpoint (window, 0);
            }
    }

    /**** scanning for breakpoints to move ****/
    for (count = 0, n = 0; n < e_d_nbrpts; n++)
        if ((!strcmp (window->datnam, e_d_sbrpts[n]))
                && (e_d_ybrpts[n] >= (start + 1)))
            count++;
    if (count == 0)
        return 1;
    br_lines = (int *) malloc (sizeof (int) * count);
    for (n = 0, count = 0; n < e_d_nbrpts; n++)
        if ((!strcmp (window->datnam, e_d_sbrpts[n]))
                && (e_d_ybrpts[n] >= (start + 1)))
        {
            br_lines[count++] = e_d_ybrpts[n];
        }

    /**** moving breakpoints ****/
    for (n = 0; n < count; n++)
    {
        b->cursor.y = br_lines[n] - 1;
        e_make_breakpoint (window, 0);
    }
    for (n = 0; n < count; n++)
    {
        b->cursor.y = br_lines[n] + len - 1;
        e_make_breakpoint (window, 0);
    }
    b->cursor.y = yline;
    free (br_lines);
    return 0;
}

/*****************************************/

/*  Breakpoints   */
int
e_breakpoint (we_window_t * window)
{
    return (e_make_breakpoint (window, 0));
}

int
e_remove_breakpoints (we_window_t * window)
{
    we_control_t *control = window->ed;
    int i;

    if (e_d_swtch)
    {
        int n;
        char *cstr = 0;
        if (!e_deb_type)
            cstr = "d\ny\n";
        else if (e_deb_type == 1)
            cstr = "D\n";
        else if (e_deb_type == 2)
            cstr = "delete all\n";
        else if (e_deb_type == 3)
            cstr = "db *\n";
        if (cstr)
        {
            n = strlen (cstr);
            if (n != write (rfildes[1], cstr, n))
            {
                printf
                ("[e_remove_breakpoints]: Write to debugger failed for %s.\n",
                 cstr);
            }
        }
    }
    for (i = 0; i < e_d_nbrpts; i++)
        free (e_d_sbrpts[i]);
    for (i = control->mxedt; i >= 0; i--)
        if (DTMD_ISTEXT (control->window[i]->dtmd))
            control->window[i]->s->brp[0] = 0;
    e_d_nbrpts = 0;
    if (e_d_sbrpts)
    {
        free (e_d_sbrpts);
        e_d_sbrpts = NULL;
    }
    if (e_d_ybrpts)
    {
        free (e_d_ybrpts);
        e_d_ybrpts = NULL;
    }
    if (e_d_nrbrpts)
    {
        free (e_d_nrbrpts);
        e_d_nrbrpts = NULL;
    }
    e_rep_win_tree (control);
    return (0);
}

int
e_mk_brk_main (we_window_t * window, int sw)
{
    UNUSED (window);
    int i, ret;
    char eing[128], str[256];

    if (sw)
    {
        if (e_d_swtch)
        {
            if (e_deb_type == 0)
                sprintf (eing, "d %d\n", e_d_nrbrpts[sw - 1]);
            else if (e_deb_type == 2)
                sprintf (eing, "delete %d\n", e_d_nrbrpts[sw - 1]);
            else if (e_deb_type == 3)
                sprintf (eing, "db %d\n", e_d_nrbrpts[sw - 1]);
            else if (e_deb_type == 1)
            {
                sprintf (eing, "e main\n");
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_mk_brk_main]: write to debugger failed for %s.\n",
                     eing);
                }
                if (e_d_dum_read () == -1)
                    return (-1);
                sprintf (eing, "%d d\n", e_d_ybrpts[sw - 1]);
            }
            int n = strlen (eing);
            if (n != write (rfildes[1], eing, n))
            {
                printf ("[e_mk_brk_main]: Write to debugger failed for: %s.\n",
                        eing);
            }
            if (e_d_dum_read () == -1)
                return (-1);
        }
        free (e_d_sbrpts[sw - 1]);
        for (i = sw - 1; i < e_d_nbrpts - 1; i++)
        {
            e_d_sbrpts[i] = e_d_sbrpts[i + 1];
            e_d_ybrpts[i] = e_d_ybrpts[i + 1];
            e_d_nrbrpts[i] = e_d_nrbrpts[i + 1];
        }
        e_d_nbrpts--;
        if (e_d_nbrpts == 0)
        {
            free (e_d_sbrpts);
            e_d_sbrpts = NULL;
            free (e_d_ybrpts);
            e_d_ybrpts = NULL;
            free (e_d_nrbrpts);
            e_d_nrbrpts = NULL;
        }
    }
    else
    {
        e_d_nbrpts++;
        if (e_d_nbrpts == 1)
        {
            e_d_sbrpts = malloc (sizeof (char *));
            e_d_ybrpts = malloc (sizeof (int));
            e_d_nrbrpts = malloc (sizeof (int));
        }
        else
        {
            e_d_sbrpts = realloc (e_d_sbrpts, e_d_nbrpts * sizeof (char *));
            e_d_ybrpts = realloc (e_d_ybrpts, e_d_nbrpts * sizeof (int));
            e_d_nrbrpts = realloc (e_d_nrbrpts, e_d_nbrpts * sizeof (int));
        }
        e_d_sbrpts[e_d_nbrpts - 1] = malloc (1);
        if (e_d_swtch)
        {
            if (e_deb_type == 0)
            {
                sprintf (eing, "b main\n");
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf ("[]: Write to debugger failed for %s.\n", eing);
                }
                while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                        && strncmp (str, "Breakpoint", 10))
                    ;
                if (ret == -1)
                    return (ret);
                if (ret == 2)
                    e_d_error (str);
                e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 11);
                if (ret != 1 && e_d_dum_read () == -1)
                    return (-1);
            }
            else if (e_deb_type == 2)
            {
                sprintf (eing, "stop in main\n");
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_mk_brk_main]: Write to debugger failed for %s.\n",
                     eing);
                }
                while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                        && str[0] != '(')
                    ;
                if (ret == -1)
                    return (ret);
                if (ret == 2)
                    e_d_error (str);
                e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 1);
                if (ret != 1 && e_d_dum_read () == -1)
                    return (-1);
            }
            else if (e_deb_type == 3)
            {
                sprintf (eing, "b main\n");
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_mk_brk_main]: Write to debugger failed for %s.\n",
                     eing);
                }
                while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                        && strncmp (str, "Added:", 6))
                    ;
                ret = e_d_line_read (wfildes[0], str, 256, 0, 0);
                if (ret == -1)
                    return (ret);
                if (ret == 2)
                    e_d_error (str);
                e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 1);
                if (ret != 1 && e_d_dum_read () == -1)
                    return (-1);
            }
            else if (e_deb_type == 1)
            {
                sprintf (eing, "e main\n");
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_mk_brk_main]: write to debugger failed for %s.\n",
                     eing);
                }
                if (e_d_dum_read () == -1)
                    return (-1);
                sprintf (eing, "b\n");
                n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_mk_brk_main]: Write to debugger failed for %s.\n",
                     eing);
                }
                if ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == -1)
                    return (ret);
                if (ret == 2)
                    e_d_error (str);
                if (ret != 1)
                {
                    for (i = 0; str[i] && str[i] != ':'; i++)
                        ;
                    if (str[i])
                        e_d_ybrpts[e_d_nbrpts - 1] = atoi (str + i + 1);
                    if (e_d_dum_read () == -1)
                        return (-1);
                }
            }
        }
    }
    return (sw ? 0 : e_d_nbrpts);
}

int
e_make_breakpoint (we_window_t * window, int sw)
{
    we_control_t *control = window->ed;
    we_screen_t *s = control->window[control->mxedt]->s;
    BUFFER *b = control->window[control->mxedt]->b;
    int ret, i;
    char eing[128], str[256];

    if (!sw)
    {
        if (!e_check_c_file (window->datnam))
            return (e_error (e_p_msg[ERR_NO_CFILE], 0, window->colorset));
        for (i = 0; i < s->brp[0] && s->brp[i + 1] != b->cursor.y; i++)
            ;
        if (i < s->brp[0])
        {
            for (i++; i < s->brp[0]; i++)
                s->brp[i] = s->brp[i + 1];
            (s->brp[0])--;
            for (i = 0; i < e_d_nbrpts && (strcmp (e_d_sbrpts[i], window->datnam) ||
                                           e_d_ybrpts[i] != b->cursor.y + 1); i++)
                ;
            if (e_d_swtch)
            {
                if (e_deb_type == 0)
                    sprintf (eing, "d %d\n", e_d_nrbrpts[i]);
                else if (e_deb_type == 2)
                    sprintf (eing, "delete %d\n", e_d_nrbrpts[i]);
                else if (e_deb_type == 3)
                    sprintf (eing, "db %d\n", e_d_nrbrpts[i]);
                else if (e_deb_type == 1)
                {
                    sprintf (eing, "e %s\n", e_d_sbrpts[i]);
                    int n = strlen (eing);
                    if (n != write (rfildes[1], eing, n))
                    {
                        printf
                        ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                         eing);
                    }
                    if (e_d_dum_read () == -1)
                        return (-1);
                    sprintf (eing, "%d d\n", e_d_ybrpts[i]);
                }
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                     eing);
                }
                if (e_d_dum_read () == -1)
                    return (-1);
            }
            free (e_d_sbrpts[i]);
            for (; i < e_d_nbrpts - 1; i++)
            {
                e_d_sbrpts[i] = e_d_sbrpts[i + 1];
                e_d_ybrpts[i] = e_d_ybrpts[i + 1];
                e_d_nrbrpts[i] = e_d_nrbrpts[i + 1];
            }
            e_d_nbrpts--;
            if (e_d_nbrpts == 0)
            {
                free (e_d_sbrpts);
                e_d_sbrpts = NULL;
                free (e_d_ybrpts);
                e_d_ybrpts = NULL;
                free (e_d_nrbrpts);
                e_d_nrbrpts = NULL;
            }
        }
        else
        {
            e_d_nbrpts++;
            if (e_d_nbrpts == 1)
            {
                e_d_sbrpts = malloc (sizeof (char *));
                e_d_ybrpts = malloc (sizeof (int));
                e_d_nrbrpts = malloc (sizeof (int));
            }
            else
            {
                e_d_sbrpts = realloc (e_d_sbrpts, e_d_nbrpts * sizeof (char *));
                e_d_ybrpts = realloc (e_d_ybrpts, e_d_nbrpts * sizeof (int));
                e_d_nrbrpts = realloc (e_d_nrbrpts, e_d_nbrpts * sizeof (int));
            }
            e_d_sbrpts[e_d_nbrpts - 1] = malloc (strlen (window->datnam) + 1);
            strcpy (e_d_sbrpts[e_d_nbrpts - 1], window->datnam);
            e_d_ybrpts[e_d_nbrpts - 1] = b->cursor.y + 1;
            if (e_d_swtch)
            {
                if (e_deb_type == 0)
                {
                    sprintf (eing, "b %s:%d\n", window->datnam, b->cursor.y + 1);
                    int n = strlen (eing);
                    if (n != write (rfildes[1], eing, n))
                    {
                        printf
                        ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                         eing);
                    }
                    while ((ret =
                                e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                            && strncmp (str, "Breakpoint", 10))
                        ;
                    if (ret == -1)
                        return (ret);
                    if (ret == 2)
                        e_d_error (str);
                    e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 11);
                    if (ret != 1 && e_d_dum_read () == -1)
                        return (-1);
                }
                else if (e_deb_type == 2)
                {
                    sprintf (eing, "stop at \"%s\":%d\n", window->datnam,
                             b->cursor.y + 1);
                    int n = strlen (eing);
                    if (n != write (rfildes[1], eing, n))
                    {
                        printf
                        ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                         eing);
                    }
                    while ((ret =
                                e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                            && str[0] != '(')
                        ;
                    if (ret == -1)
                        return (ret);
                    if (ret == 2)
                        e_d_error (str);
                    e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 1);
                    if (ret != 1 && e_d_dum_read () == -1)
                        return (-1);
                }
                else if (e_deb_type == 3)
                {
                    sprintf (eing, "b %s:%d\n", window->datnam, b->cursor.y + 1);
                    int n = strlen (eing);
                    if (n != write (rfildes[1], eing, n))
                    {
                        printf
                        ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                         eing);
                    }
                    while ((ret =
                                e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                            && strncmp (str, "Added:", 6))
                        ;
                    ret = e_d_line_read (wfildes[0], str, 256, 0, 0);
                    if (ret == -1)
                        return (ret);
                    if (ret == 2)
                        e_d_error (str);
                    e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 1);
                    if (ret != 1 && e_d_dum_read () == -1)
                        return (-1);
                }
                else if (e_deb_type == 1)
                {
                    sprintf (eing, "e %s\n", window->datnam);
                    int n = strlen (eing);
                    if (n != write (rfildes[1], eing, n))
                    {
                        printf
                        ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                         eing);
                    }
                    if (e_d_dum_read () == -1)
                        return (-1);
                    sprintf (eing, "%d b\n", b->cursor.y + 1);
                    n = strlen (eing);
                    if (n != write (rfildes[1], eing, n))
                    {
                        printf
                        ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                         eing);
                    }
                    if (e_d_dum_read () == -1)
                        return (-1);
                }
            }
            (s->brp[0])++;
            s->brp = realloc (s->brp, (s->brp[0] + 1) * sizeof (int));
            s->brp[s->brp[0]] = b->cursor.y;
        }
    }
    else
    {
        if (e_deb_type == 0)
        {
            for (i = 0; i < e_d_nbrpts; i++)
            {
                sprintf (eing, "b %s:%d\n", e_d_sbrpts[i], e_d_ybrpts[i]);
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                     eing);
                }
                while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                        && strncmp (str, "Breakpoint", 10))
                    ;
                if (ret == -1)
                    return (ret);
                if (ret == 2)
                    e_d_error (str);
                e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 11);
                if (ret != 1 && e_d_dum_read () == -1)
                    return (-1);
            }
        }
        else if (e_deb_type == 2)
        {
            for (i = 0; i < e_d_nbrpts; i++)
            {
                sprintf (eing, "stop at \"%s\":%d\n", e_d_sbrpts[i],
                         e_d_ybrpts[i]);
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                     eing);
                }
                while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                        && str[0] != '(')
                    ;
                if (ret == -1)
                    return (ret);
                if (ret == 2)
                    e_d_error (str);
                e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 1);
                if (ret != 1 && e_d_dum_read () == -1)
                    return (-1);
            }
        }
        else if (e_deb_type == 3)
        {
            for (i = 0; i < e_d_nbrpts; i++)
            {
                sprintf (eing, "b %s:%d\n", window->datnam, b->cursor.y + 1);
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                     eing);
                }
                while ((ret = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 0
                        && strncmp (str, "Added:", 6))
                    ;
                ret = e_d_line_read (wfildes[0], str, 256, 0, 0);
                if (ret == -1)
                    return (ret);
                if (ret == 2)
                    e_d_error (str);
                e_d_nrbrpts[e_d_nbrpts - 1] = atoi (str + 1);
                if (ret != 1 && e_d_dum_read () == -1)
                    return (-1);
            }
        }
        else
        {
            for (i = 0; i < e_d_nbrpts; i++)
            {
                sprintf (eing, "e %s\n", e_d_sbrpts[i]);
                int n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                     eing);
                }
                if (e_d_dum_read () == -1)
                    return (-1);
                sprintf (eing, "%d b\n", e_d_ybrpts[i]);
                n = strlen (eing);
                if (n != write (rfildes[1], eing, n))
                {
                    printf
                    ("[e_make_breakpoint]: Write to debugger failed for %s.\n",
                     eing);
                }
                if (e_d_dum_read () == -1)
                    return (-1);
            }
        }
    }
    e_schirm (window, 1);
    return (1);
}

/*   start Debugger   */
int
e_exec_deb (we_window_t * window, char *prog)
{
    int i;

    if (e_d_swtch)
        return (1);
    e_d_swtch = 1;
    fflush (stdout);
    if (WpeIsXwin ())
    {
        for (i = 0; i < 5; i++)
        {
            if (npipe[i])
                free (npipe[i]);
            npipe[i] = malloc (128);
            sprintf (npipe[i], "%s/we_pipe%d", e_tmp_dir, i);
            remove (npipe[i]);
        }
        if (mkfifo (npipe[0], S_IRUSR | S_IWUSR) < 0 ||
                mkfifo (npipe[1], S_IRUSR | S_IWUSR) < 0 ||
                mkfifo (npipe[2], S_IRUSR | S_IWUSR) < 0 ||
                mkfifo (npipe[3], S_IRUSR | S_IWUSR) < 0 ||
                mkfifo (npipe[4], S_IRUSR | S_IWUSR) < 0)
        {
            e_error (e_d_msg[ERR_CANTPIPE], 0, window->colorset);
            return (0);
        }
    }
    else
    {
        if (pipe (rfildes))
        {
            e_error (e_p_msg[ERR_PIPEOPEN], 0, window->colorset);
            return (0);
        }
        if (pipe (wfildes))
        {
            e_error (e_p_msg[ERR_PIPEOPEN], 0, window->colorset);
            return (0);
        }
        if (pipe (efildes))
        {
            e_error (e_p_msg[ERR_PIPEOPEN], 0, window->colorset);
            return (0);
        }
    }

    if ((e_d_pid = fork ()) > 0)
    {
        if (WpeIsXwin ())
        {
            if ((wfildes[0] = open (npipe[1], O_RDONLY)) < 0)
            {
                e_error (e_p_msg[ERR_PIPEOPEN], 0, window->colorset);
                return (0);
            }
            for (i = 0;
                    i < 80 && read (wfildes[0], &e_d_tty[i], 1) == 1
                    && e_d_tty[i] != '\0' && e_d_tty[i] != '\n'; i++)
                ;
            if (e_d_tty[i] == '\n')
                e_d_tty[i] = '\0';
            if ((rfildes[0] = open (e_d_tty, O_RDONLY)) < 0 ||
                    (wfildes[1] = open (e_d_tty, O_WRONLY)) < 0)
            {
                e_error (e_p_msg[ERR_PIPEOPEN], 0, window->colorset);
                return (0);
            }
            if ((rfildes[1] = open (npipe[0], O_WRONLY)) < 0 ||
                    (wfildes[0] = open (npipe[1], O_RDONLY)) < 0 ||
                    (efildes[0] = open (npipe[2], O_RDONLY)) < 0)
            {
                e_error (e_p_msg[ERR_PIPEOPEN], 0, window->colorset);
                return (0);
            }
            if (e_deb_mode)
            {
                tcgetattr (rfildes[0], &ntermio);
                /*   ioctl(rfildes[0], TCGETA, &ntermio);*/
                ntermio.c_iflag = 0;
                ntermio.c_oflag = 0;
                ntermio.c_lflag = 0;
                ntermio.c_cc[VMIN] = 1;
                ntermio.c_cc[VTIME] = 0;
#ifdef VSWTCH
                ntermio.c_cc[VSWTCH] = 0;
#endif
                /*   ioctl(rfildes[0], TCSETA, &ntermio);*/
                tcsetattr (rfildes[0], TCSADRAIN, &ntermio);
            }
        }
        else
        {
            FILE *fpp;

            if (!(fpp = popen ("tty", "r")))
            {
                e_error (e_p_msg[ERR_PIPEOPEN], 0, window->colorset);
                return (0);
            }
            if (!fgets (e_d_tty, 80, fpp))
            {
                printf ("[e_exec_deb]: reading 80 chars from tty failed.\n");
            }
            pclose (fpp);
        }
        return (wfildes[1]);
    }
    else if (e_d_pid < 0)
    {
        e_error (e_p_msg[ERR_PROCESS], 0, window->colorset);
        return (0);
    }

#ifndef NO_XWINDOWS
    if (WpeIsXwin ())
    {
        FILE *fp;
        char file[128];

        sprintf (file, "%s/we_sys", e_tmp_dir);
        fp = fopen (file, "w+");
        fprintf (fp, "#!/bin/sh\n");
        fprintf (fp, "tty > %s\n", npipe[1]);
        if (!e_deb_swtch)
            fprintf (fp,
                     "%s %s < %s > %s 2> %s\necho type \\<Return\\> to continue\nread i\n",
                     e_debugger, prog, npipe[0], npipe[1], npipe[2]);
        else
            fprintf (fp,
                     "%s %s %s < %s > %s 2> %s\necho type \\<Return\\> to continue\nread i\n",
                     e_debugger, e_deb_swtch, prog, npipe[0], npipe[1], npipe[2]);
        fprintf (fp, "rm -window %s\n", file);
        fclose (fp);
        chmod (file, 0755);

        execlp (XTERM_CMD, XTERM_CMD, "+sb", "-geometry", "80x25-0-0", "-e",
                user_shell, "-c", file, NULL);
        remove (file);
    }
    else
#endif
    {
        int kbdflgs;

        close (0);
        if (fcntl (rfildes[0], F_DUPFD, 0) != 0)
        {
            fprintf (stderr, e_p_msg[ERR_PIPEEXEC], rfildes[0]);
            exit (1);
        }
        close (1);
        if (fcntl (wfildes[1], F_DUPFD, 1) != 1)
        {
            fprintf (stderr, e_p_msg[ERR_PIPEEXEC], wfildes[1]);
            exit (1);
        }
        close (2);
        if (fcntl (efildes[1], F_DUPFD, 2) != 2)
        {
            fprintf (stderr, e_p_msg[ERR_PIPEEXEC], efildes[1]);
            exit (1);
        }
        kbdflgs = fcntl (1, F_GETFL, 0);
        fcntl (1, F_SETFL, kbdflgs | O_NONBLOCK);
        kbdflgs = fcntl (2, F_GETFL, 0);
        fcntl (2, F_SETFL, kbdflgs | O_NONBLOCK);
        if (!e_deb_swtch)
            execlp (e_debugger, e_debugger, prog, NULL);
        else
            execlp (e_debugger, e_debugger, e_deb_swtch, prog, NULL);
    }
    fprintf (stderr, "%s %s %s\n", e_p_msg[ERR_IN_COMMAND], e_debugger, prog);
    exit (1);
}

int
e_start_debug (we_window_t * window)
{
    we_control_t *control = window->ed;
    int i, file;
    char estr[128];

    efildes[0] = efildes[1] = -1;
    wfildes[0] = wfildes[1] = -1;
    rfildes[0] = rfildes[1] = -1;
    if (e_d_swtch)
        return (0);
    /*    e_copy_prog(&e_sv_prog, &e_prog);  */
    if (e_p_make (window))
        return (-1);
    if (!e__project)
    {
        for (i = control->mxedt; i > 0; i--)
            if (e_check_c_file (control->window[i]->datnam))
                break;
        if (i > 0)
            strcpy (e_d_file, control->window[i]->datnam);
    }
    else
        strcpy (e_d_file, control->window[control->mxedt - 1]->datnam);
    window = control->window[control->mxedt - 1];
    for (i = 0; i < SVLINES; i++)
    {
        e_d_sp[i] = e_d_out_str[i];
        e_d_out_str[i][0] = '\0';
    }
    if (e_deb_type == 1)
    {
        e_debugger = "sdb";
        e_deb_swtch = NULL;
    }
    else if (e_deb_type == 2)
    {
        e_debugger = "dbx";
        e_deb_swtch = "-i";
    }
    else if (e_deb_type == 3)
    {
        e_debugger = "xdb";
        e_deb_swtch = "-L";
    }
    else
    {
        e_debugger = "gdb";
        e_deb_swtch = NULL;
    }
    e_d_pid = 0;
    if (e_test_command (e_debugger))
    {
        sprintf (estr, "Debugger \'%s\' not in Path", e_debugger);
        e_error (estr, 0, window->colorset);
        return (-1);
    }
    (*e_u_sys_ini) ();
    if (e__project && (file = e_exec_deb (window, e_s_prog.exe_name)) == 0)
    {
        (*e_u_sys_end) ();
        return (-2);
    }
    else if (!e__project)
    {
        if (e_s_prog.exe_name && e_s_prog.exe_name[0])
        {
            strcpy (estr, e_s_prog.exe_name);
        }
        else
        {
            strcpy (estr, window->datnam);
            WpeStringCutChar (estr, '.');
            strcat (estr, ".e");
        }
        if ((file = e_exec_deb (window, estr)) == 0)
        {
            (*e_u_sys_end) ();
            return (-2);
        }
    }
    (*e_u_sys_end) ();
    e_d_p_message (e_d_msg[ERR_STARTDEBUG], window, 1);
    WpeMouseChangeShape (WpeDebuggingShape);
    if (control->mxedt > 1)
        e_switch_window (control->edt[control->mxedt - 1], control->window[control->mxedt]);
    return (0);
}

int
e_run_debug (we_window_t * window)
{
    we_control_t *control = window->ed;
    int kbdflgs, ret;

    if (e_d_swtch < 1 && (ret = e_start_debug (window)) < 0)
        return (ret);
    if (e_d_swtch < 2)
    {
        kbdflgs = fcntl (efildes[0], F_GETFL, 0);
        fcntl (efildes[0], F_SETFL, kbdflgs | O_NONBLOCK);
        kbdflgs = fcntl (wfildes[0], F_GETFL, 0);
        fcntl (wfildes[0], F_SETFL, kbdflgs | O_NONBLOCK);

        if (e_d_dum_read () == -1)
            return (-1);
        if (e_deb_type == 3)
        {
            char *cstr = "sm\n";
            int n = strlen (cstr);
            if (n != write (rfildes[1], cstr, n))
            {
                printf ("[e_run_debug]: write to debugger failed for %s.\n",
                        cstr);
            }
            if (e_d_dum_read () == -1)
                return (-1);
        }
        if (e_make_breakpoint (control->window[control->mxedt], 1) == -1)
            return (-1);
        e_d_swtch = 2;
    }
    return (0);
}

/*  Run  */
int
e_deb_run (we_window_t * window)
{
    we_control_t *control = window->ed;
    char eing[256];
    int ret, len, prsw = 0;

    if (e_d_swtch < 2 && (ret = e_run_debug (window)) < 0)
    {
        e_d_quit (window);
        if (ret == -1)
        {
            e_show_error (0, window);
            return (ret);
        }
        return (e_error (e_d_msg[ERR_CANTDEBUG], 0, window->colorset));
    }
    for (ret = 0; isspace (e_d_tty[ret]); ret++)
        ;
    if (e_d_tty[ret] != DIRC)
    {
        e_d_quit (window);
        sprintf (eing, "tty error: %s", e_d_tty);
        return (e_d_error (eing));
    }
    if (e_deb_type == 2)
    {
        if (e_d_swtch < 3)
        {
            if (e_prog.arguments)
                sprintf (eing, "run %s > %s\n", e_prog.arguments, e_d_tty);
            else
                sprintf (eing, "run > %s\n", e_d_tty);
        }
        else
        {
            strcpy (eing, "cont\n");
            prsw = 1;
        }
    }
    else
    {
        if (e_d_swtch < 3)
        {
            if (e_prog.arguments)
                sprintf (eing, "r %s > %s\n", e_prog.arguments, e_d_tty);
            else
                sprintf (eing, "r > %s\n", e_d_tty);
        }
        else
        {
            strcpy (eing, "c\n");
            prsw = 1;
        }
    }
    window = control->window[control->mxedt];
    e_d_nstack = 0;
    e_d_delbreak (window);
    e_d_switch_out (1);
    int n = strlen (eing);
    if (n != write (rfildes[1], eing, n))
    {
        printf ("[e_deb_run]: write to debugger failed for %s.\n", eing);
    }
    if (e_deb_type == 0 || ((e_deb_type == 2 || e_deb_type == 3) && !prsw))
    {
        while ((ret = e_d_line_read (wfildes[0], eing, 256, 0, 0)) == 2 ||
                !eing[0] || (!e_deb_type && prsw
                             && ((len = (strlen (eing) - 12)) < 0
                                 || strcmp (eing + len, e_d_msg[ERR_CONTINUE])))
                || (e_d_swtch < 3
                    &&
                    ((!e_deb_type && strncmp (e_d_msg[ERR_STARTPROG], eing, 17))
                     || (e_deb_type == 2 && strncmp ("Running:", eing, 8)))))
        {
            if (ret == 2)
                e_d_error (eing);
            else if (ret < 0)
                return (e_d_quit (window));
        }
    }
    if (!prsw)
        e_d_p_message (eing, window, 0);
    if (e_d_swtch < 3
            && ((!e_deb_type && strncmp (e_d_msg[ERR_STARTPROG], eing, 17))
                || (e_deb_type == 2 && strncmp ("Running:", eing, 8))))
    {
        e_d_quit (window);
        return (e_error (e_d_msg[ERR_CANTPROG], 0, window->colorset));
    }
    e_d_swtch = 3;
    return (e_read_output (window));
}

int
e_deb_trace (we_window_t * window)
{
    return (e_d_step_next (window, 0));
}

int
e_deb_next (we_window_t * window)
{
    return (e_d_step_next (window, 1));
}

int
e_d_step_next (we_window_t * window, int sw)
{
    int ret, main_brk = 0;

    if (e_d_swtch < 2 && (ret = e_run_debug (window)) < 0)
    {
        e_d_quit (window);
        if (ret == -1)
        {
            e_show_error (0, window);
            return (ret);
        }
        return (e_error (e_d_msg[ERR_CANTDEBUG], 0, window->colorset));
    }
    if (e_d_swtch < 3)
    {
        if ((main_brk = e_mk_brk_main (window, 0)) < -1)
            return (main_brk);
        ret = e_deb_run (window);
        e_mk_brk_main (window, main_brk);
        return (ret);
    }
    e_d_delbreak (window);
    e_d_switch_out (1);
    char *cstr = 0;
    if (sw && e_deb_type == 0)
        cstr = "n\n";
    else if (sw && (e_deb_type == 1 || e_deb_type == 3))
        cstr = "S\n";
    else if (sw && e_deb_type == 2)
        cstr = "next\n";
    else if (e_deb_type == 2)
        cstr = "step\n";
    else
        cstr = "s\n";
    if (cstr)
    {
        int n = strlen (cstr);
        if (n != write (rfildes[1], cstr, n))
        {
            printf ("[e_d_step_next]: Write to debugger for %s failed.\n",
                    cstr);
        }
    }
    e_d_nstack = 0;
    return (e_read_output (window));
}

int
e_d_goto_func (we_window_t * window, int flag)
{
    we_control_t *control = window->ed;
    BUFFER *b = control->window[control->mxedt]->b;
    int ret = 0, main_brk = 0;
    char str[128];

    if (e_deb_type != 0)		/* if gdb */
        return 0;
    if (e_d_swtch < 2 && (ret = e_run_debug (window)) < 0)
    {
        e_d_quit (window);
        if (ret == -1)
        {
            e_show_error (0, window);
            return (ret);
        }
        return (e_error (e_d_msg[ERR_CANTDEBUG], 0, window->colorset));
    }
    if (e_d_swtch < 3)
    {
        if ((main_brk = e_mk_brk_main (window, 0)) < -1)
            return (main_brk);
        ret = e_deb_run (window);
        e_mk_brk_main (window, main_brk);
        return (ret);
    }
    e_d_delbreak (window);
    e_d_switch_out (1);
    switch (flag)
    {
    case 'U':
        sprintf (str, "until %d\n", b->cursor.y + 1);
        break;
    case 'F':
        sprintf (str, "finish\n");
        break;
    default:
        *str = 0;
        break;
    }
    e_d_nstack = 0;
    if (*str)
    {
        int n = strlen (str);
        if (n != write (rfildes[1], str, n))
        {
            printf ("[e_d_goto_func]: write to debugger failed for %s.\n", str);
        }
        ret = e_read_output (window);
        /* Executing Finish twice may not work properly. */
    }
    return ret;
}

int
e_d_goto_cursor (we_window_t * window)
{
    return e_d_goto_func (window, 'U');
}

int
e_d_finish_func (we_window_t * window)
{
    return e_d_goto_func (window, 'F');
}

int
e_d_fst_check (we_window_t * window)
{
    int i, j, k = 0, l, ret = 0;

    e_d_switch_out (0);
    for (i = 0; i < SVLINES - 1; i++)
    {
        if ((e_deb_type != 2 && !strncmp (e_d_sp[i], e_d_msg[ERR_PROGEXIT], 14))
                ||
                ((e_deb_type == 2
                  && !strncmp (e_d_sp[i], e_d_msg[ERR_PROGEXIT2], 14))
                 || (e_deb_type == 3
                     && !strncmp (e_d_sp[i], e_d_msg[ERR_NORMALTERM],
                                  strlen (e_d_msg[ERR_NORMALTERM])))))
        {
            e_d_error (e_d_sp[i]);	/*  Program exited   */
            e_d_quit (window);
            return (i);
        }
        else if ((e_deb_type == 0 || e_deb_type == 1)
                 && !strncmp (e_d_sp[i], e_d_msg[ERR_PROGTERM], 18))
        {
            e_error (e_d_msg[ERR_PROGTERM], 0, window->colorset);
            e_d_quit (window);
            return (i);
        }
        else if (e_deb_type == 0
                 && !strncmp (e_d_sp[i], e_d_msg[ERR_PROGSIGNAL], 23))
        {
            e_d_pr_sig (e_d_sp[i], window);
            return (i);
        }
        else if (e_deb_type == 3
                 && !strncmp (e_d_sp[i], e_d_msg[ERR_SOFTTERM],
                              strlen (e_d_msg[ERR_SOFTTERM])))
        {
            e_d_pr_sig (e_d_sp[i], window);
            return (i);
        }
        else if (e_deb_type == 2
                 && (!strncmp (e_d_sp[i], e_d_msg[ERR_SIGNAL], 6)
                     || !strncmp (e_d_sp[i], e_d_msg[ERR_INTERRUPT], 9)))
        {
            e_d_pr_sig (e_d_sp[i], window);
            return (i);
        }
        else if (e_deb_type == 1 && strstr (e_d_sp[i], e_d_msg[ERR_SIGNAL]))
        {
            e_d_pr_sig (e_d_sp[i], window);
            return (i);
        }
        else if (e_deb_type == 3 && i == SVLINES - 2
                 && strstr (e_d_sp[i], ": "))
        {
            e_d_pr_sig (e_d_sp[i - 1], window);
            return (i - 1);
        }
        else if (!strncmp (e_d_sp[i], e_d_msg[ERR_BREAKPOINT], 10) ||
                 (e_deb_type == 1
                  && !strncmp (e_d_sp[i], e_d_msg[ERR_BREAKPOINT2], 10)))
        {
            if (!e_deb_type)
            {
                for (j = SVLINES - 2; j > i; j--)	/*  Breakpoint   */
                {
                    if ((ret = atoi (e_d_sp[j])) > 0)
                        for (k = 0; e_d_sp[j][k] && isdigit (e_d_sp[j][k]); k++)
                            ;
                    if (e_d_sp[j][k] == '\t')
                        break;
                }
                if (j > i)
                {
                    for (k = strlen (e_d_sp[j - 1]);
                            k >= 0 && e_d_sp[j - 1][k] != ':'; k--)
                        ;
                    if (k >= 0 && atoi (e_d_sp[j - 1] + k + 1) == ret)
                    {
                        if (e_make_line_num (e_d_sp[j - 1], e_d_sp[SVLINES - 1])
                                >= 0)
                            strcpy (e_d_file, e_d_sp[SVLINES - 1]);
                    }
                    if (e_d_p_watches (window, 0) == -1)
                        return (-1);
                    e_d_goto_break (e_d_file, ret, window);
                    return (i > 0 ? i - 1 : 0);
                }
            }
            else if (e_deb_type == 1)	/*  Breakpoint   */
            {
                if (!strncmp (e_d_sp[i] + 10, " at", 3) &&
                        (ret =
                             e_make_line_num (e_d_sp[i + 1], e_d_sp[SVLINES - 1])) >= 0)
                {
                    strcpy (e_d_file, e_d_sp[SVLINES - 1]);
                    if (e_d_p_watches (window, 0) == -1)
                        return (-1);
                    e_d_goto_break (e_d_file, ret, window);
                    return (i);
                }
            }
        }
        else if (e_deb_type == 2
                 && !strncmp (e_d_sp[i], e_d_msg[ERR_STOPPEDIN], 10))
        {
            for (j = i + 1; j < SVLINES - 1; j++)	/*  Breakpoint   */
            {
                for (k = 0; e_d_sp[j][k] == ' ' && e_d_sp[j][k] != '\0'; k++)
                    ;
                if ((ret = atoi (e_d_sp[j] + k)) > 0)
                {
                    if (!strstr (e_d_sp[j - 1], " line "))
                        break;
                    for (k = strlen (e_d_sp[j - 1]);
                            k >= 0 && e_d_sp[j - 1][k] != '\"'; k--)
                        ;
                    for (k--; k >= 0 && e_d_sp[j - 1][k] != '\"'; k--)
                        ;
                    if (k >= 0)
                    {
                        for (k++, l = 0;
                                e_d_sp[j - 1][k] != '\0'
                                && e_d_sp[j - 1][k] != '\"'; k++, l++)
                            e_d_file[l] = e_d_sp[j - 1][k];
                        e_d_file[l] = '\0';
                    }
                    if (e_d_p_watches (window, 0) == -1)
                        return (-1);
                    e_d_goto_break (e_d_file, ret, window);
                    return (i);
                }
            }
        }
    }
    return (-2);
}

int
e_d_snd_check (we_window_t * window)
{
    int i, j, k, ret;

    e_d_switch_out (0);
    for (i = SVLINES - 2; i >= 0; i--)
    {
        if (!e_deb_type && (ret = atoi (e_d_sp[i])) > 0)
        {
            for (k = 0; e_d_sp[i][k] && isdigit (e_d_sp[i][k]); k++)
                ;
            if (e_d_sp[i][k] != '\t')
                continue;
            if (i > 0)
            {
                for (k = strlen (e_d_sp[i - 1]);
                        k >= 0 && e_d_sp[i - 1][k] != ':'; k--)
                    ;
                if (k >= 0 && atoi (e_d_sp[i - 1] + k + 1) == ret)
                {
                    i--;
                    if (e_make_line_num (e_d_sp[i], e_d_sp[SVLINES - 1]) >= 0)
                        strcpy (e_d_file, e_d_sp[SVLINES - 1]);
                    do
                    {
                        for (; k >= 0 && e_d_sp[i][k] != ')'; k--)
                            ;
                        if (k < 0)
                        {
                            i--;
                            k = strlen (e_d_sp[i]);
                        }
                        else
                            break;
                    }
                    while (i >= 0);
                    do
                    {
                        for (j = 1, k--; k >= 0 && j > 0; k--)
                        {
                            if (e_d_sp[i][k] == ')')
                                j++;
                            else if (e_d_sp[i][k] == '(')
                                j--;
                        }
                        if (k < 0)
                        {
                            i--;
                            k = strlen (e_d_sp[i]);
                        }
                    }
                    while (i >= 0 && j > 0);
                    if (k == 0 && i > 0)
                    {
                        i--;
                        k = strlen (e_d_sp[i]);
                    }
                    for (k--; k >= 0 && isspace (e_d_sp[i][k]); k--)
                        ;
                    if (k < 0 && i > 0)
                        i--;
                }
            }
            if (e_d_p_watches (window, 0) == -1)
                return (-1);
            e_d_goto_break (e_d_file, ret, window);
            return (i);
        }
        else if (e_deb_type == 1 && (ret = atoi (e_d_sp[i])) > 0)
        {
            for (; i > 0; i--)
            {
                for (j = strlen (e_d_sp[i - 1]) - 1;
                        j >= 0 && isspace (e_d_sp[i - 1][j]); j--)
                    ;
                if (j < 0)
                {
                    i--;
                    continue;
                }
                for (j--; j >= 0 && !isspace (e_d_sp[i - 1][j]); j--)
                    ;
                if (j < 0)
                    i--;
                for (j--; j >= 0 && isspace (e_d_sp[i - 1][j]); j--)
                    ;
                if (j < 0)
                    i--;
                for (j--; j >= 0 && !isspace (e_d_sp[i - 1][j]); j--)
                    ;
                if (!strncmp (e_d_sp[i - 1] + j + 1, "in ", 3))
                {
                    strcpy (e_d_file, e_d_sp[i - 1] + j + 4);
                    for (k = i + 2; !e_d_file[0]; k++)
                        strcpy (e_d_file, e_d_sp[i - 1] + j + 4);
                    for (k = strlen (e_d_file) - 1;
                            k >= 0 && isspace (e_d_file[k]); k--)
                        ;
                    e_d_file[k + 1] = '\0';
                    if (e_d_p_watches (window, 0) == -1)
                        return (-1);
                    e_d_goto_break (e_d_file, ret, window);
                    return (i);
                }
            }
            return (-2);
        }
        else if (e_deb_type == 3 && i == SVLINES - 2 &&
                 (ret = e_make_line_num (e_d_sp[i], e_d_sp[SVLINES - 1])) >= 0)
        {
            strcpy (e_d_file, e_d_sp[SVLINES - 1]);
            if (e_d_p_watches (window, 0) == -1)
                return (-1);
            e_d_goto_break (e_d_file, ret, window);
            return (i);
        }
    }
    return (-2);
}

int
e_d_trd_check (we_window_t * window)
{
    int ret;
    char str[256];

    str[0] = '\0';
    e_d_switch_out (0);
    if ((ret = e_d_pr_sig (str, window)) == -1)
        return (-1);
    else if (ret == -2)
        e_d_error (e_d_msg[ERR_NOSOURCE]);
    return (0);
}

int
e_read_output (we_window_t * window)
{
    char *spt;
    int i, ret;

    for (i = 0; i < SVLINES; i++)
    {
        e_d_sp[i] = e_d_out_str[i];
        e_d_out_str[i][0] = '\0';
    }
    while ((ret =
                e_d_line_read (wfildes[0], e_d_sp[SVLINES - 1], 256, 0, 0)) == 2)
        e_d_error (e_d_sp[SVLINES - 1]);
    if (ret < 0)
        return (-1);
    e_d_switch_out (0);
    while (ret != 1)
    {
        spt = e_d_sp[0];
        for (i = 1; i < SVLINES; i++)
            e_d_sp[i - 1] = e_d_sp[i];
        e_d_sp[SVLINES - 1] = spt;
        do
        {
            while ((ret =
                        e_d_line_read (wfildes[0], e_d_sp[SVLINES - 1], 256, 0,
                                       0)) == 2)
                e_d_error (e_d_sp[SVLINES - 1]);
            if (ret < 0)
                return (-1);
        }
        while (!ret && !*e_d_sp[SVLINES - 1]);
    }
    if ((i = e_d_fst_check (window)) == -1)
        return (-1);
    if (i < 0 && (i = e_d_snd_check (window)) == -1)
        return (-1);
    if (i < 0 && (i = e_d_trd_check (window)) == -1)
        return (-1);
    if (i < 0)
    {
        e_d_switch_out (0);
        i = e_message (1, e_d_msg[ERR_UNKNOWNBRK], window);
        if (i == 'Y')
            return (e_d_quit (window));
    }
    return (0);
}

int
e_d_pr_sig (char *str, we_window_t * window)
{
    int i, line = -1, ret = 0;
    char file[128], str2[256];

    if (str && str[0])
        e_d_error (str);
    for (i = 0; i < SVLINES; i++)
        e_d_out_str[i][0] = '\0';
    if (e_deb_type != 1 && e_deb_type != 3)
    {
        char *cstr = "where\n";
        int n = strlen (cstr);
        if (n != write (rfildes[1], cstr, n))
        {
            printf ("[e_d_pr_sig]: write to debugger failed for %s.\n", cstr);
        }
        for (i = 0; ((ret = e_d_line_read (wfildes[0], str, 256, 0, 1)) == 0 &&
                     (line = e_make_line_num (str, file)) < 0) || ret == 2; i++)
        {
            if (!strncmp (str, e_d_msg[ERR_NOSTACK], 9))
            {
                e_error (e_d_msg[ERR_PROGEXIT], 0, window->colorset);
                while (ret == 0 || ret == 2)
                    if ((ret = e_d_line_read (wfildes[0], str2, 256, 0, 0)) == 2)
                        e_d_error (str2);
                e_d_quit (window);
                return (0);
            }
            else if (ret == 2)
                e_d_error (str);
        }
    }
    else
    {
        char *cstr = "t\n";
        int n = strlen (cstr);
        if (n != write (rfildes[1], cstr, n))
        {
            printf ("[e_d_pr_sig]: write to debugger failed for %s.\n", cstr);
        }
        for (i = 0; ((ret = e_d_line_read (wfildes[0], str, 256, 0, 1)) == 0 &&
                     (line = e_make_line_num2 (str, file)) < 0) || ret == 2;
                i++)
        {
            if (!strncmp (str, e_d_msg[ERR_NOPROCESS], 10))
            {
                e_error (e_d_msg[ERR_PROGEXIT], 0, window->colorset);
                while (ret == 0 || ret == 2)
                    if ((ret = e_d_line_read (wfildes[0], str2, 256, 0, 0)) == 2)
                        e_d_error (str2);
                e_d_quit (window);
                return (0);
            }
            else if (ret == 2)
                e_d_error (str);
        }
    }
    if (ret == 1 && i == 0)
    {
        e_d_error (e_d_msg[ERR_PROGEXIT]);
        return (e_d_quit (window));
    }
    while (ret == 0 || ret == 2)
        if ((ret = e_d_line_read (wfildes[0], str2, 256, 0, 0)) == 2)
            e_d_error (str2);
    if (ret == -1)
        return (ret);
    if (line >= 0)
    {
        strcpy (e_d_file, file);
        e_d_goto_break (file, line, window);
        return (0);
    }
    else
        return (-2);
}

int
e_make_line_num (char *str, char *file)
{
    char *sp;
    int i, n, num;

    if (e_deb_type == 0)
    {
        for (n = strlen (str); n >= 0 && str[n] != ':'; n--)
            ;
        if (n < 0)
            return (-1);
        for (i = n - 1; i >= 0 && !isspace (str[i]); i--)
            ;
        for (n = i + 1; str[n] != ':'; n++)
            file[n - i - 1] = str[n];
        file[n - i - 1] = '\0';
        return (atoi (str + n + 1));
    }
    else if (e_deb_type == 2)
    {
        if (!(sp = strstr (str, " line ")))
            return (-1);
        if (!(num = atoi (sp + 6)))
            return (-1);
        for (i = 6; sp[i] != '\"'; i++)
            ;
        sp += (i + 1);
        for (i = 0; (file[i] = sp[i]) != '\"' && file[i] != '\0'; i++)
            ;
        if (file[i] == '\0')
            return (-1);
        file[i] = '\0';
        return (num);
    }
    else if (e_deb_type == 3)
    {
        for (sp = str, i = 0; (file[i] = sp[i]) && sp[i] != ':'; i++)
            ;
        if (!sp[i])
            return (-1);
        file[i] = '\0';
        for (i++; sp[i] && sp[i] != ':'; i++)
            ;
        if (!sp[i])
            return (-1);
        for (i++; sp[i] && isspace (sp[i]); i++)
            ;
        if (!isdigit (sp[i]))
            return (-1);
        sp += i;
        return (atoi (sp));
    }
    else
    {
        for (i = 0; str[i] != '\0' && str[i] != ':'; i++)
            ;
        if ((!str[i]) || (num = atoi (str + i + 1)) < 0)
            return (-1);
        char *cstr = "e\n";
        n = strlen (cstr);
        if (n != write (rfildes[1], cstr, n))
        {
            printf ("[e_make_line_num]: Write to debugger failed for %s.\n",
                    cstr);
        }
        while ((i = e_d_line_read (wfildes[0], str, 256, 0, 0)) == 2)
            e_d_error (str);
        if (i < 0)
            return (-1);
        for (i = 0; str[i] != '\0' && str[i] != '\"'; i++)
            ;
        for (sp = str + i + 1, i = 0; (file[i] = sp[i]) && file[i] != '\"'; i++)
            ;
        file[i] = '\0';
        if (e_d_dum_read () == -1)
            return (-1);
        return (num);
    }
}

int
e_make_line_num2 (char *str, char *file)
{
    char *sp;
    int i;

    for (i = 0; str[i] != '[' && str[i] != '\0'; i++)
        ;
    if (!str[i])
        return (-1);
    for (sp = str + i + 1, i = 0; (file[i] = sp[i]) != ':' && file[i] != '\0';
            i++)
        ;
    if (file[i] == '\0')
        return (-1);
    file[i] = '\0';
    for (i++; isspace (sp[i]); i++)
        ;
    return (atoi (sp + i));
}

int
e_d_goto_break (char *file, int line, we_window_t * window)
{
    we_control_t *control = window->ed;
    BUFFER *b;
    we_screen_t *s;
    we_window_t ftmp;
    int i;
    char str[120];

    /*   if(schirm != e_d_save_schirm) e_d_switch_out(0);  */
    e_d_switch_out (0);
    ftmp.ed = control;
    ftmp.colorset = window->colorset;
    WpeFilenameToPathFile (file, &ftmp.dirct, &ftmp.datnam);
    for (i = 0; i < SVLINES; i++)
        e_d_out_str[i][0] = '\0';
    for (i = control->mxedt; i > 0; i--)
        if (!strcmp (control->window[i]->datnam, ftmp.datnam) &&
                !strcmp (control->window[i]->dirct, ftmp.dirct))
        {
            /*  for(j = 0; j <= control->mxedt; j++)
               if(!strcmp(control->window[j]->datnam, "Stack"))
               {  if(control->window[i]->e.x > 2*MAXSCOL/3-1) control->window[i]->e.x = 2*MAXSCOL/3-1;
               break;
               }  */
            e_switch_window (control->edt[i], control->window[control->mxedt]);
            break;
        }
    window = control->window[control->mxedt];
    free (ftmp.dirct);
    free (ftmp.datnam);
    if (i <= 0)
    {
        if (access (file, F_OK))
        {
            sprintf (str, e_d_msg[ERR_CANTFILE], file);
            return (e_error (str, 0, window->colorset));
        }
        if (e_edit (control, file))
            return (WPE_ESC);
        b = control->window[control->mxedt]->b;
        s = control->window[control->mxedt]->s;
    }
    window = control->window[control->mxedt];
    b = control->window[control->mxedt]->b;
    s = control->window[control->mxedt]->s;
    s->da.y = b->cursor.y = line - 1;
    s->da.x = b->cursor.x = 0;
    s->de.x = MAXSCOL;
    e_schirm (window, 1);
    e_cursor (window, 1);
    return (0);
}

int
e_d_delbreak (we_window_t * window)
{
    we_control_t *control = window->ed;
    int i;

    for (i = control->mxedt; i >= 0; i--)
        if (DTMD_ISTEXT (control->window[i]->dtmd))
            control->window[i]->s->da.y = -1;
    e_rep_win_tree (control);
    e_refresh ();
    return (0);
}

int
e_d_error (char *s)
{
    int len;

    e_d_switch_out (0);
    if (s[(len = strlen (s) - 1)] == '\n')
        s[len] = '\0';
    return (e_error (s, 0, global_editor_control->colorset));
}

int
e_d_putchar (int c)
{
    if (!WpeIsXwin ())
        c = fk_putchar (c);
    else
    {
        char cc = c;
        c = write (wfildes[1], &cc, 1);
    }
    return (c);
}

int
e_deb_options (we_window_t * window)
{
    int ret;
    W_OPTSTR *o = e_init_opt_kst (window);

    if (!o)
        return (-1);
    o->xa = 20;
    o->ya = 4;
    o->xe = 60;
    o->ye = 13;
    o->bgsw = 0;
    o->name = "Debug-Options";
    o->crsw = AltO;
    e_add_txtstr (4, 2, "Debugger:", o);
    e_add_txtstr (20, 2, "Mode:", o);
    e_add_pswstr (0, 5, 3, 0, AltG, 0, "Gdb    ", o);
    e_add_pswstr (0, 5, 4, 0, AltS, 0, "Sdb    ", o);
#ifdef XDB
    e_add_pswstr (0, 5, 5, 0, AltX, e_deb_type == 3 ? 2 : e_deb_type, "Xdb    ",
                  o);
#else
    e_add_pswstr (0, 5, 5, 0, AltD, e_deb_type, "Dbx    ", o);
#endif
    e_add_pswstr (1, 21, 3, 0, AltN, 0, "Normal     ", o);
    e_add_pswstr (1, 21, 4, 0, AltF, e_deb_mode, "Full Screen", o);
    e_add_bttstr (10, 7, 1, AltO, " Ok ", NULL, o);
    e_add_bttstr (25, 7, -1, WPE_ESC, "Cancel", NULL, o);
    ret = e_opt_kst (o);
    if (ret != WPE_ESC)
    {
#ifdef XDB
        e_deb_type = o->pstr[0]->num == 2 ? 3 : o->pstr[0]->num;
#else
        e_deb_type = o->pstr[0]->num;
#endif
        e_deb_mode = o->pstr[1]->num;
    }
    freeostr (o);
    return (0);
}

int
e_g_sys_ini ()
{
    if (!e_d_swtch || e_deb_mode)
        return (0);
    tcgetattr (0, &ttermio);
    return (tcsetattr (0, TCSADRAIN, &otermio));
}

int
e_g_sys_end ()
{
    if (!e_d_swtch || e_deb_mode)
        return (0);
    return (tcsetattr (0, TCSADRAIN, &ttermio));
}

int
e_test_command (char *str)
{
    int i = -1, k;
    char tmp[256], *path = getenv ("PATH");

    if (!path)
        return (-2);
    do
    {
        for (i++, k = 0; (tmp[k] = path[i]) && path[i] != ':'; k++, i++)
            ;
        if (k == 0)
        {
            tmp[0] = '.';
            k++;
        }
        tmp[k] = '/';
        tmp[k + 1] = '\0';
        strcat (tmp, str);
        if (!access (tmp, X_OK))
            return (0);
    }
    while (path[i]);
    return (-1);
}
#endif
