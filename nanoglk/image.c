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
 * Handling images.
 *
 * Images may be scaled automatically, since nanoglk is typically used
 * on small screens. The reference is the screen size, not the window
 * size, since glk_image_draw() and glk_image_get_info() should behave
 * in the same way, but glk_image_get_info() does not know of any
 * window.
 *
 * TODO: Caching images would probably improve performance.
 */

#include "nanoglk.h"
#include "SDL2/SDL_image.h"

static SDL_Surface *load_image(glui32 image);
static int get_scaled_image_size(glui32 *w, glui32 *h);
static glui32 draw_image(winid_t win, glui32 image, glui32 w, glui32 h,
                         glsi32 val1, glsi32 val2);

glui32 glk_image_draw(winid_t win, glui32 image, glsi32 val1, glsi32 val2)
{
   glui32 ret = draw_image(win, image, -1, -1, val1, val2);
   nanoglk_log("glk_image_draw(%p, %d, %d, %d) => %d",
               win, image, val1, val2, ret);
   return ret;
}

glui32 glk_image_draw_scaled(winid_t win, glui32 image,
                             glsi32 val1, glsi32 val2,
                             glui32 width, glui32 height)
{
   glui32 ret = draw_image(win, image, width, height, val1, val2);
   nanoglk_log("glk_image_draw_scaled(%p, %d, %d, %d) => %d",
               win, image, val1, val2, ret);
   return ret;
}

glui32 glk_image_get_info(glui32 image, glui32 *width, glui32 *height)
{
   SDL_Surface *img = load_image(image);
   if(img) {
      *width = img->w;
      *height = img->h;
      if(get_scaled_image_size(width, height))
         nano_trace("glk_image_info: scaled down form %d x %d to %d x %d",
                    img->w, img->h, *width, *height);
      SDL_FreeSurface(img);
         
      nanoglk_log("glk_image_get_info(%d, ..., ...) => %d x %d",
                  image, *width, *height);
      return 1;
   } else {
      nanoglk_log("glk_image_get_info(%d, ..., ...) => no result", image);
      return 0;
   }
}

/*
 * Load an image from the blorb and return a SDL surface. Return NULL if
 * something fails.
 */
SDL_Surface *load_image(glui32 image)
{
   // TODO: Simle, but creating a temporary file is ugly.
   char *file = tmpnam(NULL);
   giblorb_result_t res;
   giblorb_err_t err;

   if((err = giblorb_load_resource(giblorb_get_resource_map(),
                                   giblorb_method_Memory, &res, giblorb_ID_Pict,
                                   image)) == giblorb_err_None) {
      FILE *f = fopen(file, "wb");
      fwrite(res.data.ptr, 1, res.length, f);
      fclose(f);

      SDL_Surface *img = IMG_Load(file);
      if(!img)
         nano_warn("IMG_Load failed: %s\n", IMG_GetError());
      return img;
   } else {
      nano_warn("giblorb_load_resource(..., giblorb_method_Memory, ..., "
                "giblorb_ID_Pict, %d) returned %d", image, err);
      return NULL;
   }
}

/*
 * Determine whether an image must be scaled to fit on the screen (see
 * comment at the beginning of this file). If it must be scaled, the
 * values of *w and *h (which are initially the values of the image
 * size) are set to the scaled values, and 1 is returned. If it does
 * not have to be scaled, 0 is returned. This way, unneccessary calls
 * of nano_scale_surface() (or similar) can be avoided.
 */
int get_scaled_image_size(glui32 *w, glui32 *h)
{
   int scale = 0;
   if(*w > nanoglk_mainwindow->nanoglk_surface->w) {
      *h = *h * nanoglk_mainwindow->nanoglk_surface->w / *w;
      *w = nanoglk_mainwindow->nanoglk_surface->w;
      scale = 1;
   }
   if(*h > nanoglk_mainwindow->nanoglk_surface->h) {
      *w = *w * nanoglk_mainwindow->nanoglk_surface->h / *h;
      *h = nanoglk_mainwindow->nanoglk_surface->h;
      scale = 1;
   }
   return scale;
}

/*
 * Draw an image from the blorb into a given window.
 *
 * - "w" and "h" represent the desired size (but this may be scaled
 *   down). If set to -1, the original image size is used.
 * - "val1" and "val2" are the values passed to glk_image_draw() and
 *    glk_image_draw_scaled().
 *
 */
glui32 draw_image(winid_t win, glui32 image, glui32 w, glui32 h,
                  glsi32 val1, glsi32 val2)
{
   SDL_Surface *img = load_image(image);
   if(img) {
      if(w == -1)
         w = img->w;
      if(h == -1)
         h = img->h;

      SDL_Surface *drawn_img;
      if(w != img->w || h != img->h || get_scaled_image_size(&w, &h)) {
         nano_trace("glk_image_draw: scaled down from %d x %d to %d x %d",
                    img->w, img->h, w, h);
         drawn_img = nano_scale_surface(img, w, h);
         SDL_FreeSurface(img);
      } else
         drawn_img = img;

      glui32 ret;
      switch(win->wintype) {
      case wintype_TextBuffer:
         nanoglk_wintextbuffer_put_image(win, drawn_img, val1, val2);
         ret = 1;
         break;
         
      case wintype_Graphics:
         nanoglk_wingraphics_put_image(win, drawn_img, val1, val2);
         ret = 1;
         break;

      default:
         nano_warn("glk_image_draw not supported for wintype %d", win->wintype);
         ret = 0;
         break;
      }

      SDL_FreeSurface(drawn_img);
      return ret;
   } else
      return 0;
}
