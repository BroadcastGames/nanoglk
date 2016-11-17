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
 * Handling windows, generally. For specific window types, there are
 * additional files.
 *
 * About styles: There are three different layers where style colors
 * are stored:
 *
 * - nanoglk_buffer_font[style]->fg and nanoglk_buffer_font[style]->bg
 *   store the colors which are configured by the user. After they
 *   habe been set when nanoglk is initialized, they are not changed
 *   anymore.
 *
 * - next_buffer_fg, next_buffer_bg and next_buffer_rev contain the
 *   colors and the reverse flag (see be glk_stylehint_set() and
 *   set_hint() how this is handled) for the *next* text buffer
 *   window, next_grid_* contain the respective values for the *next*
 *   text grid window. They are set initially to the values from
 *   nanoglk_buffer_font, and modified by glk_stylehint_set() and
 *   glk_stylehint_clear().
 *
 * - Each window itself contains colors for each style. They are
 *   copied from next_buffer_fg etc. when the window is created (see
 *   glk_window_open()), and after this, then never modified anymore
 *   (as the Glk specification defines). These are the values used for
 *   actual rendering (see "wintextbuffer.c" and "wintextgrid.c").
 *
 * (Fonts are always taken from directly nanoglk_buffer_font, at least
 * as long Glk provides no hints for fonts.)
 */

#include "nanoglk.h"

SDL_Surface *nanoglk_surface; // The SDL surface representing the screen.
// ToDo: this should be a struct, but right now we have a single window.
SDL_Window *nanoglk_output_window;
SDL_Renderer *nanoglk_output_renderer;
SDL_Texture *nanoglk_output_texture;
static winid_t root = NULL;   // Obviously, the root window.

// Thickness of borders between windows. (Simple solid borders.)
#define BORDER_WIDTH 1

static void window_calc_sizes(winid_t pair,
                              SDL_Rect *left_area, SDL_Rect *right_area);
static int window_size_base_width(winid_t win);
static int window_size_base_height(winid_t win);
static void window_rearrange(winid_t pair);
static void window_draw_border(winid_t pair);
static void window_resize(winid_t win, SDL_Rect *area);
static void flush(winid_t win);
static glui32 get_line16(winid_t win, Uint16 *text, int max_len, int max_char);

// See comment at the beginning of this file for more informations.
static SDL_Color next_buffer_fg[style_NUMSTYLES];
static SDL_Color next_buffer_bg[style_NUMSTYLES];
static char next_buffer_rev[style_NUMSTYLES];
static SDL_Color next_grid_fg[style_NUMSTYLES];
static SDL_Color next_grid_bg[style_NUMSTYLES];
static char next_grid_rev[style_NUMSTYLES];

/*
 * Print informations on a single window to log. There should be
 * "indent" spaces at the beginning of each lines (to make trees look
 * nicer). Used by print_windows().
 */
static void print_window(winid_t win, int indent)
{
   char *type;
   switch(win->wintype) {
   case wintype_Pair: type = "pair"; break;
   case wintype_Blank: type = "blank"; break;
   case wintype_TextBuffer: type = "text buffer"; break;
   case wintype_TextGrid: type = "text grid"; break;
   case wintype_Graphics: type = "graphics"; break;
   default: type = "unknown"; break;
   }

   if(win->parent == NULL)
      nano_info("%*sroot: %s (%d) window %p, at (%d, %d, %d x %d)",
                indent, "", type, win->wintype, win,
                win->area.x, win->area.y, win->area.w, win->area.h);
   else if(win == win->parent->left)
      nano_info("%*sleft: %s (%d) window %p, at (%d, %d, %d x %d)",
                indent, "", type, win->wintype, win,
                win->area.x, win->area.y, win->area.w, win->area.h);
   else if(win == win->parent->right) {
      char *unit, *dir;
      switch(win->method & winmethod_DivisionMask) {
      case winmethod_Fixed: unit = "px"; break;
      case winmethod_Proportional: unit = "%"; break;
      default: unit = " (unknwown unit)"; break;
      }

      switch(win->method & winmethod_DirMask) {
      case winmethod_Left: dir = "left"; break;
      case winmethod_Right: dir = "right"; break;
      case winmethod_Above: dir = "above"; break;
      case winmethod_Below: dir = "below"; break;
      default: dir = "unknown";
      }

      nano_info("%*sright: %s (%d) window %p, at (%d, %d, %d x %d)",
                indent, "", type, win->wintype, win,
                win->area.x, win->area.y, win->area.w, win->area.h);
      nano_info("%*s       method = %d => dir %s, %d%s",
                indent, "", win->method, dir, win->size, unit);
   } else
      nano_info("%*sneither left nor right?: %p", indent, "", win);

   if(win->left)
      print_window(win->left, indent + 1);

   if(win->right)
      print_window(win->right, indent + 1);
}

/*
 * Print informations on all windows to log. This function is called
 * when the user presses Ctrl+Alt+W. Used for debuggung.
 */
static void print_windows(void)
{
   if(root)
      print_window(root, 0);
   else
      nano_info("no root window");
}

/*
 * Quick and dirty control flags during development.
 * ToDo: make params in config file
 * */
static int windowFlagSetResize = FALSE;

/*
 * Initialize everything related to windows. Called in main().
 */
void nanoglk_window_init(int width, int height, int depth)
{
   /* set the title bar */
   //  ToDo: set to name of story or interpreter?
#ifdef SDL12A
   SDL_WM_SetCaption("nano Glk library", "nano Glk library Hello!");
#endif
   
#ifdef SDL12A
   if (windowFlagSetResize) {
      nanoglk_surface = SDL_SetVideoMode(width, height, depth, SDL_RESIZABLE | SDL_DOUBLEBUF);
      // ToDo: implement resize logic to respond to user mouse events
   } else {
      nanoglk_surface = SDL_SetVideoMode(width, height, depth, SDL_DOUBLEBUF);
   }

   nano_reg_surface(&nanoglk_surface);
#endif

   printf("window.c SDL_SetVideoMode SDL_CreateWindow %d x %d\n", width, height);
   nanoglk_output_window = SDL_CreateWindow("Window caption", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);
   printf("window.c SDL_SetVideoMode SDL_CreateWindow after\n");
   if(nanoglk_output_window == NULL) {
      /* Handle problem */
      fprintf(stderr, "%s\n", SDL_GetError());
      SDL_Quit();
   }
   nanoglk_output_renderer = SDL_CreateRenderer(nanoglk_output_window, -1, 0);
   if(nanoglk_output_renderer == NULL) {
      /* Handle problem */
      fprintf(stderr, "%s\n", SDL_GetError());
      SDL_Quit();
   }
   nanoglk_output_texture = SDL_CreateTexture(nanoglk_output_renderer,
      SDL_PIXELFORMAT_ARGB8888,
      SDL_TEXTUREACCESS_STREAMING,
      width, height);
   if(nanoglk_output_texture == NULL) {
      /* Handle problem */
      fprintf(stderr, "%s\n", SDL_GetError());
      SDL_Quit();
   }

   printf("window.c SDL_SetVideoMode SDL_CreateWindow after CHECKPOINT_A\n");
   // ToDo: equal to nano_reg_surface for saving window content
   // nano_reg_surface(&nanoglk_surface);
   printf("window.c SDL_SetVideoMode SDL_CreateWindow after CHECKPOINT_B\n");

   int i;
   for(i = 0; i < style_NUMSTYLES; i++) {
      next_buffer_fg[i] = nanoglk_buffer_font[i]->fg;
      next_buffer_bg[i] = nanoglk_buffer_font[i]->bg;
      next_buffer_rev[i] = 0;
      next_grid_fg[i] = nanoglk_grid_font[i]->fg;
      next_grid_bg[i] = nanoglk_grid_font[i]->bg;
      next_grid_rev[i] = 0;
   }

   printf("window.c SDL_SetVideoMode SDL_CreateWindow after CHECKPOINT_D\n");
#ifdef SDL12B
   nano_register_key('w', print_windows);
#endif
   printf("window.c SDL_SetVideoMode SDL_CreateWindow after CHECKPOINT_F\n");
}

winid_t glk_window_get_root(void)
{
   nanoglk_log("glk_window_get_root() => %p", root);
   return root;
}

winid_t glk_window_open(winid_t split, glui32 method, glui32 size,
                        glui32 wintype, glui32 rock)
{
printf("window.c glk_window_open CHECKPOINT_0_A START\n");
   winid_t win = (winid_t)nano_malloc(sizeof(struct glk_window_struct));
   nanoglk_log("glk_window_open(%p, %d, %d, %d, %d) => %p",
              split, method, size, wintype, rock, win);

printf("window.c glk_window_open CHECKPOINT_0_A\n");

   win->stream = nanoglk_stream_new(streamtype_Window, 0);
   win->stream->x.window = win;

   win->method = method;
   win->size = size;
   win->wintype = wintype;
   win->rock = rock;
   win->left = win->right = NULL;
   win->cur_styl = style_Normal;

printf("window.c glk_window_open CHECKPOINT_0_B\n");
   
   // Colors for styles. See comment at the beginning of this file.
   int i;
   switch(win->wintype) {
   case wintype_TextBuffer:
      for(i = 0; i < style_NUMSTYLES; i++) {
         win->fg[i] = next_buffer_fg[i];
         win->bg[i] = next_buffer_bg[i];
      }
      break;

   case wintype_TextGrid:
      for(i = 0; i < style_NUMSTYLES; i++) {
         win->fg[i] = next_grid_fg[i];
         win->bg[i] = next_grid_bg[i];
      }
      break;
   }

printf("window.c glk_window_open CHECKPOINT_0_C\n");

   winid_t pair;
   
   if(split == NULL) {
printf("window.c glk_window_open CHECKPOINT_0_D branch0\n");
      // parent is NULL => new root window
      nano_failunless(root == NULL, "two root windows");

printf("window.c glk_window_open CHECKPOINT_0_D branch0 SPOTA\n");
      win->parent = NULL;
printf("window.c glk_window_open CHECKPOINT_0_D branch0 SPOTA0\n");
      win->area.x = win->area.y = 0;
printf("window.c glk_window_open CHECKPOINT_0_D branch0 SPOTA1\n");
#ifdef SDL12A
      win->area.w = nanoglk_surface->w;
printf("window.c glk_window_open CHECKPOINT_0_D branch0 SPOTA2\n");
      win->area.h = nanoglk_surface->h;
#endif

	  SDL_QueryTexture(nanoglk_output_texture, NULL, NULL, &win->area.w, &win->area.h);

printf("window.c glk_window_open CHECKPOINT_0_D branch0 SPOTB\n");

      root = win;

      pair = NULL; // no pair window created

      nano_trace("[glk_window_open] root %p: (%d, %d, %d x %d)",
                 win, win->area.x, win->area.y, win->area.w, win->area.h);
   } else {
printf("window.c glk_window_open CHECKPOINT_0_D branch1\n");
      // Create a pair window. The old parent "split" becomes the left
      // child, the newly created becomes the right child. (See also
      // comment on these members in "nanoglk.h".)
      pair = (winid_t)nano_malloc(sizeof(struct glk_window_struct));
      pair->stream = NULL;
      pair->wintype = wintype_Pair;
      pair->rock = 0;
      pair->left = split;
      pair->right = win;
      pair->area = split->area;
      pair->method = split->method;
      pair->size = split->size;
      pair->parent = split->parent;

      // Rearrange tree: "pair" takes over the place of "split".
      if(pair->parent == NULL)
         root = pair;
      else {
         if(pair->parent->left == split)
            pair->parent->left = pair;
         else if(pair->parent->right == split)
            pair->parent->right = pair;
         else
            nano_fail("split not child of parent?");
      }

      split->parent = win->parent = pair;
      pair->left = split;
      pair->right = win;

      win->area = split->area;
      SDL_Rect split_area, win_area;
      window_calc_sizes(pair, &split_area, &win_area);
      window_resize(split, &split_area);
      win->area = win_area;

printf("window.c glk_window_open CHECKPOINT_0_D branch1 SPOTA\n");

      window_draw_border(pair);

printf("window.c glk_window_open CHECKPOINT_0_D branch1 SPOTB\n");

      nano_trace("split %p: (%d, %d, %d x %d)", split, split->area.x,
                split->area.y, split->area.w, split->area.h);
      nano_trace("new win %p: (%d, %d, %d x %d)", win, win->area.x,
                 win->area.y, win->area.w, win->area.h);
   }

printf("window.c glk_window_open CHECKPOINT_0_E\n");

   // Further initialization depending on the type.
   switch(win->wintype) {
   case wintype_TextBuffer:
printf("window.c glk_window_open CHECKPOINT_0_E PATH0\n");
      nanoglk_wintextbuffer_init(win);
printf("window.c glk_window_open CHECKPOINT_0_E PATH0A\n");
      break;

   case wintype_TextGrid:
printf("window.c glk_window_open CHECKPOINT_0_E PATH1\n");
      nanoglk_wintextgrid_init(win);
      break;

   case wintype_Graphics:
      nanoglk_wingraphics_init(win);
      break;
   }

printf("window.c glk_window_open CHECKPOINT_0_F\n");

   if(pair)
      pair->disprock = nanoglk_call_regi_obj(pair, gidisp_Class_Window);

printf("window.c glk_window_open CHECKPOINT_0_G\n");

   win->stream->disprock
      = nanoglk_call_regi_obj(win->stream, gidisp_Class_Stream);
   win->disprock = nanoglk_call_regi_obj(win, gidisp_Class_Window);
   
printf("window.c glk_window_open CHECKPOINT_0_H\n");
   
   return win;
}

/*
 * Calculate the size of a pair of windows (left and right), based on
 * the size of the pair window, and the attributes "method" and "size"
 * of the *right* window (see comments on struct glk_window_struct in
 * "nanogkl.h").
 *
 * Important note: the member "area" of the left and the right window
 * is not changed at all; instead, the calculated area is returned in
 * "left_area" and "right_area", which must be different from the
 * window areas. After the call of this function, the former can be
 * assigned to the latter.
 *
 * (The reason is that the caller may compare old and new size. Search
 * for calls of this function and see comments there.)
 */
static void window_calc_sizes(winid_t pair,
                              SDL_Rect *left_area, SDL_Rect *right_area)
{
   int size_px;   /* The actual calculated size in pixels, either width or
                     height, depending on the orientation, of the *right*
                     window. (Remeber? The right window is always the newer
                     one, and the one for which the size is explicitly
                     defined. */

   nano_trace("window_allocate(%p, ..., ...)", pair);

   // 1. Calculate the size of the right window, "size_px".
   switch(pair->right->method & winmethod_DivisionMask) {
   case winmethod_Fixed:
      switch(pair->right->method & winmethod_DirMask) {
      case winmethod_Above: case winmethod_Below:
         size_px =
            window_size_base_height(pair->right) * pair->right->size
            * nanoglk_factor_vertical_fixed;
         break;
            
      case winmethod_Left: case winmethod_Right:
         size_px =
            window_size_base_width(pair->right) * pair->right->size
            * nanoglk_factor_horizontal_fixed;
         break;
         
      default:
         nano_fail("none of winmethod_Above, winmethod_Below, "
                   "winmethod_Left or winmethod_Right set");
      }
      break;

   case winmethod_Proportional:
      switch(pair->right->method & winmethod_DirMask) {
      case winmethod_Above: case winmethod_Below:
         size_px =
            pair->right->size * pair->area.h
            * nanoglk_factor_vertical_proportional / 100;
         break;

      case winmethod_Left: case winmethod_Right:
         size_px =
            pair->right->size * pair->area.w
            * nanoglk_factor_horizontal_proportional / 100;
         break;

      default:
         nano_fail("none of winmethod_Above, winmethod_Below, "
                   "winmethod_Left or winmethod_Right set");
      }
      break;
      
   default:
      nano_fail("none of winmethod_Fixed or winmethod_Proportional set");
   }
   
   // Thickness of the border between left and right window. Regarding
   // "size_px" (the size of the *right* window), it belogs, so to speak,
   // to the left window.
   int border =
      ((pair->right->method & winmethod_BorderMask) == winmethod_Border) ?
      BORDER_WIDTH : 0;

   nano_trace("  size_px = %d", size_px);

   // 2. Position all windows, i. e. calculate the values of "left_area" and
   // "right_area".
   // 2.1 Parts depeding on the orientatin (vertical or horizontal)
   switch(pair->right->method & winmethod_DirMask) {
   case winmethod_Above: case winmethod_Below:
      right_area->h = size_px;
      left_area->h = pair->area.h - size_px - border;
      right_area->w = left_area->w = pair->area.w;
      right_area->x = left_area->x = pair->area.x;
      break;

   case winmethod_Left: case winmethod_Right:
      right_area->w = size_px;
      left_area->w = pair->area.w - size_px - border;
      right_area->h = left_area->h = pair->area.h;
      right_area->y = left_area->y = pair->area.y;
      break;
   }

   // 2.1 Parts depending furthermore on the exact direction
   switch(pair->right->method & winmethod_DirMask) {
   case winmethod_Above:
      left_area->y = pair->area.y + size_px + border;
      right_area->y = pair->area.y;
      break;

   case winmethod_Below:
      left_area->y = pair->area.y;
      right_area->y = pair->area.y + pair->area.h - size_px;
      break;

   case winmethod_Left:
      left_area->x = pair->area.x + size_px + border;
      right_area->x = pair->area.x;
      break;

   case winmethod_Right:
      left_area->x = pair->area.x;
      right_area->x = pair->area.x + pair->area.w - size_px;
      break;
   }

   nano_trace("  => { %d, %d, %d x %d }, { %d, %d, %d x %d }",
              right_area->x, right_area->y, right_area->w, right_area->h,
              left_area->x, left_area->y, left_area->w, left_area->h);
}

/*
 * The base for fixed window sizes in horizontal direction.
 */
static int window_size_base_width(winid_t win)
{
   // Simply take the "normal" font, which is identical for all windows of
   // one type (text buffer or text grid).
   switch(win->wintype) {
   case wintype_TextBuffer:
      // As opposed to fixed size fonts used for grid windows, here, the space
      // width is not very meaningful. (Perhaps another character should be
      // used.)
      return nanoglk_buffer_font[style_Normal]->space_width;

   case wintype_TextGrid:
      return nanoglk_grid_font[style_Normal]->space_width;

   default: // including wintypeGraphics
      return 1;
   }
}

/*
 * The base for fixed window sizes in vertical direction.
 */
static int window_size_base_height(winid_t win)
{
   // Same as in window_size_base_width().
   switch(win->wintype) {
   case wintype_TextBuffer:
      return nanoglk_buffer_font[style_Normal]->text_height;

   case wintype_TextGrid:
      return nanoglk_grid_font[style_Normal]->text_height;

   default: // including wintypeGraphics
      return 1;
   }
}

/*
 * Destroy a window, including its children.
 */
static void window_destroy(winid_t win)
{
   nanoglk_call_unregi_obj(win, gidisp_Class_Window, win->disprock);

   switch(win->wintype) {
   case wintype_TextBuffer:
      nanoglk_wintextbuffer_free(win);
      break;

   case wintype_TextGrid:
      nanoglk_wintextgrid_free(win);
      break;

   case wintype_Graphics:
      nanoglk_wingraphics_free(win);
      break;
   }

   if(win->left)
      window_destroy(win->left);
   if(win->right)
      window_destroy(win->right);
   free(win);
}

void glk_window_close(winid_t win, stream_result_t *result)
{
   // TODO: Call nanoglk_regi_arr for line buffer when pending. Makes it
   // necessary to store line buffer in window, not only in event queue.

   nano_info("glk_window_close(%p, ...)", win);

   if(win->parent == NULL)
      root = NULL;
   else {
      // Replace parent by sibling. (Reverse to glk_window_close().)
      winid_t sibling = glk_window_get_sibling(win);
      winid_t pair = win->parent;
      sibling->area = pair->area; // TODO Re-allocate children of sibling?
      sibling->parent = pair->parent;

      if(sibling->parent) {
         if(pair == sibling->parent->left)
            sibling->parent->left = sibling;
         else if(pair == sibling->parent->right)
            sibling->parent->right = sibling;
         else
            nano_fail("win neither left nor right child of parent?");
      }

      // TODO: unregister pair window?

      free(pair);
   }

   window_destroy(win);
   
   // TODO
   if(result)
      result->readcount = result->writecount = 0;

   if(result)
      nanoglk_log("glk_window_close(%p, ...) => (%d, %d)", 
                  win, result->readcount, result->writecount);
   else
      nanoglk_log("glk_window_close(%p, ...)", win);
}

void glk_window_get_size(winid_t win, glui32 *widthptr, glui32 *heightptr)
{
   if(widthptr) {
      int bw = window_size_base_width(win);
      *widthptr = (win->area.w + bw - 1) / bw; // rounded up
   }

   if(heightptr) {
      int bh = window_size_base_height(win);
      *heightptr = (win->area.h + bh - 1) / bh; // rounded up
   }

   if(widthptr && heightptr)
      nanoglk_log("glk_window_get_size(%p, ...) => (%d, %d)",
                  win, *widthptr, *heightptr);
   else if(widthptr)
      nanoglk_log("glk_window_get_size(%p, ...) => (%d, ...", win, *widthptr);
   else if(heightptr)
      nanoglk_log("glk_window_get_size(%p, ...) => (..., %d)", win, *heightptr);
   else
      nanoglk_log("glk_window_get_size(%p, ...) => (..., ...)", win);
}

void glk_window_set_arrangement(winid_t win, glui32 method, glui32 size,
                                winid_t keywin)
{
   nanoglk_log("glk_window_set_arrangement(%p, %d, %d, %p)",
               win, method, size, keywin);
  
   // Notice that method and size are always attached to the *right* window.
   if(keywin == NULL || keywin == win->right)
      ; // As expected; nothing to do.
   else if(keywin == win->left) {
      // "Keywin" is the left window: simply exchange left and right.
      winid_t t = win->left;
      win->left = win->right;
      win->right = t;
   }
   else
      nano_fail("keywin neither left nor right child of win");

   win->right->method = method;
   win->right->size = size;

   window_rearrange(win);
}

/*
 * Rearrange left and right child of a given pair, based on method and sized
 * already set in the (as usually) right window.
 */
void window_rearrange(winid_t pair)
{
   SDL_Rect left_area, right_area;
   window_calc_sizes(pair, &left_area, &right_area);

   // Let the window resize first, which size has been reduced, so that it can
   // copy from the old area before the now larger window will overwrite it.
   int or = pair->right->method & winmethod_DirMask;
   if(((or == winmethod_Above || or == winmethod_Below) &&
       left_area.h < pair->left->area.h) ||
      ((or == winmethod_Left || or == winmethod_Right) &&
       left_area.w < pair->left->area.w)) {
      window_resize(pair->left, &left_area);
      window_resize(pair->right, &right_area);
   } else {
      window_resize(pair->right, &right_area);
      window_resize(pair->left, &left_area);
   }

   window_draw_border(pair);
}

/*
 * Draw the border between left and right window of a given pair window.
 */
void window_draw_border(winid_t pair)
{
   printf("window.c window_draw_border\n");
   winid_t win = pair->right;
   if(win && (win->method & winmethod_BorderMask) == winmethod_Border) {
      printf("window.c window_draw_border BRANCH_0\n");
      // TODO: Which color? Should always be the same. Or, at least, it
      // should be predictable.
      SDL_Color c = nanoglk_buffer_font[style_Normal]->fg;

#ifdef SDL12F
      switch(win->method & winmethod_DirMask) {
      case winmethod_Above:
         nano_fill_rect(nanoglk_surface, c,
                        win->area.x, win->area.y + win->area.h,
                        win->area.w, BORDER_WIDTH);
         break;
                        
      case winmethod_Below:
         nano_fill_rect(nanoglk_surface, c,
                        win->area.x, win->area.y - BORDER_WIDTH,
                        win->area.w, BORDER_WIDTH);
         break;

      case winmethod_Left:
         nano_fill_rect(nanoglk_surface, c,
                        win->area.x + win->area.w, win->area.y,
                        BORDER_WIDTH, win->area.h);
         break;

      case winmethod_Right:
         nano_fill_rect(nanoglk_surface, c,
                        win->area.x - BORDER_WIDTH, win->area.y,
                        BORDER_WIDTH, win->area.h);
         break;
      }
#endif
   }
}

/*
 * Set the size of a window. The caller should take care that the sibling
 * rets the respective size. The window may also a pair window; in this case,
 * the sizes of the child windows are also recalculated.
 */
void window_resize(winid_t win, SDL_Rect *area)
{
   switch(win->wintype) {
   case wintype_TextBuffer:
      nanoglk_wintextbuffer_resize(win, area);
      break;

   case wintype_TextGrid:
      nanoglk_wintextgrid_resize(win, area);
      break;

   case wintype_Graphics:
      nanoglk_wingraphics_resize(win, area);
      break;

   case wintype_Pair:
      win->area = *area;
      window_rearrange(win);
      break;

   default:
      // TODO necessary?
      win->area = *area;
      break;
   }
}

void glk_window_get_arrangement(winid_t win, glui32 *methodptr, glui32 *sizeptr,
                                winid_t *keywinptr)
{
   nanoglk_log("glk_window_get_arrangement(%p, ..., ...,, ...) => ...", win);
   nano_fail("glk_window_get_arrangement not implemented");
}

glui32 glk_window_get_rock(winid_t win)
{
   nanoglk_log("glk_window_get_rock(%p) => %d", win, win->rock);
   return win->rock;
}

glui32 glk_window_get_type(winid_t win)
{
   nanoglk_log("glk_window_get_type(%p) => %d", win, win->wintype);
   return win->wintype;
}

winid_t glk_window_get_parent(winid_t win)
{
   nanoglk_log("glk_window_get_parent(%p) => %p", win, win->parent);
   return win->parent;
}

winid_t glk_window_get_sibling(winid_t win)
{
   winid_t sibling = NULL;

   if(win == NULL || win->parent == NULL)
      sibling = NULL;
   else if(win == win->parent->left)
      sibling = win->parent->right;
   else if(win == win->parent->right)
      sibling = win->parent->left;
   else
      nano_fail("win neither left nor right child of parent?");

   nanoglk_log("glk_window_get_sibling(%p) => %p", win, sibling);
   return sibling;
}

void glk_window_clear(winid_t win)
{
   nanoglk_log("glk_window_clear(%p)", win);

   switch(win->wintype) {
   case wintype_TextBuffer:
      nanoglk_wintextbuffer_clear(win);
      break;

   case wintype_TextGrid:
      nanoglk_wintextgrid_clear(win);
      break;

   case wintype_Graphics:
      nanoglk_wingraphics_clear(win);
      break;
   }
}

void glk_window_move_cursor(winid_t win, glui32 xpos, glui32 ypos)
{
   nanoglk_log("glk_window_move_cursor(%p, %d, %d)", win, xpos, ypos);

   switch(win->wintype) {
   case wintype_TextBuffer:
      //nanoglk_wintextbuffer_window_move_cursor(win, xpos, ypos);
      break;

   case wintype_TextGrid:
      nanoglk_wintextgrid_move_cursor(win, xpos, ypos);
      break;
   }
}

static winid_t outer_left_window(winid_t win)
{
   while(win->left)
      win = win->left;
   return win;
}

winid_t glk_window_iterate(winid_t win, glui32 *rockptr)
{
   // Windows are not stored in a list, since they are already stored in one
   // single tree. Instead, the following rules are used to determine a next
   // window (the order does not play a role), see below.
   //
   // TODO Proof that this really works.
   winid_t next;

   if(root == NULL)
      // No window at all: return nothing.
      next = NULL;
   else if(win == NULL)
      // 1. The first window is the outer left window (i. e. root->left->left
      //    ... until a leave is found)
      next = outer_left_window(root);
   else if(win->parent == NULL)
      // 2. The root window is the last window.
      next = NULL;
   else if(win == win->parent->left)
      // 3. The successor of a left window is the outer left window of the
      //    sibling.
      next = outer_left_window(win->parent->right);
   else if(win == win->parent->right)
      // 4. The successor of a right window is its parent.
      next = win->parent;
   else
      nano_fail("win neither left nor right child of parent?");
   
   if(next && rockptr)
      *rockptr = next->rock;
   
   nanoglk_log("glk_window_iterate(%p, ...) => %p", win, next);
   return next;
}

strid_t glk_window_get_stream(winid_t win)
{
   nanoglk_log("glk_window_get_stream(%p, ...) => %p", win, win->stream);
   return win->stream;
}

void glk_window_set_echo_stream(winid_t win, strid_t str)
{
   nanoglk_log("glk_set_echo_stream(%p, %p)", win, str);
   nano_fail("glk_window_set_echo_stream not implemented");
}

strid_t glk_window_get_echo_stream(winid_t win)
{
   nanoglk_log("glk_get_echo_stream(%p) => ...", win);
   nano_fail("glk_window_get_echo_stream not implemented");
   return NULL;
}

void glk_set_window(winid_t win)
{
   nanoglk_log("glk_set_window(%p)", win);
   nanoglk_stream_set_current(win ? win->stream : NULL);
}

/*
 * Put a single character (Unicode) into the window. Called by several
 * stream functions.
 */
void nanoglk_window_put_char(winid_t win, glui32 c)
{
   switch(win->wintype) {
   case wintype_TextBuffer:
      nanoglk_wintextbuffer_put_char(win, c);
      break;

   case wintype_TextGrid:
      nanoglk_wintextgrid_put_char(win, c);
      break;
   }
}

/*
 * Flush all windows, i. e. display any pending output on the
 * screen. Called by glk_select().
 */
void nanoglk_window_flush_all(void)
{
   nano_trace("nanoglk_window_flush_all()");

   if(root)
      flush(root);

#ifdef SDL12A
   SDL_Flip(nanoglk_surface);
#endif
   SDL_RenderPresent(nanoglk_surface);
}

/*
 * Flush a window and its children, i. e. display any pending output
 * on the screen.
 */
void flush(winid_t win)
{
   switch(win->wintype) {
   case wintype_TextBuffer:
      nanoglk_wintextbuffer_flush(win);
      break;

   case wintype_TextGrid:
      nanoglk_wintextgrid_flush(win);
      break;

   case wintype_Graphics:
      nanoglk_wingraphics_flush(win);
      break;
   }

   if(win->left)
      flush(win->left);
   if(win->right)
      flush(win->right);
}

/*
 * Return a key press event from a window, but limited to
 * Latin-1. Called when the respective event has been requested and is
 * read.
 */
glui32 nanoglk_window_get_char(winid_t win)
{
   glui32 c;

   do
      c = nanoglk_window_get_char_uni(win);
   while(!(c <= 255 || c >= 0x10000)); // TODO: Clarify: keycode_* are returned?
   
   return c;
}

/*
 * Return any (Unicode) press event from a window. Called when the
 * respective event has been requested and is read.
 */
glui32 nanoglk_window_get_char_uni(winid_t win)
{
   switch(win->wintype) {
   case wintype_TextBuffer:
      return nanoglk_wintextbuffer_get_char_uni(win);
      break;

   case wintype_TextGrid:
      return nanoglk_wintextgrid_get_char_uni(win);
      break;

   default:
      return 0; // TODO warning?
   }
}

#ifdef SDL12A
/*
 * Convert SDL symbols for special keys into the respective symbols defined
 * by Glk.
 */
glui32 nanoglk_window_char_sdl_to_glk(SDL_keysym *keysym)
{
   switch(keysym->sym) {
   case SDLK_LEFT: return keycode_Left;
   case SDLK_RIGHT: return keycode_Right;
   case SDLK_UP: return keycode_Up;
   case SDLK_DOWN: return keycode_Down;
   case SDLK_RETURN: return keycode_Return;
   case SDLK_DELETE: return keycode_Delete;
   case SDLK_ESCAPE: return keycode_Escape;
   case SDLK_TAB: return keycode_Tab;
   case SDLK_PAGEUP: return keycode_PageUp;
   case SDLK_PAGEDOWN: return keycode_PageDown;
   case SDLK_HOME: return keycode_Home;
   case SDLK_END: return keycode_End;
   case SDLK_F1: return keycode_Func1;
   case SDLK_F2: return keycode_Func2;
   case SDLK_F3: return keycode_Func3;
   case SDLK_F4: return keycode_Func4;
   case SDLK_F5: return keycode_Func5;
   case SDLK_F6: return keycode_Func6;
   case SDLK_F7: return keycode_Func7;
   case SDLK_F8: return keycode_Func8;
   case SDLK_F9: return keycode_Func9;
   case SDLK_F10: return keycode_Func10;
   case SDLK_F11: return keycode_Func11;
   case SDLK_F12: return keycode_Func12;
   default:
      return keysym->unicode;
   }
}
#endif

/*
 * Read a Latin-1 line from a window. Called when the respective event
 * has been requested and is read.
 *
 * - win      the window the user must input the text
 * - buf      the buffer to store the input; may already contain text; *not*
 *            0-terminated
 * - maxlen   the lenght of the buffer
 * - initlen  the lenght of the initial text
 */
glui32 nanoglk_window_get_line(winid_t win, char *buf, glui32 maxlen,
                               glui32 initlen)
{
   nano_trace("nanoglk_window_get_line(%p, %p, %d, %d)",
              win, buf, maxlen, initlen);

   // Convert char* to Uint16* ...
   Uint16 *text = (Uint16*)nano_malloc((maxlen + 1) * sizeof(Uint16*));
   int i;
   for(i = 0; i < initlen; i++)
      text[i] = buf[i];
   text[initlen] = 0;

   // ... read Uint16* (limited to Latin-1 characters) ...
   int len = get_line16(win, text, maxlen, 0xff);

   // ... and convert it back to char*.
   for(i = 0; text[i]; i++)
      buf[i] = text[i];
   
   return len;
}

/*
 * Read a Unicode line from a window. Called when the respective event
 * has been requested and is read.
 *
 * - win      the window the user must input the text
 * - buf      the buffer to store the input; may already contain text; *not*
 *            0-terminated
 * - maxlen   the lenght of the buffer
 * - initlen  the lenght of the initial text
 */
glui32 nanoglk_window_get_line_uni(winid_t win, glui32 *buf, glui32 maxlen,
                                   glui32 initlen)
{
   nano_trace("nanoglk_window_get_line_uni(%p, %p, %d, %d)",
              win, buf, maxlen, initlen);

   // Convert glui32* to Uint16* ...
   Uint16 *text = (Uint16*)nano_malloc((maxlen + 1) * sizeof(Uint16*));
   int i;
   for(i = 0; i < initlen; i++)
      text[i] = buf[i];
   text[initlen] = 0;

   // ... read Uint16* ...
   int len = get_line16(win, text, maxlen, 0xffff);

   // ... and convert it back to glui32*.
   for(i = 0; text[i]; i++)
      buf[i] = text[i];
  
   return len;
}

/*
 * Read any line from a window. For most arguments, see nano_input_text16() in
 * "ui.h". Return the length.
 */
glui32 get_line16(winid_t win, Uint16 *text, int max_len, int max_char)
{
   switch(win->wintype) {
   case wintype_TextBuffer:
      return nanoglk_wintextbuffer_get_line16(win, text, max_len, max_char);
      break;

   case wintype_TextGrid:
      return nanoglk_wintextgrid_get_line16(win, text, max_len, max_char);
      break;

   default:
      return 0; // TODO warning?
   }
}

/*
 * Sets the style of a window. Called (indirectly) from glk_set_style().
 */
void nanoglk_set_style(winid_t win, glui32 styl)
{
   nano_trace("nanoglk_set_style(%p, %d)", win, styl);
   win->cur_styl = styl;
}

/*
 * Convert a Glk color (glsi32) into an SDL color.
 */
static void set_color(SDL_Color *col, glsi32 val)
{
   col->r = (val & 0xff0000) >> 16;
   col->g = (val & 0xff00) >> 8;
   col->b = val & 0xff;
}

/*
 * Set a style hint, which is then used for the next window (see
 * comments at the beginning).
 *
 * - styl      any of style_* as defined by Glk
 * - hint      any of stylehint_* as defined by Glk (but only few are supported;
 *             see below)
 * - val       the value for the hint
 * - next_fg   either next_bufer_fg or next_grid_fg
 * - next_bg   either next_bufer_bg or next_grid_bg
 * - next_rev  either next_bufer_rev or next_grid_rev
 */
static void set_hint(glui32 styl, glui32 hint, glsi32 val,
                     SDL_Color *next_fg, SDL_Color *next_bg, char *next_rev)
{
   switch(hint) {
   case stylehint_TextColor:
      set_color(&next_fg[styl], val);
      break;

   case stylehint_BackColor:
      set_color(&next_bg[styl], val);
      break;
      
   case stylehint_ReverseColor:
      // We simply store the effective foreground and background, so check
      // whether the value has changed ...
      if((next_rev[styl] && !val) || (!next_rev[styl] && val)) {
         // ... then swap foreground and background, ...
         SDL_Color tmp = next_bg[styl];
         next_bg[styl] = next_fg[styl];
         next_fg[styl] = tmp;
         
         // ... and store the new value.
         next_rev[styl] = val;
      }
      
   default:
      break;
   }
}

void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, glsi32 val)
{
   nanoglk_log("glk_stylehint_set(%d, %d, %d, %d)", wintype, styl, hint, val);

   if((wintype == wintype_TextBuffer || wintype == wintype_AllTypes))
      set_hint(styl, hint, val, next_buffer_fg, next_buffer_bg,
               next_buffer_rev);

   if((wintype == wintype_TextGrid || wintype == wintype_AllTypes))
      set_hint(styl, hint, val, next_grid_fg, next_grid_bg, next_grid_rev);
}

/*
 * Clear a style hint, i. e. reset it to the user-configured value
 * (see comments at the beginning).
 *
 * - styl      any of style_* as defined by Glk
 * - hint      any of stylehint_* as defined by Glk (but only few are supported;
 *             see below)
 * - val       the value for the hint
 * - next_fg   either next_bufer_fg or next_grid_fg
 * - next_bg   either next_bufer_bg or next_grid_bg
 * - next_rev  either next_bufer_rev or next_grid_rev
 */
static void clear_hint(glui32 styl, glui32 hint,
                       SDL_Color *next_fg, SDL_Color *next_bg, char *next_rev,
                       struct nanoglk_font **font)
{
   switch(hint) {
   case stylehint_TextColor:
      next_fg[styl] = font[styl]->fg;
      break;

   case stylehint_BackColor:
      next_bg[styl] = font[styl]->bg;
      break;
      
   case stylehint_ReverseColor:
      if(next_rev[styl]) {
         // If color was reversed, reverse it again, i. e. swap background and
         // foreground.
         SDL_Color tmp = next_bg[styl];
         next_bg[styl] = next_fg[styl];
         next_fg[styl] = tmp;

         next_rev[styl] = 0;
      }
      
   default:
      break;
   }
}

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
   nanoglk_log("glk_stylehint_clear(%d, %d, %d)", wintype, styl, hint);

   if((wintype == wintype_TextBuffer || wintype == wintype_AllTypes))
      clear_hint(styl, hint, next_buffer_fg, next_buffer_bg, next_buffer_rev,
                 nanoglk_buffer_font);

   if((wintype == wintype_TextGrid || wintype == wintype_AllTypes))
      clear_hint(styl, hint, next_grid_fg, next_grid_bg, next_grid_rev,
                 nanoglk_grid_font);
}

glui32 glk_style_distinguish(winid_t win, glui32 styl1, glui32 styl2)
{
   nanoglk_log("glk_style_distinguish(%p, %d, %d) => ...", win, styl1, styl2);
   nano_fail("glk_style_distinguish not implemented");
   return 0;
}

glui32 glk_style_measure(winid_t win, glui32 styl, glui32 hint, glui32 *result)
{
   nanoglk_log("glk_style_measure(%p, %d, %d, ...) => ...", win, styl, hint);
   nano_warn("glk_style_measure not implemented");
   return 0;
}

void glk_window_flow_break(winid_t win)
{
   nanoglk_log("glk_window_flow_break(%p)", win);
   nano_warn("glk_window_flow_break not implemented"); // TODO
}

void glk_window_erase_rect(winid_t win,
                           glsi32 left, glsi32 top, glui32 width, glui32 height)
{
   nanoglk_log("glk_window_erase_rect(%p, %d, %d, %d, %d)",
               win, left, top, width, height);

   switch(win->wintype) {
   case wintype_Graphics:
      nanoglk_wingraphics_erase_rect(win, left, top, width, height);
      break;
      
   default:
      nano_warn("nanoglk_wingraphics_erase_rect not supported for wintype %d",
                win->wintype);
      break;
   }
}

void glk_window_fill_rect(winid_t win, glui32 color,
                          glsi32 left, glsi32 top, glui32 width, glui32 height)
{
   nanoglk_log("glk_window_erase_rect(%p, 0x%06x, %d, %d, %d, %d)",
               win, color, left, top, width, height);

   switch(win->wintype) {
   case wintype_Graphics:
      nanoglk_wingraphics_fill_rect(win, color, left, top, width, height);
      break;
      
   default:
      nano_warn("nanoglk_wingraphics_fill_rect not supported for wintype %d",
                win->wintype);
      break;
   }
}

void glk_window_set_background_color(winid_t win, glui32 color)
{
   nanoglk_log("glk_window_set_background_color(%p, 0x%06x)", win, color);

   switch(win->wintype) {
   case wintype_Graphics:
      nanoglk_wingraphics_set_background_color(win, color);
      break;
      
   default:
      nano_warn("glk_window_set_background_color not supported for wintype %d",
                win->wintype);
      break;
   }
}
