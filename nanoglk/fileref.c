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
 * Handling file references. Mostly rather trivial.
 */

#include "nanoglk.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* These two values represent a linked list, in which all file refs
   are kept. Used for iterators. See also the macros ADD() and
   UNLINK() defined in "nanoglk.h" */
static frefid_t first = NULL, last = NULL;

static frefid_t create_by_name(glui32 usage, char *name, glui32 rock);

frefid_t glk_fileref_create_temp(glui32 usage, glui32 rock)
{
   frefid_t fref = create_by_name(usage, tmpnam(NULL), rock);
   nanoglk_log("glk_fileref_create_temp(%d, %d) => %p", usage, rock, fref);
   fref->disprock = nanoglk_call_regi_obj(fref, gidisp_Class_Fileref);
   return fref;
}

frefid_t glk_fileref_create_by_name(glui32 usage, char *name, glui32 rock)
{
   frefid_t fref = create_by_name(usage, name, rock);
   nanoglk_log("glk_fileref_create_by_name(%d, '%s', %d) => %p",
               usage, name, rock, fref);
   fref->disprock = nanoglk_call_regi_obj(fref, gidisp_Class_Fileref);
   return fref;
}

frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock)
{
   int must_exist = 0, warn_replace = 0, warn_modify = 0, warn_append = 0;
   char title8[128];

   switch(fmode) {
   case filemode_Read:
      strcpy(title8, "Read ");
      must_exist = 1;
      break;

   case filemode_Write:
      strcpy(title8, "Write (or replace) ");
      warn_replace = 1;
      break;
      
   case filemode_ReadWrite:
      strcpy(title8, "Write (or modify) ");
      warn_modify = 1;
      break;

   case filemode_WriteAppend:
      strcpy(title8, "Write (or append to) ");
      warn_append = 1;
      break;
   }

   switch(usage & fileusage_TypeMask) {
   case fileusage_Data:
      strcat(title8, "data");
      break;

   case fileusage_SavedGame:
      strcat(title8, "saved game");
      break;

   case fileusage_Transcript:
      strcat(title8, "transscript");
      break;

   case fileusage_InputRecord:
      strcat(title8, "input record file");
      break;
   }

   char cwd[FILENAME_MAX + 1];
   getcwd(cwd, FILENAME_MAX + 1);

   Uint16 *title16 = strdup16fromutf8(title8);
   char *name = 
      nano_input_file(cwd, title16, nanoglk_mainwindow->nanoglk_surface,
                      nanoglk_ui_font->font, nanoglk_ui_font->text_height,
                      nanoglk_ui_font->fg, nanoglk_ui_font->bg,
                      nanoglk_ui_list_i_fg_color, nanoglk_ui_list_i_bg_color,
                      nanoglk_ui_list_a_fg_color, nanoglk_ui_list_a_bg_color,
                      nanoglk_ui_input_fg_color, nanoglk_ui_input_bg_color,
                      (nanoglk_screen_width - nanoglk_filesel_width) / 2,
                      (nanoglk_screen_height - nanoglk_filesel_height) / 2,
                      nanoglk_filesel_width, nanoglk_filesel_height,
                      must_exist, warn_replace, warn_modify, warn_append);
   free(title16);
   // TODO change directory depending on the result?

   frefid_t fref = NULL;

   if(name) {
      fref = (frefid_t)nano_malloc(sizeof(struct glk_fileref_struct));
      fref->usage = usage;
      fref->rock = rock;
      fref->name = name;
      ADD(fref);
   }

   nanoglk_log("glk_fileref_create_by_prompt(%d, %d, %d) => %p",
               usage, fmode, rock, fref);
   if(fref)
      fref->disprock = nanoglk_call_regi_obj(fref, gidisp_Class_Fileref);

   return fref;
}

frefid_t glk_fileref_create_from_fileref(glui32 usage, frefid_t fref,
                                         glui32 rock)
{
   frefid_t nfref = create_by_name(usage, fref->name, rock);
   nanoglk_log("glk_fileref_create_from_fileref(%d, %p, %d) => %p",
               usage, fref, rock, nfref);
   fref->disprock = nanoglk_call_regi_obj(fref, gidisp_Class_Fileref);
   return nfref;
}

void glk_fileref_destroy(frefid_t fref)
{
   nanoglk_log("glk_fileref_destroy(%p)", fref);
   nanoglk_call_unregi_obj(fref, gidisp_Class_Fileref, fref->disprock);
   free(fref->name);
   UNLINK(fref);
   free(fref);
}

frefid_t glk_fileref_iterate(frefid_t fref, glui32 *rockptr)
{
   frefid_t next;

   if(fref == NULL)
      next = first;
   else
      next = fref->next;

   if(next && rockptr)
      *rockptr = next->rock;
   
   nanoglk_log("glk_fileref_iterate(%p, ...) => %p", fref, next);
   return next;
}

glui32 glk_fileref_get_rock(frefid_t fref)
{
   nanoglk_log("glk_fileref_get_rock(%p) => %d", fref, fref->rock);
   return fref->rock;
}

void glk_fileref_delete_file(frefid_t fref)
{
   nanoglk_log("glk_fileref_delete_file(%p)", fref);
   unlink(fref->name);
}

glui32 glk_fileref_does_file_exist(frefid_t fref)
{
   // Could check errno, but for practical usage, this should be sufficient.
   struct stat sb;
   int exists = stat(fref->name, &sb) == 0;
   nanoglk_log("glk_fileref_does_file_exist(%p) => %d", fref, exists);
   return exists;
}

/*
 * Create a file reference with a given name, given flags, and a given
 * rock. Called by all other functions, which return a new file
 * reference.
 */
frefid_t create_by_name(glui32 usage, char *name, glui32 rock)
{
   frefid_t fref = (frefid_t)nano_malloc(sizeof(struct glk_fileref_struct));
   fref->usage = usage;
   fref->rock = rock;
   fref->name = strdup(name);
   ADD(fref);
   return fref;
}
