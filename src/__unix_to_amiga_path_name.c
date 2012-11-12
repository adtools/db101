/*
 *  $VER: __unix_to_amiga_path_name.c 1.1
 *
 *  Copyright © Edgar Schwan 22.03.2009
 * 
 * (Modified for db101 by A.T.Wennermark 2012)
 */

#ifndef NDEBUG
#define NDEBUG 1
#endif

//#include "../libaos4util.h"

//#if (NEED_PATH_CONVERTION == 1)

#ifdef __amigaos4__
#define __USE_INLINE__
#endif

#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/syslimits.h>

#define DEBUG 0
#define bug printf
#if DEBUG == 1
#include <stdio.h>
#define D(x) (x);
#else
#define D(x) ;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *   __unix_to_amiga_path_name()
 */
int __unix_to_amiga_path_name(char *path, char **newpath)
{
  int result = -1;
  char *amigapath = NULL;
  LONG pos, lastpos;
  char buffer[1024];

  if (newpath) *newpath = NULL;

  if (path) {
    D(bug("\npath = '%s'\n", path)); //DEBUG
    if ((path[0] == '/') && (strlen(path) == 1)) return result; /* Pfad == rootdir ? */

    amigapath = malloc(PATH_MAX * 2); /* Speicher für temporären Speicher anfordern */
    if (amigapath) {
	//added by Alfkil:
	  memset(amigapath, 0x0, PATH_MAX*2);
      if (path[0] == '/') {                                             /* Laufwerksbezeichnung handhaben */
        lastpos = SplitName(path, '/', (STRPTR) &buffer, 1, 256);   /* Noch weitere Bestandteile vorhanden ? */
        if (lastpos != -1) {                                                /* ja */
          strcpy(amigapath, (char *) &buffer);
          }
        else {                                                              /* nein */
          strcpy(amigapath, path + 1);
          lastpos = strlen(path);
          }
        strcat(amigapath, ":");
        D(bug("Drive: '%s' (amigapath = '%s')\n", (char *) &buffer, amigapath)); //DEBUG
        }
      else lastpos = 0;

      while ((pos = SplitName(path, '/', (STRPTR) &buffer, lastpos, 256)) != -1) { /* Pfadbestandteile umwandeln */
        if (!strcmp((char *) &buffer, "..")) {
          D(bug("       handle '..'\n")); //DEBUG
          AddPart(amigapath, "/", PATH_MAX * 2);
          }
        else if (!strcmp((char *) &buffer, ".")) {
          D(bug("       skip '.'\n")); //DEBUG
          }
        else {
          D(bug("       adding '%s'\n", (char *) &buffer)); //DEBUG
          AddPart(amigapath, (STRPTR) &buffer, PATH_MAX * 2);
          }
        lastpos = pos;
        }
                                                           /* Noch ein Dateiname vorhanden ? */
      if (lastpos != strlen(path)) {                         /* ja */
        if (!strcmp((char *) (lastpos + path), "..")) {
          D(bug("       handle filename '..'\n")); //DEBUG
          AddPart(amigapath, "/", PATH_MAX * 2);
          }
        else if (!strcmp((char *) (lastpos + path), ".")) {
          D(bug("       skip filename '.'\n")); //DEBUG
          }
        else {
          D(bug("       add filename '%s'\n", (char *) (lastpos + path))); //DEBUG
          AddPart(amigapath, (STRPTR) (lastpos + path), PATH_MAX * 2);
          }
        }

      D(bug("amigapath = '%s'\n", amigapath)); //DEBUG

      if (newpath) {                                           /* Neuen Pfad duplizieren */
        if ((*newpath = strdup(amigapath)) != NULL) result = 1;
        }
      else {                                                   /* Neuen Pfad übertragen */
        if (strlen(path) >= strlen(amigapath)) {
          D(bug("overwrite path...\n")); //DEBUG
          strcpy(path, amigapath); result = 1;
          }
        }
      }
    if (amigapath) free(amigapath);                    /* temporären Speicher freigeben */
    }

  return(result);
}

#ifdef __cplusplus
}
#endif

//#endif /* NEED_PATH_CONVERTION == 1 */

#ifdef TEST

#define AU_NUM_TESTS 11

int main(int argc,char **argv)
{
  char *path[AU_NUM_TESTS];
  int i;

  path[0] = "/";
  path[1] = "/Sys";
  path[2] = "/Sys/test";
  path[3] = "/Sys/Utilities/.././Prefs/env/test";
  path[4] = "test/Hallo";
  path[5] = "..";
  path[6] = ".";
  path[7] = "./configure";
  path[8] = "../../test";
  path[9] = "/Sys/Utilities/Ghostscript/gs -sDEVICE=amiga_printer -dBATCH -dNOPAUSE Cygnix:CygnixPPC/src/X11R6.3/xc/aos4/misc/test/test.ps";
  path[10] = "/Sys/../ram";

#if 1
  printf("Test 1: (alloc)\n");
  for (i = 0; i < AU_NUM_TESTS; i++) {
    char *ptr;

    if (__unix_to_amiga_path_name(path[i], &ptr) != -1) {
      printf("mPath %d '%s' converted to '%s'm\n\n", i, (char *) path[i], ptr);
      free(ptr);
      }
    else {
      printf("convertion of path %d failed!\n\n", i);
      //exit(0);
      }
    }
#endif

#if 1
  printf("\nTest 2: (overwrite)\n");
  for (i = 0; i < AU_NUM_TESTS; i++) {
    char buf[PATH_MAX * 2];

    strcpy((char *) &buf, path[i]);
    if (__unix_to_amiga_path_name((char *) &buf, NULL) != -1) {
      printf("mPath %d '%s' converted to '%s'm\n", i, path[i], (char *) &buf);
      }
    else {
      printf("convertion of path %d failed!\n", i);
      //exit(0);
      }
    }
#endif

  return 0;
  }
#endif

