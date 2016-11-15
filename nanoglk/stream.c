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
 * Handling streams.
 *
 * TODO: Quite much, search for "TODO" in this file. Especially,
 * unicode file streams are handled the same way as non-unicode file
 * streams. (It has to be specified how they are stored. UTF-8?
 * Configurable? Detecting when opening an existing file for reading?)
 */

#include "nanoglk.h"

static strid_t first = NULL, last = NULL, current = NULL;

static void put_char_uni(strid_t str, glui32 ch);
static void put_string(strid_t str, char *s);
static void put_string_uni(strid_t str, glui32 *s);
static void put_buffer(strid_t str, char *buf, glui32 len);
static void put_buffer_uni(strid_t str, glui32 *buf, glui32 len);
static void set_style(strid_t str, glui32 styl);
static glsi32 get_char_uni(strid_t str);

/*
 * Called by all functions creating a stream. Also by glk_window_open(),
 * so this function must not be static and gets the prefix "nanoglk_".
 */
strid_t nanoglk_stream_new(glui32 type, glui32 rock)
{
   strid_t str = (strid_t)nano_malloc(sizeof(struct glk_stream_struct));
   str->type = type;
   str->rock = rock;

   ADD(str);
   return str;
}

/*
 * Create a stream from a a standard C FILE.
 */
static strid_t new_file_stream(FILE *file, glui32 type, glui32 rock)
{
   if(file == NULL)
      return NULL;
   else {
      strid_t str = nanoglk_stream_new(type, rock);
      str->x.file = file;
      return str;
      
      // TODO: There is certainly a reason, why all callers of new_file_stream()
      // contain the lines
      //
      // if(str)
      //    str->disprock = nanoglk_call_regi_obj(str, gidisp_Class_Stream);
      //
      // But I've forgotten it. Perhaps GLK logging.

   }
}

strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode, 
                                     glui32 rock)
{
   strid_t str = new_file_stream(fopen(pathname, "r"), streamtype_File, rock);
   nanoglk_log("glkunix_stream_open_pathname('%s', %d, %d) => %p",
               pathname, textmode, rock, str);
   if(str)
      str->disprock = nanoglk_call_regi_obj(str, gidisp_Class_Stream);
   return str;
}

/*
 * Convert Glk mode into mode needed by fopen(3).
 */
static char *conv_mode(glui32 fmode)
{
   switch(fmode) {
   case filemode_Write:
      return "w";

   case filemode_Read:
      return "r";

   case filemode_ReadWrite:
      return "r+";

   case filemode_WriteAppend:
      return "a";

   default:
      nano_fail("invalid file mode %d", fmode);
      return NULL;
   }
}

strid_t glk_stream_open_file(frefid_t fileref, glui32 fmode, glui32 rock)
{
   strid_t str = new_file_stream(fopen(fileref->name, conv_mode(fmode)),
                                 streamtype_File, rock);
   nanoglk_log("glk_stream_open_file(%p ['%s'], %d, %d) => %p",
               fileref, fileref->name, fmode, rock, str);
   if(str)
      str->disprock = nanoglk_call_regi_obj(str, gidisp_Class_Stream);
   return str;
}

strid_t glk_stream_open_file_uni(frefid_t fileref, glui32 fmode, glui32 rock)
{
   strid_t str = new_file_stream(fopen(fileref->name, conv_mode(fmode)),
                                 streamtype_File_Uni, rock);
   nanoglk_log("glk_stream_open_file_uni(%p ['%s'], %d, %d) => %p",
               fileref, fileref->name, fmode, rock, str);
   if(str)
      str->disprock = nanoglk_call_regi_obj(str, gidisp_Class_Stream);
   return str;
}

strid_t glk_stream_open_memory(char *buf, glui32 buflen, glui32 fmode,
                               glui32 rock)
{
   strid_t str = nanoglk_stream_new(streamtype_Buffer, rock);
   str->x.buf.b.u8 = buf;
   str->x.buf.pos = 0;
   str->x.buf.len = buflen;

   if(str->x.buf.b.u8)
      str->arrrock =
         nanoglk_call_regi_arr(str->x.buf.b.u8, str->x.buf.len, "&+#!Cn");

   nanoglk_log("glk_stream_open_memory(%p %d, %d, %d) => %p",
               buf, buflen, fmode, rock, str);
   str->disprock = nanoglk_call_regi_obj(str, gidisp_Class_Stream);
   return str;
}

strid_t glk_stream_open_memory_uni(glui32 *buf, glui32 buflen, glui32 fmode,
                                   glui32 rock)
{
   strid_t str = nanoglk_stream_new(streamtype_Buffer_Uni, rock);
   str->x.buf.b.u32 = buf;
   str->x.buf.pos = 0;
   str->x.buf.len = buflen;

   if(str->x.buf.b.u32)
      str->arrrock =
         nanoglk_call_regi_arr(str->x.buf.b.u32, str->x.buf.len, "&+#!Iu");

   nanoglk_log("glk_stream_open_memory_uni(%p %d, %d, %d) => %p",
               buf, buflen, fmode, rock, str);
   str->disprock = nanoglk_call_regi_obj(str, gidisp_Class_Stream);
   return str;
}

void glk_stream_close(strid_t str, stream_result_t *result)
{
   nanoglk_call_unregi_obj(str, gidisp_Class_Stream, str->disprock);

   // TODO: Bytes/characters are not yet count.
   if(result)
      result->readcount = result->writecount = 0;

   switch(str->type) {
   case streamtype_Window:
      str->x.window = NULL;
      break;

   case streamtype_File:
   case streamtype_File_Uni:
      fclose(str->x.file);
      str->x.file = NULL;
      break;

   case streamtype_Buffer:
      if(str->x.buf.b.u8)
         nanoglk_call_unregi_arr(str->x.buf.b.u8, str->x.buf.len, "&+#!Cn",
                                 str->arrrock);
      break;

   case streamtype_Buffer_Uni:
      if(str->x.buf.b.u32)
         nanoglk_call_unregi_arr(str->x.buf.b.u32, str->x.buf.len, "&+#!Iu",
                                 str->arrrock);
      break;

   default:
      nano_fail("glk_stream_close not implemented");
   }

   if(result)
      nanoglk_log("glk_stream_close(%p, ...) => (%d, %d)", 
                  str, result->readcount, result->writecount);
   else
      nanoglk_log("glk_stream_close(%p, ...)", str);

   UNLINK(str);
   free(str);
}

strid_t glk_stream_iterate(strid_t str, glui32 *rockptr)
{
   strid_t next;

   if(str == NULL)
      next = first;
   else
      next = str->next;

   if(next && rockptr)
      *rockptr = next->rock;
   
   nanoglk_log("glk_stream_iterate(%p, ...) => %p", str, next);
   return next;
}

glui32 glk_stream_get_rock(strid_t str)
{
   nanoglk_log("glk_stream_get_rock(%p) => %d", str, str->rock);
   return str->rock;
}

void glk_stream_set_position(strid_t str, glsi32 pos, glui32 seekmode)
{
   nanoglk_log("glk_stream_set_position(%p, %d, %d)", str, pos, seekmode);

   switch(str->type) {
   case streamtype_Window:
      nano_warn("glk_stream_set_position not implemented for windows");
      break;

   case streamtype_File:
   case streamtype_File_Uni: // TODO
      {
         int whence = 0;
         switch(seekmode) {
         case seekmode_Start: whence = SEEK_SET; break;
         case seekmode_Current: whence = SEEK_CUR; break;
         case seekmode_End: whence = SEEK_END; break;
         default: nano_fail("unknown seekmode %d", seekmode);
         }
         fseek(str->x.file, pos, whence);
      }
      break;
         
   case streamtype_Buffer:
   case streamtype_Buffer_Uni:
      switch(seekmode) {
      case seekmode_Start: str->x.buf.pos = pos; break;
      case seekmode_Current: str->x.buf.pos += pos; break;
      case seekmode_End: str->x.buf.pos = str->x.buf.len - pos; break;
      default: nano_fail("unknown seekmode %d", seekmode);
      }
      break;
   }
}

glui32 glk_stream_get_position(strid_t str)
{
   glui32 ret = 0;

   switch(str->type) {
   case streamtype_Window:
      nano_warn("glk_stream_get_position not implemented");
      break;

   case streamtype_File:
   case streamtype_File_Uni: // TODO
      ret = ftell(str->x.file);
      break;

   case streamtype_Buffer:
   case streamtype_Buffer_Uni:
      ret = str->x.buf.pos;
      break;
   }

   nanoglk_log("glk_stream_set_position(%p) => %d", str, ret);
   return ret;
}

void glk_stream_set_current(strid_t str)
{
   nanoglk_log("glk_stream_set_current(%p)", str);
   nanoglk_stream_set_current(str);
}

/*
 * Identical to glk_stream_set_current() (actually, the latter calls
 * this function), but needed to prevent Glk functions calling Glk
 * functions (see glk_set_window() in "window.c").
 */
void nanoglk_stream_set_current(strid_t str)
{
   current = str;
}

strid_t glk_stream_get_current(void)
{
   nanoglk_log("glk_stream_get_current() => %p", current);
   return current;
}

void glk_put_char(unsigned char ch)
{
   if(ch >= 32 && ch <= 126)
      nanoglk_log("glk_put_char('%c')", ch);
   else
      nanoglk_log("glk_put_char('\\u%02x')", ch);

   if(current)
      put_char_uni(current, ch);
}

void glk_put_char_uni(glui32 ch)
{
   if(ch >= 32 && ch <= 126)
      nanoglk_log("glk_put_char_uni('%c')", ch);
   else
      nanoglk_log("glk_put_char_uni('\\u%04x')", ch);

   if(current)
      put_char_uni(current, ch);
}

void glk_put_char_stream(strid_t str, unsigned char ch)
{
   if(ch >= 32 && ch <= 126)
      nanoglk_log("glk_put_char_stream(%p, '%c')", str, ch);
   else
      nanoglk_log("glk_put_char_stream(%p, '\\u%02x')", str, ch);

   put_char_uni(str, ch);
}

void glk_put_char_stream_uni(strid_t str, glui32 ch)
{
   if(ch >= 32 && ch <= 126)
      nanoglk_log("glk_put_char_stream_uni(%p, '%c')", str, ch);
   else
      nanoglk_log("glk_put_char_stream_uni(%p, '\\u%04x')", str, ch);

   put_char_uni(str, ch);
}

/*
 * Write a unicode character into a stream. TODO: Used also for Latin-1.
 */
void put_char_uni(strid_t str, glui32 ch)
{
   switch(str->type) {
   case streamtype_Window:
      nanoglk_window_put_char(str->x.window, ch);
      break;

   case streamtype_File:
   case streamtype_File_Uni: // TODO
      fputc(ch, str->x.file);
      break;

   case streamtype_Buffer:
      if(str->x.buf.b.u8 && str->x.buf.pos < str->x.buf.len)
         str->x.buf.b.u8[str->x.buf.pos] = ch;
      str->x.buf.pos++;
      break;

   case streamtype_Buffer_Uni:
      if(str->x.buf.b.u32 && str->x.buf.pos < str->x.buf.len)
         str->x.buf.b.u32[str->x.buf.pos] = ch;
      str->x.buf.pos++;
      break;
   }
}

void glk_put_string(char *s)
{
   nanoglk_log("glk_put_string('%s')", s);
   if(current)
      put_string(current, s);
}

void glk_put_string_uni(glui32 *s)
{
   nanoglk_log("glk_put_string_uni(...)");
   if(current)
      glk_put_string_stream_uni(current, s);
}

void glk_put_string_stream(strid_t str, char *s)
{
   nanoglk_log("glk_put_string_stream(%p, '%s')", str, s);
   put_string(str, s);
}

void glk_put_string_stream_uni(strid_t str, glui32 *s)
{
   nanoglk_log("glk_put_string_stream_uni(%p, ...)", str);
   put_string_uni(str, s);
}

/*
 * Write a Latin-1 string into a stream.
 */
void put_string(strid_t str, char *s)
{
   for(char *p = s; *p; p++)
      put_char_uni(str, *p);
}

/*
 * Write a unicode string into a stream.
 */
void put_string_uni(strid_t str, glui32 *s)
{
   for(glui32 *p = s; *p; p++)
      put_char_uni(str, *p);
}

void glk_put_buffer(char *buf, glui32 len)
{
   nanoglk_log("glk_put_buffer(..., %d)", len);
   if(current)
      put_buffer(current, buf, len);
}

void glk_put_buffer_stream(strid_t str, char *buf, glui32 len)
{
   nanoglk_log("glk_put_buffer_stream(%p, ..., %d)", str, len);
   put_buffer(str, buf, len);
}

void glk_put_buffer_uni(glui32 *buf, glui32 len)
{
   nanoglk_log("glk_put_buffer_uni(..., %d)", len);
   if(current)
      put_buffer_uni(current, buf, len);
}

void glk_put_buffer_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
   nanoglk_log("glk_put_buffer_stream_uni(%p, ..., %d)", str, len);
   put_buffer_uni(str, buf, len);
}

/*
 * Write a Latin-1 buffer into a stream.
 */
void put_buffer(strid_t str, char *buf, glui32 len)
{
   for(glui32 i = 0; i < len; i++)
      put_char_uni(str, buf[i]);
}

/*
 * Write a unicode buffer into a stream.
 */
void put_buffer_uni(strid_t str, glui32 *buf, glui32 len)
{
   for(glui32 i = 0; i < len; i++)
      put_char_uni(str, buf[i]);
}

void glk_set_style(glui32 styl)
{
   nanoglk_log("glk_set_style(%d)", styl);
   if(current)
      set_style(current, styl);
}

void glk_set_style_stream(strid_t str, glui32 styl)
{
   nanoglk_log("glk_set_style_stream(%p, %d)", str, styl);
   set_style(str, styl); // Separation only for Glk logging.
}

/*
 * Set the stype of a stream. Currently only supported for windows.
 *
 * (TODO: Apply some markup when configured by the user?)
 */
void set_style(strid_t str, glui32 styl)
{
   switch(str->type) {
   case streamtype_Window:
      nanoglk_set_style(str->x.window, styl);
      break;

   case streamtype_File:
   case streamtype_File_Uni:
   case streamtype_Buffer:
   case streamtype_Buffer_Uni:
      // ignore
      break;
   }
}

glsi32 glk_get_char_stream(strid_t str)
{
   glsi32 c = get_char_uni(str);
   nanoglk_log("glk_get_char_stream(%p) => %d", str, c);
   return c;
}

glsi32 glk_get_char_stream_uni(strid_t str)
{
   glsi32 c = get_char_uni(str);
   nanoglk_log("glk_get_char_stream_uni(%p) => %d", str, c);
   return c;
}

glsi32 get_char_uni(strid_t str)
{
   switch(str->type) {
   case streamtype_Window:
      nano_warn("glk_get_char_stream_uni not implemented for windows");
      return 0;

   case streamtype_File:
   case streamtype_File_Uni: // TODO
      return fgetc(str->x.file);

   case streamtype_Buffer:
      if(str->x.buf.b.u8 && str->x.buf.pos < str->x.buf.len)
         return str->x.buf.b.u8[str->x.buf.pos++];
      else
         return -1;

   case streamtype_Buffer_Uni:
      if(str->x.buf.b.u32 && str->x.buf.pos < str->x.buf.len)
         return str->x.buf.b.u32[str->x.buf.pos++];
      else
         return -1;
   }
   
   // TODO error
   return 0;
}

glui32 glk_get_buffer_stream(strid_t str, char *buf, glui32 len)
{
   glui32 n;

   switch(str->type) {
   case streamtype_Window:
      nano_warn("glk_get_buffer_stream not implemented for windows");
      n = 0;
      break;

   case streamtype_File:
   case streamtype_File_Uni: // TODO
      n = fread(buf, sizeof(char), len, str->x.file);
      break;

   case streamtype_Buffer:
      n = 0;
      for(glui32 i = 0;
          i < len && str->x.buf.b.u8 && str->x.buf.pos < str->x.buf.len;
          i++) {
         buf[i] = str->x.buf.b.u8[str->x.buf.pos];
         str->x.buf.pos++;
         n++;
      }
      break;

   case streamtype_Buffer_Uni:
      n = 0;
      for(glui32 i = 0;
          i < len && str->x.buf.b.u32 && str->x.buf.pos < str->x.buf.len;
          i++) {
         buf[i] = str->x.buf.b.u32[str->x.buf.pos];
            str->x.buf.pos++;
            n++;
      }
      break;

   default:
      nano_fail("glk_get_buffer_stream: unknown stream type %d", str->type);
      n = 0;
      break;
   }

   nanoglk_log("glk_get_buffer_stream(%p, ..., %d) => %d", str, len, n);
   return n;
}

glui32 glk_get_buffer_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
   nanoglk_log("glk_get_buffer_stream_uni(%p, ..., %d) => ...", str, len);
   nano_fail("glk_get_buffer_stream_uni not implemented");
   return 0;
}

glui32 glk_get_line_stream(strid_t str, char *buf, glui32 len)
{
   nanoglk_log("glk_get_line_stream(%p, ... %d) => ...", str, len);
   nano_fail("glk_get_line_stream not implemented");
   return 0;
}

glui32 glk_get_line_stream_uni(strid_t str, glui32 *buf, glui32 len)
{
   nanoglk_log("glk_get_line_stream_uni(%p, ... %d) => ...", str, len);
   nano_fail("glk_get_line_stream_uni not implemented");
   return 0;
}
