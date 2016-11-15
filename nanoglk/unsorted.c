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
 * Functions fitting nowhere else. Not much implemented, by the way ...
 */

#include "nanoglk.h"
#include <ctype.h>

static glui32 gestalt(glui32 sel, glui32 val)
{
   switch(sel) {
   case gestalt_Version:
      return 0x702;

   case gestalt_CharInput:
   case gestalt_LineInput:
   case gestalt_CharOutput:
      //case gestalt_CharOutput_CannotPrint:
      //case gestalt_CharOutput_ApproxPrint:
      //case gestalt_CharOutput_ExactPrint:
      return 1;

   case gestalt_MouseInput:
   case gestalt_Timer:
      return 0;

   case gestalt_Graphics:
   case gestalt_DrawImage:
      return 1;

   case gestalt_Sound:
   case gestalt_SoundVolume:
   case gestalt_SoundNotify:
   case gestalt_Hyperlinks:
   case gestalt_HyperlinkInput:
   case gestalt_SoundMusic:
   case gestalt_GraphicsTransparency:
      return 0;

   case gestalt_Unicode:
   case gestalt_UnicodeNorm:
      return 1;

   case gestalt_LineInputEcho:
   case gestalt_LineTerminators:
   case gestalt_LineTerminatorKey:
   case gestalt_DateTime:
      return 0;

   default:
      return 0;
   }

}

glui32 glk_gestalt(glui32 sel, glui32 val)
{
   glui32 res = gestalt(sel, val);
   nanoglk_log("glk_gestalt(%d, %d) => %d", sel, val, res);
   return res;
}

glui32 glk_gestalt_ext(glui32 sel, glui32 val, glui32 *arr, glui32 arrlen)
{
   glui32 res = gestalt(sel, val);
   nanoglk_log("glk_gestalt(%d, %d, ..., %d) => %d", sel, val, arrlen, res);
   return res;
}

extern unsigned char glk_char_to_lower(unsigned char ch)
{
   nanoglk_log("glk_char_to_lower(%d) => ...", ch);
   // TODO
   return tolower(ch);
}

extern unsigned char glk_char_to_upper(unsigned char ch)
{
   nanoglk_log("glk_char_to_upper(%d) => ...", ch);
   // TODO
   return toupper(ch);
}

glui32 glk_buffer_to_lower_case_uni(glui32 *buf, glui32 len, glui32 numchars)
{
   nanoglk_log("glk_char_to_lower_case_uni(..., %d, %d) => ...", len, numchars);
   // TODO
   return len;
}

glui32 glk_buffer_to_upper_case_uni(glui32 *buf, glui32 len, glui32 numchars)
{
   nanoglk_log("glk_char_to_upper_case_uni(..., %d, %d) => ...", len, numchars);
   // TODO
   return len;
}

glui32 glk_buffer_to_title_case_uni(glui32 *buf, glui32 len,
                                    glui32 numchars, glui32 lowerrest)
{
   nanoglk_log("glk_char_to_title_case_uni(..., %d, %d, %d) => ...",
               len, numchars, lowerrest);
   // TODO
   return len;
}


void glk_set_terminators_line_event(winid_t win, glui32 *keycodes, glui32 count)
{
   nanoglk_log("glk_set_terminators_line_event(%p, ..., %d)", win, count);
   // TODO
}

