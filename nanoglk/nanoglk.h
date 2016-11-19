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
 * Single header file for everything in the "nanoglk" directory.
 */

#ifndef __NANOGLK_H__
#define __NANOGLK_H__

// TODO: Somehow, strdup() and basename() are not defined otherwise.
// (Are these non-standard?)
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "glk.h"
#include "gi_dispa.h"
#include "gi_blorb.h"
#include "glkstart.h"
#include "misc/misc.h"

/*
 * These two macros, ADD() and UNLINK(), are used to keep objects in a
 * global list. In the file where a certain object type "foo_t" is
 * managed (especially where glk_foo_iterate() is defined), there
 * should be the following *static* definition:
 *
 * static foo_t first = NULL, last = NULL;
 *
 * Furthermore, foo_t must contain the fields "prev" and "next"; these
 * macros take care of the values, so nothing more is do be done.
 *
 * ADD() should be called after a new instance of "foo_t" has been
 * created; likewise UNLINK(), shortly before an instance is
 * destroyed.
 */

#define ADD(node) do {                          \
      (node)->next = NULL;                      \
                                                \
      if(last) {                                \
         last->next = (node);                   \
         (node)->prev = last;                   \
         last = (node);                         \
      } else {                                  \
         (node)->prev = NULL;                   \
         last = first = (node);                 \
      }                                         \
   } while(0)

#define UNLINK(node) do {                       \
      if((node)->prev)                          \
         (node)->prev->next = (node)->next;     \
      if((node)->next)                          \
         (node)->next->prev = (node)->prev;     \
                                                \
      if((node) == first)                       \
         first = (node)->next;                  \
      if((node) == last)                        \
         last = (node)->prev;                   \
   } while(0)

/*
 * Definition of the opaque objects. Unfortunately, they cannot be
 * kept in the respective C files only.
 *
 * Some members all structures have:
 *
 * - prev, next: Links within the list of all objects of this
 *               type. See ADD() and UNLINK() for more informations.
 * - rock:       Simply the rock of the object.
 * - disprock:   The rock used by the dispatching layer.
 *
 */

struct glk_fileref_struct
{
   frefid_t prev, next;

   glui32 usage; // fileusage_* in "glk.h"
   glui32 rock;
   gidispatch_rock_t disprock;

   char *name; /* UTF-8 encoded. TODO should be clarified, see also comment at
                  the beginning of "misc/filesel.c". */
};

struct glk_stream_struct
{
   strid_t prev, next;

   enum { streamtype_Window,
          streamtype_File, streamtype_File_Uni,
          streamtype_Buffer, streamtype_Buffer_Uni } type;
   glui32 rock;
   gidispatch_rock_t disprock, arrrock;

   union {
      winid_t window;  // streamtype_Window
      FILE *file;      // streamtype_File, streamtype_File_Uni
      struct {
         union { char *u8; glui32 *u32; } b;
         int pos, len;
      } buf;           // streamtype_Buffer, streamtype_Buffer_Uni
   } x;
};

struct glk_window_struct
{
   winid_t parent;                // NULL for the root window
   winid_t left, right;           /* Children of a pair window (otherwise NULL).
                                     Notice that "left" and "right do not have
                                     any geometrical meaning; instead, "left"
                                     is always the split window, "right" the
                                     newly created. When using method
                                     "winmethod_Left", the "right" child will
                                     indeed be on the left geometrical side. */
   glui32 method, size;           /* Only meaningful for the right window, since
                                     a newly created window becomes the *right*
                                     child of the newly created pair window.
                                     (The split window becomes the left child.)
                                  */
   glui32 wintype;                // wintype_*, as defined in "glk.h"
   glui32 cur_styl;               // style_*, as defined in "glk.h"
   glui32 rock;
   gidispatch_rock_t disprock, arrrock;
   strid_t stream;                // the window stream

   SDL_Color fg[style_NUMSTYLES]; /* The foreground colors for the styles of
                                     this window. See comment at the beginning
                                     of "windows.c" for more details. */
   SDL_Color bg[style_NUMSTYLES]; /* The background colors for the styles of
                                     this window. See comment at the beginning
                                     of "windows.c" for more details. */
   SDL_Rect area;                 /* the area this window occupies within
                                     nanoglk_surface */

   void *data;                    /* Additional data depending on types. See
                                     "wintextbuffer.c", "wintextgrid.c", and
                                     "wingraphics.c". */
};

struct glk_schannel_struct
{
   schanid_t prev, next;

   glui32 rock;
   gidispatch_rock_t disprock;

   // Otherwise empty, no functionality.
};

/*
 * Wrapper for SDL TTF fonts.
 */
struct nanoglk_font
{
   TTF_Font *font;          
   SDL_Color fg, bg; /* foreground and background, in which text with this
                        font should be rendered */
   int space_width;  /* width of a space (result of rendering); see new_font()
                        in "main.c" */
   int text_height;  /* font height (result of rendering); see new_font() in
                        "main.c" */
};

/*
 * The definitions for these variables can be found in "main.c"; see there
 * for more informations.
 */

extern struct nanoglk_font *nanoglk_buffer_font[style_NUMSTYLES];
extern struct nanoglk_font *nanoglk_grid_font[style_NUMSTYLES];

extern struct nanoglk_font *nanoglk_ui_font;
extern SDL_Color nanoglk_ui_input_fg_color, nanoglk_ui_input_bg_color;
extern SDL_Color nanoglk_ui_list_i_fg_color, nanoglk_ui_list_i_bg_color;
extern SDL_Color nanoglk_ui_list_a_fg_color, nanoglk_ui_list_a_bg_color;

extern int nanoglk_screen_width, nanoglk_screen_height, nanoglk_screen_depth;
extern int nanoglk_filesel_width, nanoglk_filesel_height;
extern SDL_Surface *nanoglk_surface;
extern SDL_Window *nanoglk_output_window;
extern SDL_Renderer *nanoglk_output_renderer;
extern SDL_Texture *nanoglk_output_texture;

extern double nanoglk_factor_horizontal_fixed, nanoglk_factor_vertical_fixed;
extern double nanoglk_factor_horizontal_proportional;
extern double nanoglk_factor_vertical_proportional;

void nanoglk_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

gidispatch_rock_t nanoglk_call_regi_obj(void *obj, glui32 objclass);
void nanoglk_call_unregi_obj(void *obj, glui32 objclass,
                             gidispatch_rock_t objrock);
gidispatch_rock_t nanoglk_call_regi_arr(void *array, glui32 len,
                                        char *typecode);
void nanoglk_call_unregi_arr(void *array, glui32 len, char *typecode,
                             gidispatch_rock_t objrock);


void nanoglk_window_init(int width, int height, int depth);
void nanoglk_window_put_char(winid_t win, glui32 c);
void nanoglk_window_flush_all(void);
glui32 nanoglk_window_get_char(winid_t win);
glui32 nanoglk_window_get_char_uni(winid_t win);
glui32 nanoglk_window_char_sdl_to_glk(SDL_Keysym *keysym);
glui32 nanoglk_window_get_line(winid_t win, char *buf, glui32 maxlen,
                               glui32 initlen);
glui32 nanoglk_window_get_line_uni(winid_t win, glui32 *buf, glui32 maxlen,
                                   glui32 initlen);
void nanoglk_set_style(winid_t win, glui32 styl);

void nanoglk_wintextbuffer_init(winid_t win);
void nanoglk_wintextbuffer_free(winid_t win);
void nanoglk_wintextbuffer_clear(winid_t win);
void nanoglk_wintextbuffer_resize(winid_t win, SDL_Rect *area);
void nanoglk_wintextbuffer_flush(winid_t win);
void nanoglk_wintextbuffer_put_char(winid_t win, glui32 c);
void nanoglk_wintextbuffer_put_image(winid_t win, SDL_Surface *image,
                                     glsi32 val1, glsi32 val2);
glui32 nanoglk_wintextbuffer_get_char_uni(winid_t win);
glui32 nanoglk_wintextbuffer_get_line16(winid_t win, Uint16 *text,
                                        int max_len, int max_char);

void nanoglk_wintextgrid_init(winid_t win);
void nanoglk_wintextgrid_free(winid_t win);
void nanoglk_wintextgrid_clear(winid_t win);
void nanoglk_wintextgrid_resize(winid_t win, SDL_Rect *area);
void nanoglk_wintextgrid_move_cursor(winid_t win, glui32 xpos, glui32 ypos);
void nanoglk_wintextgrid_flush(winid_t win);
void nanoglk_wintextgrid_put_char(winid_t win, glui32 c);
glui32 nanoglk_wintextgrid_get_char_uni(winid_t win);
glui32 nanoglk_wintextgrid_get_line16(winid_t win, Uint16 *text,
                                        int max_len, int max_char);

void nanoglk_wingraphics_init(winid_t win);
void nanoglk_wingraphics_free(winid_t win);
void nanoglk_wingraphics_clear(winid_t win);
void nanoglk_wingraphics_resize(winid_t win, SDL_Rect *area);
void nanoglk_wingraphics_flush(winid_t win);
void nanoglk_wingraphics_erase_rect(winid_t win,
                                    glsi32 left, glsi32 top,
                                    glui32 width, glui32 height);
void nanoglk_wingraphics_fill_rect(winid_t win, glui32 color,
                                   glsi32 left, glsi32 top,
                                   glui32 width, glui32 height);
void nanoglk_wingraphics_set_background_color(winid_t win, glui32 color);
void nanoglk_wingraphics_put_image(winid_t win, SDL_Surface *image,
                                   glsi32 val1, glsi32 val2);

strid_t nanoglk_stream_new(glui32 type, glui32 rock);
void nanoglk_stream_set_current(strid_t str);

#endif // __NANOGLK_H__
