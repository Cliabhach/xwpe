#ifndef WE_FL_UNIX_H
#define WE_FL_UNIX_H

/** \file we_file_unix.h */

#include "config.h"
#include "we_block.h"
#include "we_debug.h"
#include "we_file_fkt.h"
#include "we_mouse.h"
#include "we_opt.h"
#include "we_wind.h"

#ifdef UNIX

int e_data_eingabe(we_control_t* control);

char* WpeGetCurrentDir(we_control_t* control);
struct dirfile* WpeCreateWorkingDirTree(int sw, we_control_t* control);
char* WpeAssemblePath(char* pth, struct dirfile* cd, struct dirfile* dd,
                      int n, we_window_t* window);
struct dirfile* WpeGraphicalFileList(struct dirfile* df, int sw, we_control_t* control);
struct dirfile* WpeGraphicalDirTree(struct dirfile* cd, struct dirfile* dd,
                                    we_control_t* control);
int e_funct(we_window_t* window);
int e_funct_in(we_window_t* window);
int e_data_first(int sw, we_control_t* control, char* nstr);
int e_data_schirm(we_window_t* window);

#endif

int e_ed_man(unsigned char* str, we_window_t* window);
int e_rename(char* file, char* newname, we_window_t* window);
int e_copy(char* file, char* newname, we_window_t* window);
int e_link(char* file, char* newname, we_window_t* window);
int e_duplicate(char* file, we_window_t* window);

int WpeManager(we_window_t* window);
int WpeManagerFirst(we_window_t* window);
int WpeHandleFileManager(we_control_t* control);
int WpeCreateFileManager(int sw, we_control_t* control, char* dirct);
int WpeFileManagerOptions(we_window_t* window);
int WpeDrawFileManager(we_window_t* window);
int WpeFindWindow(we_window_t* window);
int WpeGrepWindow(we_window_t* window);
int WpePrintFile(we_window_t* window);
int WpeShowWastebasket(we_window_t* window);
int WpeDelWastebasket(we_window_t* window);
int WpeQuitWastebasket(we_window_t* window);
int WpeSaveAsManager(we_window_t* window);
int WpeExecuteManager(we_window_t* window);
int WpeRenameCopy(char* file, char* newname, we_window_t* window, int sw);
int WpeShell(we_window_t* window);

#endif