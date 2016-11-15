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
 * Test the highly idiosyncratic file selection dialog./
 */

#include <stdio.h>
#include "glk.h"
#include "glkstart.h"

void glk_main()
{
   winid_t win1 = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
   winid_t win2 =
      glk_window_open(win1,
                      winmethod_Left | winmethod_Above | winmethod_Proportional,
                      10, wintype_TextBuffer, 0);

   strid_t str1 = glk_window_get_stream(win1);
   strid_t str2 = glk_window_get_stream(win2);

   glk_stream_set_current(str2);
   glk_put_string("Status line");
   glk_stream_set_current(str1);

   glk_fileref_create_by_prompt(fileusage_Transcript, filemode_WriteAppend, 0);
   glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Read, 0);

   glk_exit();
}

int glkunix_startup_code(glkunix_startup_t *data)
{
   return 1;
}
