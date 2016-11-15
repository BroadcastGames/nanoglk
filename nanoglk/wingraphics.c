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
 * Handling graphic windows. Most functions are called from the
 * general window functions defined in "window.c".
 */

#include "nanoglk.h"

/*
 * The structure used for win->data.
 */
struct graphics
{
   SDL_Color bg;  // background of the window
};

/*
 * Initialize a graphics window.
 */
void nanoglk_wingraphics_init(winid_t win)
{
   win->data = nano_malloc(sizeof(struct graphics));
   struct graphics *g = (struct graphics*)win->data;
   g->bg = nanoglk_buffer_font[style_Normal]->bg; // To have one background.
   nanoglk_wingraphics_clear(win);
}

/*
 * Destroy the specific part of a graphics window.
 */
void nanoglk_wingraphics_free(winid_t win)
{
   free(win->data);
}

/*
 * Clear a graphics window.
 */
void nanoglk_wingraphics_clear(winid_t win)
{
   struct graphics *g = (struct graphics*)win->data;
   SDL_FillRect(nanoglk_surface, &win->area,
                SDL_MapRGB(nanoglk_surface->format, g->bg.r, g->bg.g, g->bg.b));
}

/*
 * Resize a graphics window.
 */
void nanoglk_wingraphics_resize(winid_t win, SDL_Rect *area)
{
   win->area = *area;
   // Nothing more. Perhaps it should be redrawn.
}

/*
 * Flush a graphics window, i. e. display all pending output.
 */
void nanoglk_wingraphics_flush(winid_t win)
{
   // Nothing to do.
}

/*
 * Erase a rectangle in a graphics window.
 */
void nanoglk_wingraphics_erase_rect(winid_t win,
                                    glsi32 left, glsi32 top,
                                    glui32 width, glui32 height)
{
   struct graphics *g = (struct graphics*)win->data;
   nano_fill_rect(nanoglk_surface, g->bg,
                  win->area.x + left, win->area.y + top, width, height);
}

/*
 * Fill a rectangle in a graphics window.
 */
void nanoglk_wingraphics_fill_rect(winid_t win, glui32 color,
                                   glsi32 left, glsi32 top,
                                   glui32 width, glui32 height)
{
   SDL_Color c = { (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff };
   nano_fill_rect(nanoglk_surface, c,
                  win->area.x + left, win->area.y + top, width, height);
}

/*
 * Sets the background of a graphics window.
 */
void nanoglk_wingraphics_set_background_color(winid_t win, glui32 color)
{
   struct graphics *g = (struct graphics*)win->data;
   g->bg.r = (color >> 16) & 0xff;
   g->bg.g = (color >> 8) & 0xff;
   g->bg.b = color & 0xff;
}

/*
 * Put an image into a graphics window. (The complicated stuff is done
 * in "image.c".
 */
void nanoglk_wingraphics_put_image(winid_t win, SDL_Surface *image,
                                   glsi32 val1, glsi32 val2)
{
   SDL_Rect r1 = { 0, 0,
                   MIN(image->w, win->area.w - val1),
                   MIN(image->h, win->area.h - val2) };
   SDL_Rect r2 = { win->area.x + val1, win->area.y + val2, r1.w, r1.h };
   SDL_BlitSurface(image, &r1, nanoglk_surface, &r2);
}
