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
 * Handling text buffer windows. Most functions are called from the
 * general window functions defined in "window.c".
 *
 * ToDo SDL2: instead of passing in surface have a SDL_WindowHolder struct
 *    and pass in SDL_Window, SDL_Renderer, Surface, etc. And an int
 *    to indicate if running in Surface mode or Texture mode.
 *
 * TODO (aside from several glitches): perhaps preservation of
 * text. This would make other features possible: e. g. proper
 * resizing and scrolling.
 */

#include "nanoglk.h"

#define MAX_WORD_LEN 2000
#define MAX_HISTORY  100

/*
 * The structure used for win->data.
 */
struct textbuffer
{
   int cur_x, cur_y;      /* the position where to insert new text,
                             relative to win->area.x and
                             win->area.y */
   int line_height;       /* the height of the line currently drawn:
                             the maximum of all word heights */
   int last_line_height;  // TODO neccessary?
   int read_until;        /* The number of pixels the user has already
                             read (or, has had the chance to read,
                             because he had to conform by pressing a
                             key or something similar), starting from
                             the top of the window. When scrolling
                             down, this area can so simply
                             overwritten, while anything below should
                             be confirmed by the user ("- more -" and
                             wait for a key). */

   // The following three variables contain the word to be rendered.
   Uint16 curword[MAX_WORD_LEN + 1];    /* The characters. See
                                           render_word(), why one more
                                           character is needed (for
                                           the 0-termination). */
   glui32 curword_styles[MAX_WORD_LEN]; // style_*, as defined in "glk.h"
   int curword_len;                     // the number of chacters.

   glui32 space_styl; /* The style (style_*, as defined in "glk.h")
                         of the space before the current word. -1 when
                         there is no space. */
};

static void add_word(winid_t win, SDL_Surface **word);
static SDL_Surface **render_word(winid_t win);
static int width_word(SDL_Surface **t);
static void free_word(SDL_Surface **t);
static void new_line(winid_t win);
static void ensure_space(winid_t win, int space);
static void wait_for_key(void);
static void user_has_read(winid_t win);

/*
 * The input history. TODO Should be either window specific or global,
 * and so also used for grid buffers, as soon as these are implemented
 * fully.
 */
static int num_history = 0;
static Uint16* history[MAX_HISTORY];

/*
 * Initialize a text buffer window.
 */
void nanoglk_wintextbuffer_init(winid_t win)
{
   win->data = nano_malloc(sizeof(struct textbuffer));
   nanoglk_wintextbuffer_clear(win);
}

/*
 * Clear a text buffer window.
 */
void nanoglk_wintextbuffer_clear(winid_t win)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;
   tb->cur_x = tb->cur_y = tb->line_height = tb->last_line_height
      = tb->curword_len = tb->read_until = 0;
   tb->space_styl = -1;
   nano_trace("win %p (clear): space_styl = %d", win, tb->space_styl);
   SDL_FillRect(nanoglk_surface, &win->area,
                SDL_MapRGB(nanoglk_surface->format,
                           win->bg[win->cur_styl].r, win->bg[win->cur_styl].g,
                           win->bg[win->cur_styl].b));
}

/*
 * Destroy the specific part of a text buffer window.
 */
void nanoglk_wintextbuffer_free(winid_t win)
{
   free(win->data);
}

/*
 * Resize a text buffer window.
 */
void nanoglk_wintextbuffer_resize(winid_t win, SDL_Rect *area)
{
   nano_trace("nanoglk_wintextbuffer_resize(%p { %d, %d, %d x %d }, "
              "{ %d, %d, %d x %d } )",
              win, win->area.x, win->area.y, win->area.w, win->area.h,
              area->x, area->y, area->w, area->h);

   if(area->w != win->area.w) {
      // Width has changed. Since the text is not preserved, it cannot be
      // rewrapped, so the window is simply cleared (in the hope that this
      // does not happen very often).
      win->area = *area;
      nanoglk_wintextbuffer_clear(win);
   } else {
      nano_trace("   same width");
      if(area->h == win->area.h) {
         // Window size has not changed at all. Simply copy.
         nano_trace("      same height");
         SDL_BlitSurface(nanoglk_surface, &win->area, nanoglk_surface, area);
      } else if(area->h > win->area.h) {
         nano_trace("      height larger");
         // Window has become taller.
         // First, copy the old area ...
         SDL_Rect r1 = { area->x, area->y, win->area.w, win->area.h };
         // ... then clear the difference.
         SDL_BlitSurface(nanoglk_surface, &win->area, nanoglk_surface, &r1);
         SDL_Rect r2 = { area->x + win->area.h, area->y,
                         win->area.w, area->h - win->area.h };
         SDL_FillRect(nanoglk_surface, &r2,
                      SDL_MapRGB(nanoglk_surface->format,
                                 win->bg[win->cur_styl].r,
                                 win->bg[win->cur_styl].g,
                                 win->bg[win->cur_styl].b));
      } else {
         nano_trace("      height smaller");
         // Window has become shallower.
         struct textbuffer *tb = (struct textbuffer*)win->data;
         if(area->h >= tb->cur_y + tb->line_height) {
            nano_trace("          content fits");
            // Content still fits in new space.
            SDL_Rect r = { win->area.x, win->area.y, area->w, area->h };
            SDL_BlitSurface(nanoglk_surface, &r, nanoglk_surface, area);
         } else {
            // Part of content gets lost. Again, this hopefully does not happen
            // too often.
            nano_trace("          content lost");
            int d = tb->cur_y + tb->line_height - area->h;
            SDL_Rect r = { win->area.x, win->area.y + d, area->w, area->h };
            SDL_BlitSurface(nanoglk_surface, &r, nanoglk_surface, area);
            tb->cur_y = MAX(tb->cur_y - d, 0);
            tb->read_until = MAX(tb->read_until - d, 0);
         }
      }

      win->area = *area;
   }

   nano_trace("finished: nanoglk_wintextbuffer_resize(...)");
}

/*
 * Flush a text buffer window, i. e. display all pending output.
 */
void nanoglk_wintextbuffer_flush(winid_t win)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;
   
   // TODO In some cases, a part of an unfinished word is broken here. Check
   // again.

   if(tb->curword_len > 0) {
      SDL_Surface **t = render_word(win);
      add_word(win, t);
      free_word(t);
   }

   tb->curword_len = 0;
}

/*
 * Add a rendered word, i. e. an array of SDL surfaces. Typically text
 * (as returned by render_word()), but "word" may also contain images.
 */
void add_word(winid_t win, SDL_Surface **word)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;

   int w_space =
      tb->space_styl != -1 ?
      nanoglk_buffer_font[tb->space_styl]->space_width : 0;
   nano_trace("win %p (add word): space width = %d", win, w_space);
   int w_word = width_word(word);
   if(tb->cur_x != 0 && tb->cur_x + w_space + w_word > win->area.w)
      // word does not fit -> break line
      new_line(win);
   else if(tb->cur_x != 0)
      // word fits -> only add space before
      tb->cur_x += w_space;
   
   int i;
   for(i = 0; word[i]; i++) {
      // Simply copy on screen surface.

      // TODO Notice that all characters are aligned at the top (rendered at
      // the same vertical position), not at the base line.
      ensure_space(win, word[i]->h);
      
      SDL_Rect rt = { 0,  0, word[i]->w, word[i]->h };
      SDL_Rect rs = { win->area.x + tb->cur_x, win->area.y + tb->cur_y,
                      word[i]->w, word[i]->h };
      SDL_BlitSurface(word[i], &rt, nanoglk_surface, &rs);
      tb->cur_x += word[i]->w;
      tb->line_height = MAX(tb->line_height, word[i]->h);
   }
}

/*
 * Puts a character into a text buffer window.
 */
void nanoglk_wintextbuffer_put_char(winid_t win, glui32 c)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;

   // Make logging more readable by printing printable characters directly.
   if(c >= 32 && c <= 127)
      nano_trace("nanoglk_wintextbuffer_put_char(%p, '%c') at (%d, %d)",
                 win, c, tb->cur_x, tb->cur_y);
   else if(c >= 32 && c <= 127)
      nano_trace("nanoglk_wintextbuffer_put_char(%p, 0x%04x) at (%d, %d)",
                 win, c, tb->cur_x, tb->cur_y);

   switch(c) {
   case ' ':
      // End of word, but space has to be preserved.
      nanoglk_wintextbuffer_flush(win); /* Note: At this point,
                                           tb->space_styl refers to
                                           the space *before* the word
                                           which is going to be displayed. */
      tb->space_styl = win->cur_styl;   /* And now, it refers to the
                                           space *after* this word
                                           (which was just displayed). */
      nano_trace("win %p (add space): space_styl = %d", win, tb->space_styl);
      break;

   case '\n':
      // End of line, so end of word.
      nanoglk_wintextbuffer_flush(win);
      new_line(win);
      break;
      
   default:
      // Another character of the current word.
      if(tb->curword_len < MAX_WORD_LEN) {
         tb->curword[tb->curword_len] = c;
         tb->curword_styles[tb->curword_len] = win->cur_styl;
         tb->curword_len++;
      }
      break;
   }
}

/*
 * Put an image into a text buffer window. (The complicated stuff is
 * done in "image.c".
 */
void nanoglk_wintextbuffer_put_image(winid_t win, SDL_Surface *image,
                                     glsi32 val1, glsi32 val2)
{
   nanoglk_wintextbuffer_flush(win);

   // TODO Currently no alignment etc., just inserstion into the text flow.
   SDL_Surface *word[2] = { image, NULL };
   add_word(win, word);
}

/*
 * Get a Unicode character from a text buffer window. Called by
 * nanoglk_window_get_char_uni().
 */
glui32 nanoglk_wintextbuffer_get_char_uni(winid_t win)
{
   while(1) {
      SDL_Event event;
      nano_wait_event(&event);
      switch(event.type) {
      case SDL_KEYDOWN:
         user_has_read(win);
         return nanoglk_window_char_sdl_to_glk(&event.key.keysym);
      }
   }
}

/*
 * Read any line from a text buffer window. For most arguments, see
 * nano_input_text16() in "ui.h". Return the length.
 */
glui32 nanoglk_wintextbuffer_get_line16(winid_t win, Uint16 *text,
                                        int max_len, int max_char)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;

   nanoglk_wintextbuffer_flush(win); /* Not neccessary currently, since this
                                        function is always called from
                                        glk_select(), but we do not want to
                                        rely on this. */

   // Show the pending space and ensure a minimal width for the input.
   int w_space =
      tb->space_styl != -1 ?
      nanoglk_buffer_font[tb->space_styl]->space_width : 0;
   nano_trace("win %p (get line): space width = %d", win, w_space);

   // Minimal width for the input: a third of the window width, but at least
   // 10 pixels, unless the window is less than 10 pixels wide.
   // TODO Make this configurable.
   int w_input = MAX(win->area.w / 3, MIN(win->area.w, 10));
   if(tb->cur_x != 0 && tb->cur_x + w_space + w_input > win->area.w)
      // word does not fit -> break line
      new_line(win);
   else if(tb->cur_x != 0)
      // word fits -> only add space before
      tb->cur_x += w_space;

   if(num_history >= MAX_HISTORY) {
      // History buffer is full: remove oldest entry.
      free(history[0]);
      memmove(history, history + 1, (MAX_HISTORY - 1) * sizeof(Uint16*));
      num_history = MAX_HISTORY - 1;
   }

   /*
    * Dealing with the input history is done here (TODO should perhaps
    * be moved into the "misc" part.
    *
    * How history works:
    *
    * 1. You can move in the history using up and down, starting
    *    always at a new, empty entry at the end.
    *
    * 2. You can modify entries; as long as you do not press enter,
    *    your changes are preserved when pressing up and down.
    *
    * 3. If you press enter, the line, which you are currently
    *    editing, is added at the end of the history (but only when it
    *    is different from the current last line, and if it is not
    *    empty). All changes in the history are discarded and get
    *    lost.
    *
    * The current position is kept in "history_pos". The history
    * itself is stored in the global (but static) array "history",
    * while the changes are stored in "history_repl". The latter
    * contains NULL at the beginning, so you will often see something
    * like:
    *
    *    history_repl[history_pos] ?
    *       history_repl[history_pos] : history[history_pos]
    *
    * which results in the original (when never touched) or modified
    * entry in the history.
    */

   int state = -1; // As passed to nano_input_text16().
   int history_pos = num_history;     // See above.
   Uint16* history_repl[MAX_HISTORY]; // See above.
   for(int i = 0; i < MAX_HISTORY; i++)
      history_repl[i] = NULL;

   while(1) {
      SDL_Event event;
      nano_input_text16(nanoglk_surface, &event, text, max_len, max_char,
                        win->area.x + tb->cur_x, win->area.y + tb->cur_y,
                        win->area.w - tb->cur_x,
                        nanoglk_buffer_font[style_Input]->text_height,
                        nanoglk_buffer_font[style_Input]->font,
                        win->fg[style_Input], win->bg[style_Input],
                        &state);
      if(event.type == SDL_KEYDOWN)
         switch(event.key.keysym.sym) {
         case SDLK_RETURN:
            user_has_read(win);
            new_line(win);

            for(int i = 0; i < MAX_HISTORY; i++)
               // history_repl is discarded. (And "text" can be used below.)
               if(history_repl[i])
                  free(history_repl[i]);

            /* Store input at the end of the history, but only when it
             * is different from the current last line, and if it is
             * not empty.
             */
            if(text[0] &&
               (num_history == 0 ||
                strcmp16(text, history[num_history - 1]) != 0)) {
               history[num_history] = strdup16(text);
               num_history++;
            }

            return strlen16(text);
            
         case SDLK_UP:
            if(history_pos > 0) {
               // Store changes.
               if(history_repl[history_pos])
                  free(history_repl[history_pos]);
               history_repl[history_pos] = strdup16(text);

               history_pos--;
               // Display (possibly changed) history entry.
               // TODO limit length
               strcpy16(text, history_repl[history_pos] ?
                        history_repl[history_pos] : history[history_pos]);
               state = -1;
            }
            break;

         case SDLK_DOWN:
            if(history_pos < num_history) {
               // Store changes. 
               if(history_repl[history_pos])
                  free(history_repl[history_pos]);
               history_repl[history_pos] = strdup16(text);

               history_pos++;
               // Display (possibly changed) history entry.
               // TODO limit length
               strcpy16(text, history_repl[history_pos] ?
                        history_repl[history_pos] : history[history_pos]);
               state = -1;
            }
            break;

         default:
            break;
         }
   }
}

/*
 * Render the current word, i. e. return a NULL-terminated array of
 * SDL surfaces for the different parts (with different styles) of the
 * current word.
 */
SDL_Surface **render_word(winid_t win)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;

   // First, calculate how many times the style changes, and so the length
   // of the returned array.
   int i, num_parts = 0;
   for(i = 0; i < tb->curword_len; i++)
      if(i == 0 || tb->curword_styles[i] != tb->curword_styles[i - 1])
         num_parts++;

   // Render the parts of the word.
   SDL_Surface **t =
      (SDL_Surface**)nano_malloc((num_parts + 1) * sizeof(SDL_Surface*));
   int word_start = 0, word_end;
   for(i = 0; i < num_parts; i++) {
      Uint16 saved; /* Since the TTF rendering functions expect a
                       0-terminated Uint16 array, we simply set the
                       end of a part to 0, but save the character
                       (which is the first character of the next part)
                       before in this variable. */

      if(i < num_parts - 1) {
         word_end = word_start + 1;
         // Search for change of style.
         while(tb->curword_styles[word_end - 1] == tb->curword_styles[word_end])
            word_end++;
         saved = tb->curword[word_end];
         tb->curword[word_end] = 0;
      } else
         tb->curword[tb->curword_len] = 0; // Last part, saving not neccessary.
      
      int styl = tb->curword_styles[word_start];
      t[i] =
         TTF_RenderUNICODE_Shaded(nanoglk_buffer_font[styl]->font,
                                  tb->curword + word_start,
                                  win->fg[styl], win->bg[styl]);
      
      if(i < num_parts - 1)
         tb->curword[word_end] = saved;

      word_start = word_end;
   }

   t[num_parts] = NULL;
   return t;
}

/*
 * Calculate the width of a rendered word (array of SDL surfaces) as
 * created by render_word().
 */
int width_word(SDL_Surface **t)
{
   int i, w = 0;
   for(i = 0; t[i]; i++)
      w += t[i]->w;
   return w;
}

/*
 * Free a rendered word (array of SDL surfaces) as created by
 * render_word().
 */
void free_word(SDL_Surface **t)
{
   int i;
   for(i = 0; t[i]; i++)
      SDL_FreeSurface(t[i]);
   free(t);
}

/*
 * Begin a new line. Either forced (by putting a newline character
 * into the window) or because a new word does not fit into the
 * current line.
 */
void new_line(winid_t win)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;

   // Height of the line: If not yet changed (and so 0), take the height
   // of the current font. (Otherwise empty lines would not be visible,
   // since tb->line_height is 0 in this case.)
   //
   // TODO Rework needed. Is this correct? Actually, the height of the
   // next line is needed (but it is likely to be equally high).

   int h = tb->line_height > 0 ?
      tb->line_height : nanoglk_buffer_font[win->cur_styl]->text_height;
   ensure_space(win, h);

   tb->cur_x = 0;
   tb->cur_y += h;

   tb->last_line_height = tb->line_height;
   tb->line_height = 0;
   tb->space_styl = -1;
   nano_trace("win %p (new line): space_styl = %d", win, tb->space_styl);
}

/*
 * Makes sure that there is some space at the bottom of the window. If
 * there is not enough space, scroll down. Also, make sure that the
 * user was able to read all text, i. e. regard "read_until"; display
 * a "- more -" message and wait for a key when neccessary.
 */
void ensure_space(winid_t win, int space)
{
   // TODO Rework needed.
   struct textbuffer *tb = (struct textbuffer*)win->data;

   nano_trace("ensure_space(%p, %d) [cur_y = %d]", win, space, tb->cur_y);
   
   if(tb->cur_y + space > win->area.h) {
      // Not enough space.
      int d = tb->cur_y + space - win->area.h; // What is missing.

      if(d > tb->read_until) {
         // TODO Maybe scroll some bit already?
         // Display "- more -" and wait for a key.
         Uint16 more[] = { 0x2014, ' ', 'm', 'o', 'r', 'e', ' ', 0x2014, 0 };
         SDL_Surface *t =
            TTF_RenderUNICODE_Shaded(nanoglk_buffer_font[style_Input]->font,
                                     more, win->fg[style_Input],
                                     win->bg[style_Input]);

         nano_save_window(nanoglk_surface,
                          win->area.x, win->area.y + win->area.h - t->h,
                          win->area.w, t->h);
         nano_fill_rect(nanoglk_surface, win->bg[win->cur_styl],
                        win->area.x, win->area.y + win->area.h - t->h,
                        win->area.w, t->h);
         
         SDL_Rect r1 = { 0, 0, t->w, t->h };
         SDL_Rect r2 = { win->area.x, win->area.y + win->area.h - t->h,
                         t->w, t->h };
         SDL_BlitSurface(t, &r1, nanoglk_surface, &r2);
         SDL_FreeSurface(t);

         // SDL1.2: SDL_Flip(nanoglk_surface);
         SDL_UpdateWindowSurface(nanoglk_output_window);

         wait_for_key();

         // TODO Clarify why a simple "user_has_read(win)" does not work here.
         tb->read_until = tb->cur_y - tb->last_line_height;

         nano_restore_window(nanoglk_surface);
      }
      
      // Copy (scroll down).
      SDL_Rect r1 = { win->area.x, win->area.y + d,
                      win->area.w, win->area.h - d };
      SDL_Rect r2 = { win->area.x, win->area.y, win->area.w, win->area.h - d };
      SDL_BlitSurface(nanoglk_surface, &r1, nanoglk_surface, &r2);

      // Clear new, free area.
      SDL_Rect r = { win->area.x, win->area.y + win->area.h - d,
                     win->area.w, d };
      SDL_FillRect(nanoglk_surface, &r,
                   SDL_MapRGB(nanoglk_surface->format,
                              win->bg[win->cur_styl].r,
                              win->bg[win->cur_styl].g,
                              win->bg[win->cur_styl].b));

      tb->cur_y -= d;
      tb->read_until -= d;
   }
}

/*
 * Simply wait for a key (space or return).
 */
void wait_for_key(void)
{
   while(1) {
      SDL_Event event;
      nano_wait_event(&event);
      if(event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_SPACE ||
                                       event.key.keysym.sym == SDLK_RETURN))
         return;
   }
}

/*
 * Called when it is sure that the user has read (or better: has had
 * the chance) to read) all the text on the screen.
 */
void user_has_read(winid_t win)
{
   struct textbuffer *tb = (struct textbuffer*)win->data;
   tb->read_until = tb->cur_y;
}
