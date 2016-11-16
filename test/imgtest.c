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
 * Test image scaling. Needs "Jabberwocky_creatures.jpg" from
 * Wikipedia (or any other image).
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "misc/misc.h"

int main(int argc, char **argv)
{
   SDL_Init(SDL_INIT_VIDEO);
   atexit( SDL_Quit);

   SDL_Surface *i0 = IMG_Load("Jabberwocky_creatures.jpg");
   SDL_Surface *i1[4];
   i1[0] = nano_scale_surface(i0, 160, 160 * i0->h / i0->w);
   i1[1] = nano_scale_surface(i0, 5 * i0->w, 5 * i0->h);
   i1[2] = nano_scale_surface(i0, 5 * i0->w, 120);
   i1[3] = nano_scale_surface(i0, 160, 5 * i0->h);
   SDL_FreeSurface(i0);

#ifdef SDL12A
   SDL_Surface *s = SDL_SetVideoMode( 320, 240, 24, SDL_DOUBLEBUF);
   
   for(int i = 0; i < 4; i++) {
      SDL_Rect r1 = { 0, 0, MIN(160, i1[i]->w), MIN(120, i1[i]->h) };
      SDL_Rect r2 = { 160 * (i % 2), 120 * (i / 2), r1.w, r1.h };
      SDL_BlitSurface(i1[i], &r1, s, &r2);
      SDL_FreeSurface(i1[i]);
   }

   SDL_RenderPresent(s);
#endif

   SDL_Event e;
   do
      nano_wait_event(&e);
   while(e.type != SDL_KEYDOWN);
   
   return 0;
}
