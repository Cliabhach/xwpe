#ifndef __MODEL_H
#define __MODEL_H
/* model.h						  */
/* Copyright (C) 1993 Fred Kruse                          */
/* This is free software; you can redistribute it and/or  */
/* modify it under the terms of the                       */
/* GNU General Public License, see the file COPYING.      */

/*
   General Model Definitions      */

/*  System Definition   */

#define UNIX
#undef DJGPP

/*  Effects of #Defines (do not change)  */

/**
 * e_check_header(char *file, M_TIME otime, ECTN *cn, int sw)
 *  
 * is a function in we_prog.c that reads the files in edit 
 * and checks the file itself plus the included files.
 *
 * It seems to compare times of files. Not clear what the role of
 * the different files is (denoted by obuf and cbuf).
 *
 */
#define CHECKHEADER	// define to activate e_check_header functionality

#ifdef DJGPP
#define NODEBUGGER
#undef NOPROG
#define NO_XWINDOWS
#define NOPRINTER
#define NONEWSTYLE
#define MOUSE 1
#ifndef UNIX
#define UNIX
#endif
#endif

/*  XWindow Definitions  */

#if defined(DJGPP) || !defined(NO_XWINDOWS)  || defined (HAVE_LIBGPM)
#define MOUSE   1        /*  activate mouse  */
#else
#define MOUSE   0        /*  deactivate mouse  */
#endif

/*  Programming Environment  */

#ifndef NOPROG
#define PROG
#ifndef NODEBUGGER
#define DEBUGGER
#endif
#endif

/*  Newstyle only for XWindow   */
#if !defined(NO_XWINDOWS) && !defined(NONEWSTYLE)
#define NEWSTYLE
#endif

#endif
