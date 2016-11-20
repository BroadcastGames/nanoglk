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
 * Handling text grid windows. Most functions are called from the
 * general window functions defined in "window.c".
 *
 * TODO: Text rendering en bloc, more efficient? Would restrict
 * rendering to the one fixed width font. (But isn't this actually
 * what grid windows are intended for?). To solve this: distinguish
 * between general and the latter case?
 */

#include "nanoglk.h"

/*
 * The structure used for win->data.
 */
struct textgrid
{
   int cur_x, cur_y;  // current position to insert new text
};


/*
 * Initialize a text grid window.
 */
void nanoglk_wintextgrid_init(winid_t win)
{
   win->data = nano_malloc(sizeof(struct textgrid));
   nanoglk_wintextgrid_clear(win);
}

/*
 * Destroy the specific part of a text grid window.
 */
void nanoglk_wintextgrid_free(winid_t win)
{
   free(win->data);
}

/*
 * Clear a text grid window.
 */
void nanoglk_wintextgrid_clear(winid_t win)
{
   struct textgrid *tg = (struct textgrid*)win->data;
   tg->cur_x = tg->cur_y = 0;

   // The current style is used for the background.
   SDL_FillRect(nanoglk_mainwindow->nanoglk_surface, &win->area,
                SDL_MapRGB(nanoglk_mainwindow->nanoglk_surface->format,
                           win->bg[win->cur_styl].r, win->bg[win->cur_styl].g,
                           win->bg[win->cur_styl].b));
}

static int wtg_resize_callcount = 0;

/*
 * Resize a text grid window.
 */
void nanoglk_wintextgrid_resize(winid_t win, SDL_Rect *area)
{
   wtg_resize_callcount++;
   SDL_UpdateWindowSurface(nanoglk_mainwindow->nanoglk_output_window);

printf("WTGR saving %d %d %d %d cc %d\n", win->area.x, win->area.y, MIN(win->area.w, area->w), MIN(win->area.h, area->h), wtg_resize_callcount);
   // Save the current contents in a new surface "s".
   SDL_Rect r1 = { win->area.x, win->area.y,
                   MIN(win->area.w, area->w), MIN(win->area.h, area->h) };
   SDL_Rect r2 = { 0, 0, r1.w, r2.w };
printf("WTGR create_surface %d %d %d cc %d\n", r2.w, r2.h, nanoglk_screen_depth, wtg_resize_callcount);
   SDL_Surface *s =
      SDL_CreateRGBSurface(SDL_SWSURFACE, r2.w, r2.h, nanoglk_screen_depth,
                           0, 0, 0, 0); /* TODO Argument [RGB]mask? Currently
                                           passed 0. */
   SDL_BlitSurface(nanoglk_mainwindow->nanoglk_surface, &r1, s, &r2);

   SDL_UpdateWindowSurface(nanoglk_mainwindow->nanoglk_output_window);

switch (wtg_resize_callcount) {
   case 6:
   //case 7:
   printf("WTGR skipA SUPER HACK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
   break;
   default:
   win->area = *area;
   // Clear new area ...
   SDL_FillRect(nanoglk_mainwindow->nanoglk_surface, &win->area,
                SDL_MapRGB(nanoglk_mainwindow->nanoglk_surface->format,
                           win->bg[win->cur_styl].r, win->bg[win->cur_styl].g,
                           win->bg[win->cur_styl].b));
};

   // ... and copy the old contents.
printf("WTGR copy %d %d %d %d cc %d\n", win->area.x, win->area.y, r1.w, r2.w, wtg_resize_callcount);
   SDL_Rect r3 = { win->area.x, win->area.y, r1.w, r2.w };
   SDL_BlitSurface(s, &r2, nanoglk_mainwindow->nanoglk_surface, &r3);
   SDL_FreeSurface(s);

SDL_UpdateWindowSurface(nanoglk_mainwindow->nanoglk_output_window);
printf("wintextgrid.c !!!!!!!!!!!! did I JUST NUKE A000\n");
switch (wtg_resize_callcount) {
    case 1:
    case 7:
    SDL_Delay(6000);
    break; 
}
   // (One could also copy directly, and clear new areas, when the window
   // has become larger; but this would be more complicated.)
}

/*
 * Move the cursor within a text grid window.
 */
void nanoglk_wintextgrid_move_cursor(winid_t win, glui32 xpos, glui32 ypos)
{
   struct textgrid *tg = (struct textgrid*)win->data;
   tg->cur_x = xpos * nanoglk_grid_font[style_Normal]->space_width;
   tg->cur_y = ypos * nanoglk_grid_font[style_Normal]->text_height;
}

/*
 * Flush a text grid window, i. e. display all pending output.
 */
void nanoglk_wintextgrid_flush(winid_t win)
{
   // Nothing to to, everything is drawn immediately. (May change, see TODO
   // at the beginning on this file.)
}

/*
 * Begin a new line in a text window.
 */
static void new_line(winid_t win)
{
   struct textgrid *tg = (struct textgrid*)win->data;
   tg->cur_x = 0;
   tg->cur_y += nanoglk_grid_font[style_Normal]->text_height;
}

/*
 * Puts a character into a text grid window.
 */
void nanoglk_wintextgrid_put_char(winid_t win, glui32 c)
{
   struct textgrid *tg = (struct textgrid*)win->data;

   // Make logging more readable by printing printable characters directly.
   if(c >= 32 && c <= 127)
      nano_trace("nanoglk_wintextgrid_put_char(%p, '%c') at (%d, %d)",
                 win, c, tg->cur_x, tg->cur_y);
   else if(c >= 32 && c <= 127)
      nano_trace("nanoglk_wintextgrid_put_char(%p, 0x%04x) at (%d, %d)",
                 win, c, tg->cur_x, tg->cur_y);

   // Width and height of a grid unit. Taken from the "normal" font, in the hope
   // that all fonts for grid windows have exactly same measurements.
   int gw = nanoglk_grid_font[style_Normal]->space_width;
   int gh = nanoglk_grid_font[style_Normal]->text_height;

   if(c == '\n')
      new_line(win);

   // No scrolling! Anything below the bottom border is ignored.
   if(tg->cur_y < win->area.h) {
      Uint16 str[2] = { c, 0 };
      SDL_Surface *t =
         TTF_RenderUNICODE_Shaded(nanoglk_grid_font[win->cur_styl]->font, str,
                                  win->fg[win->cur_styl],
                                  win->bg[win->cur_styl]);
      SDL_Rect r1 = { 0, 0, gw, gh };
      SDL_Rect r2 = { tg->cur_x, tg->cur_y, gw, gh };
      SDL_BlitSurface(t, &r1, nanoglk_mainwindow->nanoglk_surface, &r2);
      SDL_FreeSurface(t);

      tg->cur_x += gw;
      if(tg->cur_x >= win->area.w) // right border of the window
         new_line(win);
   }
}

/*
 * Get a Unicode character from a text grid window. Called by
 * nanoglk_window_get_char_uni().
 */
glui32 nanoglk_wintextgrid_get_char_uni(winid_t win)
{
   while(1) {
      SDL_Event event;
      nano_wait_event(&event);
      switch(event.type) {
      case SDL_KEYDOWN:
         return nanoglk_window_char_sdl_to_glk(&event.key.keysym);
      }
   }
}

/*
 * Read any line from a text grid window. For most arguments, see
 * nano_input_text16() in "ui.h". Return the length.
 */
glui32 nanoglk_wintextgrid_get_line16(winid_t win, Uint16 *text,
                                      int max_len, int max_char)
{
   // TODO Not implemented.
   return strlen16(text);
}
