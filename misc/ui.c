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
 * Some UI stuff. The file selection is in a seperate file, "filesel.c".
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

/*
 * Load a font, given by a path (where to find the TTF file), a family name
 * (e. g. "DejaVuSerif"), a font weight (0 = normal, 1 = bold) and a style
 * (0 = normal, otherwise ITALICS (1) or OBLIQUE (2). see "misc.h").
 *
 * It tries to guess which font file contains what and which eventually fits
 * best.
 */
TTF_Font *nano_load_font(const char *path, const char *family,
                         int weight, int style, int size)
{
   // TODO cache contents of directory?

   int len_family = strlen(family);
   char *first_choice = NULL, *second_choice = NULL;
   int first_choice_len, second_choice_len;

   DIR *dir = opendir(path);
   if(!dir)
      nano_fail("Cannot read directory '%s': %s", path, strerror(errno));
   else {
      struct dirent *de;
      do {
         if((de = readdir(dir))) {
            // TODO check for suffix ".ttf"
            int belongs_to_family = 1;
            
            for(int i = 0; belongs_to_family && family[i]; i++) {
               // TODO tolower() does not probably work with UTF-8
               // check for de->d_name[i] == 0 is implied
               if(tolower(de->d_name[i]) != tolower(family[i]))
               belongs_to_family = 0;
            }
            
            if(!belongs_to_family)
               continue;
            
            char *rest = de->d_name + len_family;
         
            if(weight && strcasestr(rest, "bold") == NULL)
               continue;
        
            // "First choice" means that the style is exactly what was
            // requested. "Second choise" means that the style has been
            // replaced by something similar: italics by oblique or oblique
            // by italics.
            
            int has_style, is_first_choice;
            if(style == 0)
               has_style = is_first_choice = 1;
            else {
               switch(style) {
               case ITALICS:
                  if(strcasestr(rest, "italic"))
                     has_style = is_first_choice = 1;
                  else if(strcasestr(rest, "oblique")) {
                     has_style = 1;
                     is_first_choice = 0;
                  } else
                     has_style = 0;
                  break;
                  
               case OBLIQUE:
                  if(strcasestr(rest, "oblique"))
                     has_style = is_first_choice = 1;
                  else if(strcasestr(rest, "italic")) {
                     has_style = 1;
                     is_first_choice = 0;
                  } else
                     has_style = 0;
                  break;
                  
               default:
                  // TODO error
                  has_style = 0;
               }
            }
            
            if(!has_style)
               continue;
            
            int len = strlen(de->d_name);
            if(is_first_choice) {
               if(first_choice == NULL || len < first_choice_len) {
                  if(first_choice)
                     free(first_choice);
                  first_choice = strdup(de->d_name);
                  first_choice_len = len;
               }
            } else {
               if(second_choice == NULL || len < second_choice_len) {
                  if(second_choice)
                     free(second_choice);
                  second_choice = strdup(de->d_name);
                  second_choice_len = len;
               }
            }            
         }
      } while(de);
      closedir(dir);
   }

   // TODO: The current distinction between first and second choise is not
   // perfect. If the italic variant of "Foo" is requested, and the font files
   // "Foo-oblique.tff" and "FooButActallyOnlyResemblingRemotely-italic.ttf",
   // the latter is choosen, but the former is more likely the desired font.
   // Again, the lenght should be regarded: since the latter name is *much*
   // longer (simply longer may not be sufficient), the former should be used
   // in the example, although it is only the second choice.
   if(first_choice || second_choice) {
      char file[FILENAME_MAX + 1];
      sprintf(file, "%s/%s",
              path, first_choice ? first_choice : second_choice);
      if(first_choice)
         free(first_choice);
      if(second_choice)
         free(second_choice);

      TTF_Font *font = TTF_OpenFont(file, size);
      if(font == NULL)
         nano_fail("Found, but cannot load font file '%s', size %d: %s",
                   file, size, SDL_GetError());
      return font;
   } else  {
      nano_fail("No font file found for path '%s', family '%s', weight %d, and"
                " style %d", path, family, weight, style);
      return NULL; // compiler happiness
   }
}

/*
 * Load font given by strings instead of (partly) numbers. Allowed values for
 * weight: "normal" (or short "n"), "bold" (or short "b"). Allowed values for
 * style: "normal" (or short "n"), "italic" or "italics" (or short "i"),
 * "oblique" (or short "o").
 */
TTF_Font *nano_load_font_str(const char *path, const char *family,
                             const char *weight, const char *style,
                             const char *size)
{
   int w = 0, s = 0;

   if(strcasecmp(weight, "n") == 0 || strcasecmp(weight, "normal") == 0)
      w = 0;
   else if(strcasecmp(weight, "b") == 0 || strcasecmp(weight, "bold") == 0)
      w = 1;
   else
      nano_fail("Invalid weight '%s'.", weight);

   if(strcasecmp(style, "n") == 0 || strcasecmp(style, "normal") == 0)
      s = 0;
   else if(strcasecmp(style, "i") == 0 || strcasecmp(style, "italic") ||
      strcasecmp(style, "italics") == 0)
      s = ITALICS;
   else if(strcasecmp(style, "o") == 0 || strcasecmp(style, "oblique") == 0)
      s = OBLIQUE;
   else
      nano_fail("Invalid style '%s'.", style);

   return nano_load_font(path, family, w, s, nano_parse_int(size));
}

/*
 * Parse color string of the form "RRGGBB" (currently without "#" prefix, and
 * with exactly 8 bits per channel!) and return it as SDL color.
 */
void nano_parse_color(const char *s, SDL_Color *c)
{
   // TODO corrent parsing of colors
   int n = strtol(s, NULL, 16);
   c->r = (n & 0xff0000) >> 16;
   c->g = (n & 0xff00) >> 8;
   c->b = n & 0xff;
}


/*
 * Scale a surface.
 *
 * The algorithm is rather simple. If the scaled surface is smaller
 * (both width and height) than the original surface, each pixel in
 * the scaled surface is assigned a rectangle of pixels in the
 * original surface; the resulting pixel value (red, green, blue) is
 * simply the average of all pixel values. This is pretty fast and
 * leads to rather good results.
 *
 * Nothing special (like interpolation) is done when scaling up. For
 * nanoglk, this is rarely needed.
 */
SDL_Surface *nano_scale_surface(SDL_Surface *surface,
                                Uint16 width, Uint16 height)
{
   SDL_Surface *scaled =
      SDL_CreateRGBSurface(surface->flags, width, height,
                           surface->format->BitsPerPixel,
                           surface->format->Rmask, surface->format->Gmask,
                           surface->format->Bmask, surface->format->Amask);

   // TODO actually nonsense in some cases
   int bpp = surface->format->BytesPerPixel;
   int *v = (int*)nano_malloc(bpp * sizeof(int));
   
   for(int x = 0; x < width; x++)
      for(int y = 0; y < height; y++) {
         int xo1 = x * surface->w / width;
         int xo2 = MAX((x + 1) * surface->w / width, xo1 + 1);
         int yo1 = y * surface->h / height;
         int yo2 = MAX((y + 1) * surface->h / height, yo1 + 1);
         int n = (xo2 - xo1) * (yo2 - yo1);
         
         for(int i = 0; i < bpp; i++)
            v[i] = 0;
         
         for(int xo = xo1; xo < xo2; xo++)
            for(int yo = yo1; yo < yo2; yo++) {
               Uint8 *ps =
                  (Uint8*)surface->pixels + yo * surface->pitch + xo * bpp;
               for(int i = 0; i < bpp; i++)
                  v[i] += ps[i];
            }
         
         Uint8 *pd = (Uint8*)scaled->pixels + y * scaled->pitch + x * bpp;
         for(int i = 0; i < bpp; i++)
            pd[i] = v[i] / n;
      }
   
   free(v);

   return scaled;
}

void nano_fill_rect(SDL_Surface *surface, SDL_Color c,
                    int x, int y, int w, int h)
{
   SDL_Rect r = { x, y, w, h };
   SDL_FillRect(surface, &r, SDL_MapRGB(surface->format, c.r, c.g, c.b));
}

static void draw_hline(SDL_Surface *surface, SDL_Color c, int x, int y, int w)
{
   nano_fill_rect(surface, c, x, y, w, 1);
}

static void draw_vline(SDL_Surface *surface, SDL_Color c, int x, int y, int h)
{
   nano_fill_rect(surface, c, x, y, 1, h);
}

/*
 * Calculates the "shaded" variant of a coor. If d < 0, the result is
 * darker, unless "c" is already quite dark; in this case, a slightly
 * lighter color is returned. If d > 0, the result is lighter, unless
 * "c" is alreadz quite light; in this case, a slightly darker color
 * is returned.
 *
 * This is mainly used for 3D borders.
 */
static void shade_color(SDL_Color c, SDL_Color *s, int d)
{
   double old_lightness = ((double)MAX3(c.r, c.g, c.b)) / 255;
   double new_lightness;

   if (old_lightness > 0.8) {
      if (d > 0)
         new_lightness = old_lightness - 0.2;
      else
         new_lightness = old_lightness - 0.4;
   } else if (old_lightness < 0.2) {
      if (d > 0)
         new_lightness = old_lightness + 0.4;
      else
         new_lightness = old_lightness + 0.2;
   } else
      new_lightness = old_lightness + d * 0.2;

   if (old_lightness) {
      double f = (new_lightness / old_lightness);
      s->r = (int)(c.r * f);
      s->g = (int)(c.g * f);
      s->b = (int)(c.b * f);
   } else
      s->r = s->g = s->b = (int)(new_lightness * 255);
}

void nano_fill_3d_inset(SDL_Surface *surface, SDL_Color bg,
                        int x, int y, int w, int h)
{
   SDL_Color li, sh;
   shade_color(bg, &li, +1);
   shade_color(bg, &sh, -1);

   draw_hline(surface, sh, x, y, w);
   draw_vline(surface, sh, x, y + 1, h - 1);
   draw_hline(surface, li, x + 1, y + h - 1, w - 1);
   draw_vline(surface, li, x + w - 1, y + 1, h - 2);
   nano_fill_rect(surface, bg, x + 1, y + 1, w -2, h - 2);
}

void nano_fill_3d_outset(SDL_Surface *surface, SDL_Color bg,
                         int x, int y, int w, int h)
{
   SDL_Color li, sh;
   shade_color(bg, &li, +1);
   shade_color(bg, &sh, -1);

   draw_hline(surface, li, x, y, w - 1);
   draw_vline(surface, li, x, y + 1, h - 2);
   draw_hline(surface, sh, x, y + h - 1, w);
   draw_vline(surface, sh, x + w - 1, y, h - 1);

   nano_fill_rect(surface, bg, x + 1, y + 1, w -2, h - 2);
}

/*
 * Show a message, defined as a list of lines, "msg", which is NULL-terminated.
 * Furthermore a row of buttons, defined by NULL-terminated "btn". The button
 * with index "def_btn" (0-based) gets an additional border marking it as
 * default button (Motif style). "dfg" and "dbg" are used as foreground and
 * background color.
 *
 * Return immediately after drawing. This function is only a helper function for
 * nano_show_message() and nano_ask_yes_no().
 */
static void message(SDL_Surface *surface, Uint16 **msg, Uint16 **btn,
                    int def_btn, SDL_Color dfg, SDL_Color dbg, TTF_Font *font)
{
   int num_msg = 0;
   while(msg[num_msg])
      num_msg++;

   int num_btn = 0;
   while(btn[num_btn])
      num_btn++;

   int wt = 0, ht = 0;
   SDL_Surface **t = (SDL_Surface**)nano_malloc(num_msg * sizeof(SDL_Surface*));
   for(int i = 0; i < num_msg; i++) {
printf("ui.c message %s\n", msg);
     t[i] = TTF_RenderUNICODE_Shaded(font, msg[i], dfg, dbg);
      wt = MAX(wt, t[i]->w);
      ht += t[i]->h;
   }
   
   int d1 = t[0]->h, d2 = d1 / 2, d4 = d1 / 4;

   int wb = 0;
   SDL_Surface **b = (SDL_Surface**)nano_malloc(num_msg * sizeof(SDL_Surface*));
   for(int i = 0; i < num_btn; i++) {
      b[i] = TTF_RenderUNICODE_Shaded(font, btn[i], dfg, dbg);
      wb = MAX(wb, b[i]->w);
   }
   wb += (8 + 2 * d2);
   int hb = b[0]->h + 8 + 2 * d4;

   int w = 2 + MAX(wt + 2 * d1, num_btn * wb + (num_btn + 1) * d2);
   int h = 4 + 2 * d1 + ht + 2 * d2 + hb;
   int x = (surface->w - w) / 2, y = (surface->h - h) / 2;
   nano_save_window(surface, x, y, w, h);
   nano_fill_3d_outset(surface, dbg, x, y, w, h);

   int yt = y + 1 + d1;
   for(int i = 0; i < num_msg; i++) {
      SDL_Rect r1 = { 0, 0, t[i]->w, t[i]->h };
      SDL_Rect r2 = { (surface->w - t[i]->w) / 2, yt, t[i]->w, t[i]->h };
      SDL_BlitSurface(t[i], &r1, surface, &r2);
      yt += t[i]->h;
      SDL_FreeSurface(t[i]);
   }

   yt += d1;
   nano_fill_3d_inset(surface, dbg, x + d4, yt, w - 2 * d4, 2);
   yt += (2 + d2);

   for(int i = 0; i < num_btn; i++) {
      int xb =
         (surface->w - (num_btn * wb + (num_btn - 1) * d2)) / 2 + i * (wb + d2);
      if(i == def_btn)
         nano_fill_3d_inset(surface, dbg, xb, yt, wb, hb);
      nano_fill_3d_outset(surface, dbg, xb + 3, yt + 3, wb - 6, hb - 6);
      
      SDL_Rect r1 = { 0, 0, b[i]->w, b[i]->h };
      SDL_Rect r2 = { xb + (wb - b[i]->w) / 2, yt + 4 + d4, b[i]->w, b[i]->h };
      SDL_BlitSurface(b[i], &r1, surface, &r2);
      SDL_FreeSurface(b[i]);
   }
  
   free(t);
   SDL_RenderPresent(surface);
}

/*
 * Show a message, defined as a list of lines, "msg", which is NULL-terminated.
 * Dismiss the message, as soon as the user has pressed SPACE, RETURN, or
 * ESCAPE. "dfg" and "dbg" are used as foreground and background color.
 */
void nano_show_message(SDL_Surface *surface, Uint16 **msg,
                       SDL_Color dfg, SDL_Color dbg, TTF_Font *font)
{
   Uint16 dismiss[] = { 'D', 'i', 's', 'm', 'i', 's', 's', 0 };
   Uint16 *btn[] = { dismiss, NULL };

printf("ui.c nano_show_message %s\n", msg);

   message(surface, msg, btn, 0, dfg, dbg, font);
   
   while(1) {
      SDL_Event event;
      nano_wait_event(&event);
      if(event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_SPACE ||
                                       event.key.keysym.sym == SDLK_RETURN ||
                                       event.key.keysym.sym == SDLK_ESCAPE)) {
         nano_restore_window(surface);
         return;
      }
   }
}

/*
 * Show a message, defined as a list of lines, "msg", which is NULL-terminated.
 * Let the user choose between "yes" ("y" key) or "no" ("n" key or ESCAPE). 
 * SPACE and RETURN will activate the default answer, which is given by
 * "default_answer" (1 = yes, 0 = no). "dfg" and "dbg" are used as foreground
 * and background color.
 */
int nano_ask_yes_no(SDL_Surface *surface, Uint16 **msg, int default_answer,
                    SDL_Color dfg, SDL_Color dbg, TTF_Font *font)
{
   Uint16 yes[] = { 'Y', 'e', 's', 0 };
   Uint16 no[] = { 'N', 'o', 0 };
   Uint16 *btn[] = { yes, no, NULL };

   message(surface, msg, btn, default_answer ? 0 : 1, dfg, dbg, font);
   
   while(1) {
      SDL_Event event;
      nano_wait_event(&event);
      if(event.type == SDL_KEYDOWN) {
         switch(event.key.keysym.sym) {
         case 'y':
            nano_restore_window(surface);
            return 1;

         case 'n': case SDLK_ESCAPE:
            nano_restore_window(surface);
            return 0;

         case SDLK_SPACE: case SDLK_RETURN:
            nano_restore_window(surface);
            return default_answer;

         default:
            break;
         }
      }
   }
}

/*
 * Lets the user input text. This function returns as soon as the user has input
 * something which is not processed here; this way, other keys (like UP and DOWN
 * for history) can be easily implemented. It should look like this:
 *
 *    int state = -1;
 *    while(...) {
 *       SDL_Event event;
 *       nano_input_text16(..., &event, ..., &state);
 *       switch(event.type) {
 *          ...
 *
 * This way, the state of the input (scroll offset and cursor position) is
 * preserved.
 *
 * Arguments:
 * - surface     the SDL surface the input field should be shown in
 * - event       the event, which is not processed by this function, is returned
 *               by this pointer
 * - text        the buffer of the text, 0-terminated; may be set before
 * - max_len     the maximum number of characters the user is allowed to enter,
 *               0-terminator excluded; so, max_len + 1 characters should be
 *               allocated before
 * - max_char    the maximal character allowed to enter; this can limit input
 *               to ASCII (127) or ISO-8859-1 (255) (a requirement defined by
 *               Glk)
 * - x, y, w, h  the area in which the input field is shown
 * - font        the font used for the text
 * - fg, bg      foreground, background
 * - state       if not NULL, the state (scroll offset and cursor position) is
 *               stored here; should be initially set to -1; exact format is
 *               not part of the interface, and may change!
 */
void nano_input_text16(SDL_Surface *surface, SDL_Event *event,
                       Uint16 *text, int max_len, int max_char,
                       int x, int y, int w, int h, TTF_Font *font,
                       SDL_Color fg, SDL_Color bg, int *state)
{
   nano_trace("input_text16(..., %p, %d, %d, %d, %d, %d, %d, ...)",
              text, max_len, max_char, x, y, w, h);

   int pos = strlen16(text);
   Uint16 *text_part = (Uint16*)nano_malloc((max_len + 1) * sizeof(Uint16*));
   int ox = 0;

   if(state && *state != -1) {
      pos = *state & 0x7fff;
      ox = *state >> 15;
   }

printf("ui.c nano_input_text16 starting while-loop\n");
   while(1) {
      int cx, c;

      if(pos == 0)
         cx = 0;
      else {
         memcpy(text_part, text, pos * sizeof(Uint16));
         text_part[pos] = 0;
         SDL_Surface *ts_part =
            TTF_RenderUNICODE_Shaded(font, text_part, fg, bg);
         cx = ts_part->w;
         SDL_FreeSurface(ts_part);
      }
      
      if(cx > ox + w - 1)
         ox = cx - w + 1;
      else if(cx < ox)
         ox = cx;

      SDL_Rect rs = { x, y, w, h };
#ifdef SDL12P
      SDL_FillRect(surface, &rs, SDL_MapRGB(surface->format, bg.r, bg.g, bg.b));
#endif

      SDL_Surface *ts_total = TTF_RenderUNICODE_Shaded(font, text, fg, bg);
      SDL_Rect r1 = { ox, 0, w, h };
      SDL_Rect r2 = { x, y, w, h };
#ifdef SDL12P
      SDL_BlitSurface(ts_total, &r1, surface, &r2);
#endif
      SDL_FreeSurface(ts_total);

      SDL_Rect rc = { x + cx - ox, y, 1, h };
#ifdef SDL12P
      SDL_FillRect(surface, &rc, SDL_MapRGB(surface->format, fg.r, fg.g, fg.b));

      SDL_RenderPresent(surface);
#endif

      nano_wait_event(event);
      int len = strlen16(text);

// SDL1.2 --> SDL2 "You no longer get character input from SDL_KEYDOWN events. Use SDL_KEYDOWN to treat the keyboard like a 101-button joystick now. Text input comes from somewhere else."
// SDL1.2 --> SDL2 "The new event is SDL_TEXTINPUT."

      switch(event->type) {
	  case SDL_TEXTINPUT:
          printf("ui.c SDL_TEXTINPUT\n");
		  break;
      case SDL_KEYDOWN:
         switch(event->key.keysym.sym) {
	     case SDLK_UP:
	     case SDLK_DOWN:
	        // ToDo: history feature
	        printf("SDLK_UP / SDLK_DOWN ToDo: implement history feature.\n");
	        break;
         case SDLK_LEFT:
            if(pos > 0)
               pos--;
            break;
            
         case SDLK_RIGHT:
            if(text[pos])
               pos++;
            break;

         case SDLK_BACKSPACE:
            if(pos > 0) {
               memmove(text + pos - 1, text + pos,
                       sizeof(Uint16) * (len - pos + 1));
               pos--;
            }
            break;

         case SDLK_DELETE:
            if(text[pos])
               memmove(text + pos, text + pos + 1,
                       sizeof(Uint16) * (len - pos));
            break;
            
         case SDLK_HOME:
            pos = 0;
            break;

         case SDLK_END:
            pos = strlen16(text);
            break;
            
         default:
#ifdef SDL12P
            c = event->key.keysym.unicode;
#endif
			c = event->key.keysym.scancode;
            printf("ui.c normal key\n");
// this logic is no longer valid.            
            if((c >= 32 && c <= 126) || (c >= 160 && c <= max_char)) {
               if(len < max_len) {
                  memmove(text + pos + 1, text + pos,
                          sizeof(Uint16) * (len - pos + 1));
                  text[pos] = c;
                  pos++;
               }
            }
            else {
               if(state)
                  *state = pos | (ox << 15);

               SDL_Rect rs = { x, y, w, h};
#ifdef SDL12P
               SDL_FillRect(surface, &rs, SDL_MapRGB(surface->format,
                                                     bg.r, bg.g, bg.b));
#endif
               ts_total = TTF_RenderUNICODE_Shaded(font, text, fg, bg);
               SDL_Rect r1 = { ox, 0, w, h };
               SDL_Rect r2 = { x, y, w, h };
#ifdef SDL12P
               SDL_BlitSurface(ts_total, &r1, surface, &r2);
#endif
               SDL_FreeSurface(ts_total);
#ifdef SDL12P
               SDL_RenderPresent(surface);
#endif
               return;
            }
            break;
         }
         break;
         
         default:
            if(state)
               *state = pos | (ox << 15);

            SDL_Rect rs = { x, y, w, h};
#ifdef SDL12P
            SDL_FillRect(surface, &rs, SDL_MapRGB(surface->format,
                                                  bg.r, bg.g, bg.b));
#endif
            ts_total = TTF_RenderUNICODE_Shaded(font, text, fg, bg);
            SDL_Rect r1 = { ox, 0, w, h };
            SDL_Rect r2 = { x, y, w, h };
#ifdef SDL12P
            SDL_BlitSurface(ts_total, &r1, surface, &r2);
#endif
            SDL_FreeSurface(ts_total);
#ifdef SDL12P
            SDL_RenderPresent(surface);
#endif
            return;
      }
   }
}

