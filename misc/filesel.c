/*
 * This file is part of nanoglk.
 *
 * Copyright (C) 2012 by Sebastian Geerken
 *
 * Nanoglk is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The highly ideosyncratic file selection dialog. See "README" for informations
 * how to use it, and "nano_input_file" on details how to instanciate it. Some
 * general remarks:
 *
 * - Currently suited to Unix-like systems.
 * - It is assumed that filenames are encoded as UTF-8.
 * - Nice to have: dealing properly with "~user~" (see comment in
 *   handle_anything()).
 */

#include "misc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>

/* ----------------------------------------------------------------------
   Simple operations on directory entries.
   ---------------------------------------------------------------------- */

struct dir_entry
{
   enum { DE_TYPE_DIR, DE_TYPE_FILE } type;
   Uint16 *name; // Unicode, as expected by TTF_RenderUNICODE*.
};

/*
 * Used for qsort(3).
 */
static int cmp_files(const void *p1, const void *p2)
{
   struct dir_entry *de1 = (struct dir_entry*)p1;
   struct dir_entry *de2 = (struct dir_entry*)p2;
   int n = de1->type - de2->type;
   return n == 0 ? strcmp16(de1->name, de2->name) : n;
}

/*
 * Reads the directory "path". Return an array of "struct dir_entry", and
 * the length in "num_files".
 */
static struct dir_entry *read_files(char *path, int *num_files)
{
   *num_files = 0;
   int  num_files_all = 8;
   struct dir_entry *files =
      (struct dir_entry*)nano_malloc(num_files_all * sizeof(struct dir_entry));

   DIR *dir = opendir(path);
   struct dirent *de;
   do {
      if((de = readdir(dir)) &&
         (strcmp(de->d_name, "..") == 0 || de->d_name[0] != '.')) {
         if(*num_files >= num_files_all) {
            num_files_all <<= 1;
            files = (struct dir_entry*)
               realloc(files, num_files_all * sizeof(struct dir_entry));
         }          

         int is_dir, type_det;
#if defined(_DIRENT_HAVE_D_TYPE) && defined(_BSD_SOURCE)
         type_det = de->d_type != DT_UNKNOWN && de->d_type != DT_LNK;
         if(type_det)
            is_dir = de->d_type == DT_DIR;
#else
         type_det = 0;
#endif
         if(!type_det) {
            char filepath[FILENAME_MAX + 1];
            strcpy(filepath, path);
            if(strcmp(path, "/") != 0)
               strcat(filepath, "/");
            strcat(filepath, de->d_name);
            struct stat sb;
            stat(filepath, &sb);
            is_dir = S_ISDIR(sb.st_mode);
         }
         
         Uint16 *name = strdup16fromutf8(de->d_name);
         if(name) {
            files[*num_files].type = is_dir ? DE_TYPE_DIR : DE_TYPE_FILE;
            files[*num_files].name = name;
            (*num_files)++;
         } // TODO else: broken file name
      }
   } while(de);
   closedir(dir);

   qsort(files, *num_files, sizeof(struct dir_entry), cmp_files);

   return files;
}

/*
 * Filter an of "struct dir_entry", "all_files", with length "num_all_files".
 * Return those entry starting with "filter". The length of the result is
 * returned in "num_files".
 *
 * "filter" may be NULL; in this case, any file is accepted, so a copy is
 * returned.
 */
static struct dir_entry *filter_files(struct dir_entry *all_files,
                                      int num_all_files, int *num_files,
                                      Uint16 *filter)
{
   *num_files = 0;
   int  num_files_all = 8;
   struct dir_entry *files =
      (struct dir_entry*)nano_malloc(num_files_all * sizeof(struct dir_entry));

   for(int i = 0; i < num_all_files; i++) {
      int matches = 1;
      if(filter) {
         for(int j = 0; matches && filter[j] && all_files[i].name[j]; j++)
            matches =
               matches && all_files[i].name[j]
               && all_files[i].name[j] == filter[j];
      }

      if(matches) {
         if(*num_files >= num_files_all) {
            num_files_all <<= 1;
            files = (struct dir_entry*)
               realloc(files, num_files_all * sizeof(struct dir_entry));
         }

         files[*num_files] = all_files[i];
         (*num_files)++;
      }
   }

   return files;
}

/*
 * Free an array of "struct dir_entry", as returned by "read_files" or
 * "filter_files".
 */
static void free_files(struct dir_entry *files, int num_files)
{
   for(int i = 0; i < num_files; i++)
      free(files[i].name);
   free(files);
}

/* ----------------------------------------------------------------------
   Actual file selections: Operations on struct input_file.
   ---------------------------------------------------------------------- */

/*
 * This structure holds the state of the file selection. (In OOP terms, think
 * of it as an object and the following functions as final methods.)
 */
struct input_file
{
   // Many fields are set by "nano_input_file" and described there.
   SDL_Surface *surface;
   TTF_Font *font;
   int line_height;
   SDL_Color dfg, dbg, lifg, libg,lafg, labg, ifg, ibg;
   SDL_Color sbbg; /* Mixture of dbg (dialog background) and libg (background of
                      the inactive part of the list); used as background for the
                      scrollbar. The scrollbar itself is drawn with dbg. */
   int must_exist, warn_replace, warn_modify, warn_append;

   int xt, yt, wt, ht; // space for the title
   int xd, yd, wd, hd; // space for the directory display, including border
   int xl, yl, wl, hl; // space for the list, including border
   int xi, yi, wi, hi; // space for the input field, including border

   /* List and size of (i) all files of the current directory, and (ii) the
      ones visible, possibly filtered by the TAB key. At the beginning,
      "filtered_files" is a copy of "all_files"; this simplifies memory
      management. */
   int num_all_files, num_filtered_files;
   struct dir_entry *all_files, *filtered_files;

   int off_files; /* vertical offset of the list, in pixels (i. e. the first
                     pixel of the list "canvas", which is visible in the
                     viewport */
   int sel_file;  /* index of the selected file, starting with 0, or -1, when
                     no file is selected */

   // buffer and state for the input; see "nano_input_text16"
   Uint16 input_buf[FILENAME_MAX + 1];
   int input_state;

   // currently selected directory
   char curpath[FILENAME_MAX + 1];
};

/*
 * Display the directory contents. Called rather often; e. g. when the selected
 * entry changes.
 */
static void display_dir(struct input_file *infi)
{
   int d = infi->line_height / 2;

   if(infi->sel_file != -1) {
      // Adjust "off_files", so that the selected file is fully visible.
      if(infi->sel_file * infi->line_height < infi->off_files)
         infi->off_files = infi->sel_file * infi->line_height;
      if((infi->sel_file + 1) * infi->line_height
         > infi->off_files + infi->hl - 2)
         infi->off_files =
            (infi->sel_file + 1) * infi->line_height - (infi->hl - 2);
   }

   // Display scrollbar: background ...
   nano_fill_rect(infi->surface, infi->sbbg,
                  infi->xl + infi->wl - 1 - d, infi->yl + 1, d, infi->hl - 2);
   // ... and 3D foreground
   int vah = infi->num_filtered_files * infi->line_height;
   int vph = infi->hl - 2;
   // MAX(vah, 1) to avoid errors when infi->num_filtered_files == 0
   int sby1 = vph * infi->off_files / MAX(vah, 1);
   int sby2 = vph * (infi->off_files + vph) / MAX(vah, 1);
   nano_fill_3d_outset(infi->surface, infi->dbg,
                       infi->xl + infi->wl - 1 - d, infi->yl + 1 + sby1,
                       d, MIN(sby2 - sby1, infi->hl - 2));

   // the actual list
   nano_fill_rect(infi->surface, infi->libg,
                  infi->xl + 1, infi->yl + 1, infi->wl - 2 - d, infi->hl - 2);
   SDL_Rect r = { infi->xl + 1, infi->yl + 1, infi->wl - 2 - d, infi->hl - 2 };
   SDL_SetClipRect(infi->surface, &r);

   // entries of the list
   for(int i = infi->off_files / infi->line_height;
       i < MIN(infi->num_filtered_files,
               (infi->hl - 2 + infi->off_files + infi->line_height - 1)
               / infi->line_height);
       i++) {
      Uint16 *s;
      if(infi->filtered_files[i].type == DE_TYPE_DIR) {
         // directories in brackets
         Uint16 buf[FILENAME_MAX + 1];
         const Uint16 bro[2] = { '[', 0 };
         const Uint16 brc[2] = { ']', 0 };
         strcpy16(buf, bro);
         strcat16(buf, infi->filtered_files[i].name);
         strcat16(buf, brc);
         s = buf;
      } else
         s = infi->filtered_files[i].name;

      // vertical position (relative to the SDL surface) of the entry
      int yi = infi->yl + 1 - infi->off_files + i * infi->line_height;
      
      // background (typically blue) for the selected entry
      if(i == infi->sel_file)
         nano_fill_rect(infi->surface, infi->labg, infi->xl + 1, yi,
                        infi->wl - 2, infi->line_height);

      // text; different foreground color for selected entry.
      SDL_Surface *t =
         TTF_RenderUNICODE_Shaded(infi->font, s,
                                  i == infi->sel_file ? infi->lafg : infi->lifg,
                                  i == infi->sel_file ? infi->labg
                                  : infi->libg);
      SDL_Rect r1 = { 0, 0, t->w, t->h };
      SDL_Rect r2 = { infi->xl + 1 + infi->line_height / 4, yi, t->w, t->h };
      SDL_BlitSurface(t, &r1, infi->surface, &r2);
      SDL_FreeSurface(t);
   }

   SDL_SetClipRect(infi->surface, NULL);
}

/*
 * Do anything neccessary, after the selected directory has changed.
 */
static void read_dir(struct input_file *infi)
{
   // 1. Display directory below the title.
   // TODO text may be too long
   nano_fill_3d_inset(infi->surface, infi->dbg,
                      infi->xd, infi->yd, infi->wd, infi->hd);
   Uint16 *curpath16 = strdup16fromutf8(infi->curpath);
   SDL_Surface *t =
      TTF_RenderUNICODE_Shaded(infi->font, curpath16, infi->dfg, infi->dbg);
   SDL_Rect r1 = { 0, 0, t->w, t->h };
   SDL_Rect r2 = { infi->xd + 1, infi->yd + 1, t->w, t->h };
   SDL_BlitSurface(t, &r1, infi->surface, &r2);
   SDL_FreeSurface(t);
   free(curpath16);
   
   // 2. Initialize both lists (all and filtered files).
   if(infi->filtered_files)
      free(infi->filtered_files);
   if(infi->all_files)
      free_files(infi->all_files, infi->num_all_files);
   
   infi->all_files = read_files(infi->curpath, &infi->num_all_files);
   infi->filtered_files =
      filter_files(infi->all_files, infi->num_all_files,
                   &infi->num_filtered_files, NULL);
   infi->off_files = 0, infi->sel_file = -1;

   // 3. Display the contents.
   display_dir(infi);
}

/*
 * Called by handle_anything(), to deal with the directory part of the input
 * in a consistent way, when ENTER or TAB has been pressed.
 *
 * Returns whether handling was successful.
 */
static int handle_directory(struct input_file *infi, char *dir)
{
   // TODO: Existence/access permissions/error handling could be improved!
   struct stat sb;
   if(stat(dir, &sb) == 0) {
      // Exists (actually, more precisely: is accessable) ...
      if(S_ISDIR(sb.st_mode)) {
         // ... and is a directory: enter it.
         // TODO: This could fail.
         realpath(dir, infi->curpath);
         read_dir(infi);
         return 1;
      } else {
         // .. is not a directory: print an error.
         Uint16 *dir16 = strdup16fromutf8(dir);
         int l = strlen16(dir16);
         Uint16 *line1 = (Uint16*)nano_malloc((l + 3) * sizeof(Uint16));
         line1[0] = 0x201c;
         strcpy16(line1 + 1, dir16);
         line1[l + 1] = 0x201d;
         line1[l + 2] = 0;
         Uint16 line2[] = { 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ',
                            'd', 'i', 'r', 'e', 'c', 't', 'o', 'r', 'y', '\0' };
         Uint16 *lines[] = { line1, line2, NULL };
         nano_show_message(infi->surface, lines, infi->dfg, infi->dbg,
                           infi->font);
         free(dir16);
         free(line1);
         return 0;
      }
   } else {
      // Does not exist. Ask to create.
      // TODO: "Does not exist" is not correct, "stat" may fail for different
      //       reasons. Perhaps check the obvious case (errno == ENOENT) for
      //       this branch, otherwise print strerror(errno).
      Uint16 *dir16 = strdup16fromutf8(dir);
      int l = strlen16(dir16);
      Uint16 line1[] = { 'T', 'h', 'e', ' ',
                         'd', 'i', 'r', 'e', 'c', 't', 'o', 'r', 'y', '\0' };
      Uint16 *line2 = (Uint16*)nano_malloc((l + 3) * sizeof(Uint16));
      line2[0] = 0x201c;
      strcpy16(line2 + 1, dir16);
      line2[l + 1] = 0x201d;
      line2[l + 2] = 0;
      Uint16 line3[] = { 'd', 'o', 'e', 's', ' ', 'n', 'o', 't', ' ',
                         'e', 'x', 'i', 's', 't', '.', ' ',
                         'C', 'r', 'e', 'a', 't', 'e', '?', 0 };
      Uint16 *lines[] = { line1, line2, line3, NULL };
      int ret;

      if(nano_ask_yes_no(infi->surface, lines, 0, infi->dfg, infi->dbg,
                         infi->font)) {
         // User agrees ...
         if(mkdir(dir, 0777) == 0)
            // ... and succeeds.
            ret = handle_directory(infi, dir);
         else {
            // ... but fails.
            Uint16 line1b[] = { 'C', 'r', 'e', 'a', 't', 'i', 'n', 'g', 0 };
            Uint16 line3b[] = { 'f', 'a', 'i', 'l', 'e', 'd', ':', 0 };
            Uint16 *line4b = strdup16fromutf8(strerror(errno));
            Uint16 *linesb[] = { line1b, line2, line3b, line4b, NULL };
            nano_show_message(infi->surface, linesb, infi->dfg, infi->dbg,
                              infi->font);
            free(line4b);
            ret = 0;
         }
      } else
         ret = 0;

      free(dir16);
      free(line2);
      return ret;
   }
}

/*
 * This function is called when TAB or ENTER is pressed, to handle a directory
 * part, when given. The function returns whether everything went OK (1 means:
 * the caller can continue, 0 means: the current operation has failed, so
 * conuing is not possible). Furthermore, it sets several values in "infi":
 * "curpath" contains the new directory, and "input_buf" contains a simple
 * filename, without any directory part (at least, when 1 is returned).
 */
static int handle_anything(struct input_file *infi)
{
   // Simple rule to determine, whether there is a directory part: starts
   // with "~", or contains "/".
   if(infi->input_buf[0] == '~' || strrchr16(infi->input_buf, '/')) {
      char complete[FILENAME_MAX + 1]; // contains both, directory and base name
      char *f = strduputf8from16(infi->input_buf);

      if(f[0] == '/')
         // absolute path already given
         strcpy(complete, f);
      else if(f[0] == '~') {
         // Simple: only home directory of current user. (Otherwise, handling
         // the TAB key would become more complicated.)
         struct passwd *pw = getpwuid(getuid());
         strcpy(complete, pw->pw_dir);
         strcat(complete, f + 1);
      } else {
         // relative path
         strcpy(complete, infi->curpath);
         if(strcmp(infi->curpath, "/") != 0)
            strcat(complete, "/");
         strcat(complete, f);
      }
      free(f);

      // cut of base name
      char *slash = strrchr(complete, '/'), *dir;
      *slash = 0;
      if(slash == complete)
         // Special case: only one slash at the beginning. Handled specially,
         // since otherwise, the directory would be "".
         dir = "/";
      else
         dir = complete;
      
      if(handle_directory(infi, dir)) {
         // Success => change input field.
         // (Changing the directory is already done in handle_directory().)
         Uint16 *base = nano_strdup16fromutf8(slash + 1);
         strcpy16(infi->input_buf, base);
         free(base);
         infi->input_state = -1;
         return 1;
      } else
         return 0;
   }
   else
      // No directory => nothing to do, and nothing can fail.
      return 1;
}

/*
 * Called, when ENTER has been pressed.
 */
static char *return_pressed(struct input_file *infi)
{
   if(handle_anything(infi)) {
      char filepath[FILENAME_MAX + 1];
      strcpy(filepath, infi->curpath);
      if(strcmp(infi->curpath, "/") != 0)
         strcat(filepath, "/");
      char *f = strduputf8from16(infi->input_buf);
      strcat(filepath, f);
      free(f);

      struct stat sb;
      if(stat(filepath, &sb) == 0) {
         if(S_ISDIR(sb.st_mode)) {
            // file exists and is a directory => change directory
            realpath(filepath, infi->curpath);
            read_dir(infi);
            infi->input_buf[0] = 0;
            infi->input_state = -1;
            return NULL;
         } else {
            // file exists and is a regular file => warn when necessary
            int ok;

            if(infi->warn_modify || infi->warn_replace || infi->warn_append) {
               Uint16 line_modify[] = { 'M', 'o', 'd', 'i', 'f', 'y', 0 };
               Uint16 line_replace[] = { 'R', 'e', 'p', 'l', 'a', 'c', 'e', 0 };
               Uint16 line_append[] = { 'A', 'p', 'p', 'e', 'n', 'd', ' ',
                                        't', 'o', 0 };
               Uint16 *line1;
               if(infi->warn_modify)
                  line1 = line_modify;
               else if(infi->warn_replace)
                  line1 = line_replace;
               else if(infi->warn_append)
                  line1 = line_append;

               Uint16 *filename = strdup16fromutf8(filepath);
               int l = strlen16(filename);
               Uint16 *line2 = (Uint16*)nano_malloc((l + 4) * sizeof(Uint16));
               line2[0] = 0x201c;
               strcpy16(line2 + 1, filename);
               line2[l + 1] = 0x201d;
               line2[l + 2] = '?';
               line2[l + 3] = 0;
      
               Uint16 *lines[] = { line1, line2, NULL };
               ok = nano_ask_yes_no(infi->surface, lines, 0,
                                    infi->dfg, infi->dbg, infi->font);
               free(filename);
               free(line2);
            } else
               ok = 1;

            if(ok)
               return strdup(filepath);
            else
                  return NULL;
         }
      } else {
         // file does not exist: return when this is allowed.
         if(infi->must_exist) {
            Uint16 *filename = strdup16fromutf8(filepath);
            int l = strlen16(filename);
            Uint16 line1[] = { 'T', 'h', 'e', ' ', 'f', 'i', 'l', 'e', '\0' };
            Uint16 *line2 = (Uint16*)nano_malloc((l + 3) * sizeof(Uint16));
            line2[0] = 0x201c;
            strcpy16(line2 + 1, filename);
            line2[l + 1] = 0x201d;
            line2[l + 2] = 0;
            Uint16 line3[] = { 'd', 'o', 'e', 's', ' ', 'n', 'o', 't', ' ',
                               'e', 'x', 'i', 's', 't', '!', 0 };
            Uint16 *lines[] = { line1, line2, line3, NULL };
            nano_show_message(infi->surface, lines, infi->dfg, infi->dbg,
                              infi->font);
            return NULL;
         } else
            return strdup(filepath);
      }
   } else
      return NULL;
}

/*
 * Called, when TAB has been pressed.
 */
static void tab_pressed(struct input_file *infi)
{
   if(handle_anything(infi)) {
      int num_new_filtered_files;
      struct dir_entry *new_filtered_files =
         filter_files(infi->all_files, infi->num_all_files,
                      &num_new_filtered_files, infi->input_buf);
      
      // Can the current input completed in an unambigous way? If yes,
      // "n" is the number of chacacters for this.
      int l = strlen16(infi->input_buf), n = 0;
      if(num_new_filtered_files == 1)
         n = strlen16(new_filtered_files[0].name) - l;
      else if(num_new_filtered_files > 1) {
         int match = 1;
         while(match) {
            for(int i = 1; match && i < num_new_filtered_files; i++)
               match = match &&
                  (new_filtered_files[0].name[l + n] ==
                   new_filtered_files[i].name[l + n]);
            if(match)
               n++;
         }
      }
      
      if(n > 0) {
         // Completion of some characters is possible. Change input field,
         // but not list.
         for(int i = 0; i < n; i++)
            infi->input_buf[l + i] = new_filtered_files[0].name[l + i];
         infi->input_buf[l + n] = 0;
         infi->input_state = -1;
         free(new_filtered_files);
      } else {
         // Not possible => show filtered files.
         free(infi->filtered_files);
         infi->filtered_files = new_filtered_files;
         infi->num_filtered_files = num_new_filtered_files;
         infi->sel_file = -1;
         infi->off_files = 0;
         display_dir(infi);
      }

      // Do we get a directory this way? (In both cases possible.)
      // Trivial: empty + slash is a directory, but this is not what we want.
      if(infi->input_buf[0]) {
         char filepath[FILENAME_MAX + 1];
         strcpy(filepath, infi->curpath);
         if(strcmp(infi->curpath, "/") != 0)
            strcat(filepath, "/");
         char *f = strduputf8from16(infi->input_buf);
         strcat(filepath, f);
         free(f);
         
         struct stat sb;
         if(stat(filepath, &sb) == 0 && S_ISDIR(sb.st_mode)) {
            // Yes. Add "/" for convenience.
            infi->input_buf[l + n] = '/';
            infi->input_buf[l + n + 1] = 0;
            infi->input_state = -1;
         }
      }
   }
}


/*
 * Starts the highly ideosyncratic file selection dialog. Retuns the absolute
 * path, when the user has selected a file, or NULL, when the user has escaped
 * the dialog.
 *
 * Arguments:
 * - path          the initially selected directory
 * - title         a static title shown at the top of the dialog
 * - surface       the SDL surface this dialog should be shown in
 * - font          the font used for all text in the dialog
 * - line_height   height of the font; the result of TTF_RenderText_Shaded(font,
                   " ", ...)->h
 * - dfg, dbg      foreground and background for the dialog
 * - lifg, libg    foreground and background for the inactive part of the list
 * - lafg, labg    foreground and background for the selected (active) list item
 * - ifg, ibg      foreground and background for the input field
 * - x, y, w, h    the area in which the dialog is shown
 * - must_exist    if set to 1, only an existing file may be selected
 * - warn_replace  and
 * - warn_modify   and
 * - warn_append   if set to 1, a respective warning is shown, when the user
 *                 selects an existin file
 */
char *nano_input_file(char *path, Uint16 *title, SDL_Surface *surface,
                      TTF_Font *font, int line_height,
                      SDL_Color dfg, SDL_Color dbg,
                      SDL_Color lifg, SDL_Color libg,
                      SDL_Color lafg, SDL_Color labg,
                      SDL_Color ifg, SDL_Color ibg,
                      int x, int y, int w, int h, int must_exist,
                      int warn_replace, int warn_modify, int warn_append)
{
   nano_save_window(surface, x, y, w, h);

   nano_fill_3d_outset(surface, dbg, x, y, w, h);
   
   int d = line_height / 4;

   struct input_file infi;
   infi.surface = surface;
   infi.font = font;
   infi.line_height = line_height;
   infi.dfg = dfg;
   infi.dbg = dbg;
   infi.lifg = lifg;
   infi.libg = libg;
   infi.lafg = lafg;
   infi.labg = labg;
   infi.ifg = ifg;
   infi.ibg = ibg;

   infi.sbbg.r = (dbg.r + libg.r) / 2;
   infi.sbbg.g = (dbg.g + libg.g) / 2;
   infi.sbbg.b = (dbg.b + libg.b) / 2;

   infi.xt = infi.xd = infi.xl = infi.xi = x + d + 1;
   infi.wt = infi.wd = infi.wl = infi.wi = w - 2 * d - 2;
   infi.ht = line_height;
   infi.hd = infi.hi = line_height + 2;
   infi.hl = h - (2 + infi.ht + infi.hd + infi.hi + 5 * d);
   infi.yt = y + 1 + d;
   infi.yd = infi.yt + infi.ht + d;
   infi.yl = infi.yd + infi.hd + d;
   infi.yi = infi.yl + infi.hl + d;

   infi.must_exist = must_exist;
   infi.warn_replace = warn_replace;
   infi.warn_modify = warn_modify;
   infi.warn_append = warn_append;

   SDL_Surface *t = TTF_RenderUNICODE_Shaded(font, title, dfg, dbg);
   SDL_Rect r1 = { 0, 0, t->w, t->h };
   SDL_Rect r2 = { infi.xt, infi.yt, t->w, t->h };
   SDL_BlitSurface(t, &r1, surface, &r2);
   SDL_FreeSurface(t);
   
   nano_fill_3d_inset(surface, libg, infi.xl, infi.yl, infi.wl, infi.hl);
   nano_fill_3d_inset(surface, ibg, infi.xi, infi.yi, infi.wi, infi.hi);

   infi.all_files = infi.filtered_files = NULL;
   
   infi.input_buf[0] = 0;
   infi.input_state = -1;
   
   char *result;
   int main_loop = 1;
 
   realpath(path, infi.curpath);
   read_dir(&infi);

   while(main_loop) {
      SDL_Event event;

      nano_input_text16(surface, &event, infi.input_buf,
                        FILENAME_MAX, 255,
                        infi.xi + 1, infi.yi + 1, infi.wi - 2, infi.hi - 2,
                        font, ifg, ibg, &infi.input_state);
      if(event.type == SDL_KEYDOWN) {
         switch(event.key.keysym.sym) {
         case SDLK_RETURN:
            result = return_pressed(&infi);
            main_loop = result == NULL;
            break;
            
         case SDLK_ESCAPE:
            main_loop =  0;
            result = NULL;
            break;
            
         case SDLK_UP:
            if(infi.num_filtered_files > 0) {
               if(infi.sel_file == -1)
                  infi.sel_file = infi.num_filtered_files - 1;
               else {
                  infi.sel_file--;
                  if(infi.sel_file < 0)
                     infi.sel_file = infi.num_filtered_files - 1;
               }
               strcpy16(infi.input_buf,
                        infi.filtered_files[infi.sel_file].name);
               infi.input_state = -1;
            }
            display_dir(&infi);
            break;
            
         case SDLK_DOWN:
            if(infi.num_filtered_files > 0) {
               if(infi.sel_file == -1)
                  infi.sel_file = 0;
               else {
                     infi.sel_file++;
                     if(infi.sel_file >= infi.num_filtered_files)
                        infi.sel_file = 0;
               }
               strcpy16(infi.input_buf,
                        infi.filtered_files[infi.sel_file].name);
               infi.input_state = -1;
            }
            display_dir(&infi);
            break;
            
         case SDLK_TAB:
            tab_pressed(&infi);
            break;
            
         default:
            break;
         }
      }
   }

   if(infi.filtered_files)
      free(infi.filtered_files);
   if(infi.all_files)
      free_files(infi.all_files, infi.num_all_files);

   nano_restore_window(surface);
   return result;
}
