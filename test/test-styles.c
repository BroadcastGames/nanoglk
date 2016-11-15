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
 * Test bed for displaying text in windows in different styles. You
 * may also vary the fonts for the styles.
 */

#include <stdio.h>
#include "glk.h"
#include "glkstart.h"

void glk_main()
{
   winid_t win1 = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
   winid_t win2 = glk_window_open(win1,
                                  winmethod_Above | winmethod_Proportional,
                                  10, wintype_TextBuffer, 0);

   strid_t str1 = glk_window_get_stream(win1);
   strid_t str2 = glk_window_get_stream(win2);

   glk_stream_set_current(str2);
   glk_put_string("Status line");
   glk_stream_set_current(str1);

   int i, j;
   for(i = 0; i < 5; i++) {
      for(j = 0; j < 20; j++) {
         char buf[30];
         sprintf(buf, "Test #%d.%d: ", i, j);
         glk_put_string(buf);
         glk_set_style(style_Header);
         glk_put_string(" Header");
         glk_set_style(style_Normal);
         glk_put_string(" and normal again.\n");
      }

      glk_put_string("Please press key ");
      glk_put_char_uni(0x2026);
      glk_put_char('\n');
      
      glk_request_char_event(win1);
      
      int f = 1;
      while(f) {
         event_t ev;
         glk_select(&ev);
         switch(ev.type) {
         case evtype_CharInput:
            /* do nothing */
            f = 0;
         }
      }

      glk_window_set_arrangement(glk_window_get_parent(win2),
                                 winmethod_Left | winmethod_Above
                                 | winmethod_Proportional,
                                 10 + i * 5, win2);
   }

   glk_exit();
}

int glkunix_startup_code(glkunix_startup_t *data)
{
   return 1;
}
