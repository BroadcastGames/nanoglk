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
 * Miscellaneous, which does not fit anywhere else.
 */

#include "misc.h"
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#ifdef LOG_FILE
static FILE *log;
#define LOG_ENABLED
#endif

#ifdef LOG_STD
#define log stdout
#define LOG_ENABLED
#endif

struct saved_window
{
   struct saved_window *prev;
   SDL_Rect r;
   SDL_Surface *saved;
} *saved_window = NULL;

static void (*registered_key_func[26])(void);

static int _allow_suspend = FALSE;

static void quit(void);

/*
 * Some initialization. The arguments of main() should be passed.
 */
void nano_init(int argc, char *argv[], int allow_suspend)
{
#ifdef LOG_FILE
   char buf[1024];
   sprintf(buf, "%s-%d.log", argv[0], getpid());
   log = fopen(buf, "w");
#endif

   for(int i = 0; i < 26; i++)
      registered_key_func[i] = NULL;

   _allow_suspend = allow_suspend;

   atexit(quit);
}

void quit(void)
{
#ifdef LOG_FILE
   fclose(log);
#endif
   exit(0);
}

/*
 * Some general remarks about logging: Logging can be enabled by the following
 * CPP variables (e. g. defined in Makefile):
 *
 * - LOG_FILE  log to a file, given by the name of the executable and the
 *             process id
 * - LOG_STD   log to stdout
 *
 * If non of these two is defined, only warnings and errors are printed at all.
 *
 * LOG_TRACE   log also trace messages (used rather for debugging)
 *
 * There are following log levels:
 *
 * - Traces are only logged when explicitly enabled.
 * - Infos are logged when logging is enabled (LOG_FILE or LOG_STD).
 * - Warnings and errors are logged into a file, when enabled, and, in any
 *   case, printed on the screen, even if logging is not enabled.
 * - Errors furthermore abort the program.
 *
 * For examples how to extend this logging on an application level, see
 * nanoglk_log().
 */

/*
 * Return the logfile (may also be stdout), if logging is enabled; otherwise
 * NULL.
 */
FILE *nano_logfile(void)
{
#ifdef LOG_ENABLED
   return log;
#else
   return NULL;
#endif
}

void nano_trace(const char *fmt, ...)
{
#if defined(LOG_ENABLED) && defined(LOG_TRACE)
   fprintf(log, "TRACE: ");

   va_list argp;
   va_start(argp, fmt);
   vfprintf(log, fmt, argp);
   va_end(argp);
   
   fprintf(log, "\n");
   fflush(log);
#endif
}

void nano_info(const char *fmt, ...)
{
#ifdef LOG_ENABLED
   fprintf(log, "INFO: ");

   va_list argp;
   va_start(argp, fmt);
   vfprintf(log, fmt, argp);
   va_end(argp);
   
   fprintf(log, "\n");
   fflush(log);
#endif
}

void nano_warn(const char *fmt, ...)
{
#ifdef LOG_ENABLED
   fprintf(log, "WARN: ");

   va_list argp;
   va_start(argp, fmt);
   vfprintf(log, fmt, argp);
   va_end(argp);
   
   fprintf(log, "\n");
   fflush(log);
#endif
}

void nano_fail(const char *fmt, ...)
{
#ifdef LOG_FILE
   fprintf(log, "ERROR: ");

   va_list argp;
   va_start(argp, fmt);
   vfprintf(log, fmt, argp);
   va_end (argp);
   
   fprintf(log, "\n");
   fflush(log);
#endif

   fprintf(stderr, "ERROR: ");

   va_list argp2;
   va_start(argp2, fmt);
   vfprintf(stderr, fmt, argp2);
   va_end (argp2);
   
   fprintf(stderr, "\n");
   abort();
}

void nano_warnunless(int b, const char *fmt, ...)
{
#ifdef LOG_ENABLED
   if(!b) {
      fprintf(log, "WARN: ");

      va_list argp;
      va_start(argp, fmt);
      vfprintf(log, fmt, argp);
      va_end(argp);
      
      fprintf(log, "\n");
      fflush(log);
   }
#endif
}

void nano_failunless (int b, const char *fmt, ...)
{
   if(!b) {
#ifdef LOG_FILE
      fprintf(log, "ERROR: ");

      va_list argp;
      va_start(argp, fmt);
      vfprintf(log, fmt, argp);
      va_end (argp);
   
      fprintf(log, "\n");
      fflush(log);
#endif

      fprintf(stderr, "ERROR: ");

      va_list argp2;
      va_start(argp2, fmt);
      vfprintf(stderr, fmt, argp2);
      va_end (argp2);
   
      fprintf(stderr, "\n");
      abort();
   }
}

/*
 * Simple wrapper for malloc().
 */
void *nano_malloc(size_t size)
{
   void *p = malloc(size);
   nano_failunless(p != NULL, "Cannot allocate %d bytes.", size);
   return p;
}

/*
 * Register a special key. If nano_wait_event() is used instead of
 * SDL_WaitEvent(), and ALT + CTRL + this key is pressed, the respective
 * function is called.
 */
void nano_register_key(char key, void (*func)(void))
{
   nano_failunless(key >= 'a' && key <= 'z',
                   "nano_register_key: invalid key '%c'", key);
   registered_key_func[key - 'a'] = func;
}

struct saved_buffer {
   struct saved_buffer *prev, *next;
   
   SDL_Surface **surface;
   Uint8 *buf;
   int w, h, bpp;
};

static struct saved_buffer *first_saved = NULL,  *last_saved = NULL;

/*
 * Register an SDL surface used by the program. This is needed for suspending
 * the program.
 */
void nano_reg_surface(SDL_Surface **surface)
{
   struct saved_buffer *saved =
      (struct saved_buffer*)nano_malloc(sizeof(struct saved_buffer));
   saved->surface = surface;
   saved->next = NULL;

   if(last_saved) {
      last_saved->next = saved;
      saved->prev = last_saved;
      last_saved = saved;
   } else {
      saved->prev = NULL;
      last_saved = first_saved = saved;
   }
}

/*
 * Unregister an SDL surface previously registered. Should be called before
 * the surface is destroyed.
 */
void nano_unreg_surface(SDL_Surface **surface)
{
   for(struct saved_buffer *saved = first_saved; saved; saved = saved->next)
      if(saved->surface == surface) {
         if(saved->prev)
            saved->prev->next = saved->next;
         if(saved->next)
            saved->next->prev = saved->prev;

         if(saved == first_saved)
            first_saved = saved->next;
         if(saved == last_saved)
            last_saved = saved->prev;

         free(saved);
         return;
      }

   nano_fail("surface pointer %p not registered", surface);
}

// Called, when the suspended process is woken up again (e. g. by "fg" in
// a shell).
static void handle_cont()
{
   nano_info("SIGCONT");

   // Do not call handle_cont() again.
   signal(SIGCONT, SIG_DFL);

   // Reinit SDL again and restore screen contents.
   // TODO (cf. main())
   if(SDL_Init(SDL_INIT_VIDEO) != 0)
      nano_fail("Couldn't initialize SDL: %s", SDL_GetError());

   // TODO (cf. main())
#ifdef NANONOTE
   SDL_ShowCursor(SDL_DISABLE);
#endif

   SDL_EnableUNICODE(1);
   SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

   for(struct saved_buffer *saved = first_saved; saved; saved = saved->next) {
      // TODO (cf. nanoglk_window_init())
      *saved->surface = SDL_SetVideoMode(saved->w, saved->h, 8 * saved->bpp, 0);
      
      SDL_LockSurface(*saved->surface);
      memcpy((*saved->surface)->pixels,
             saved->buf, saved->w * saved->h * saved->bpp);
      SDL_UnlockSurface(*saved->surface);
      free(saved->buf);
      SDL_Flip(*saved->surface);
   }
}

// Suspend the process, by sending SIGSTOP to oneself.
static void suspend()
{
   nano_info("SIGSTOP");

   // 1. Save the contents of the surfaces (before calling SQL_Quit below).
   for(struct saved_buffer *saved = first_saved; saved; saved = saved->next) {
      saved->w = (*saved->surface)->w;
      saved->h = (*saved->surface)->h;
      saved->bpp = (*saved->surface)->format->BytesPerPixel;

      int size = saved->w * saved->h * saved->bpp;
      saved->buf = malloc(size);
      SDL_LockSurface(*saved->surface);
      memcpy(saved->buf, (*saved->surface)->pixels, size);
      SDL_UnlockSurface(*saved->surface);
   }

   // 2. Ensure that the process can be woken up again.
   signal(SIGCONT, handle_cont);

   // 3. SDL_Quit will have different effects: on X11, the window is closed,
   //    in framebuffer mode, the text mode is restored.
   SDL_Quit();

   // 4. Finally, stop.
   kill(getpid(), SIGSTOP);
}

/*
 * A wrapper for SDL_WaitEvent(), which adds suspension via CTRL+Z, as well
 * as registered functions via ALT+CTRL+..., and some workarounds for the Ben
 * NanoNote.
 */
void nano_wait_event(SDL_Event *event)
{
   while(1) {
      if(!SDL_WaitEvent(event))
         nano_fail("SDL_WaitEvent returned 0");

      // suspend
      if(_allow_suspend &&
#ifdef NANONOTE
         // On Nanonote, KMOD_LCTRL is the "fn" key.
         (event->key.keysym.mod & KMOD_RCTRL) == KMOD_RCTRL
#else
         ((event->key.keysym.mod & KMOD_LCTRL) == KMOD_LCTRL ||
          (event->key.keysym.mod & KMOD_RCTRL) == KMOD_RCTRL)
#endif
         && (event->key.keysym.sym == 'z')) {
         suspend();
         continue;
      }

      // Alt+Ctrl+<x> => registered function
      if(event->type ==  SDL_KEYDOWN &&
         event->key.keysym.sym >= 'a' && event->key.keysym.sym <= 'z' &&
         registered_key_func[event->key.keysym.sym - 'a'] &&
#ifdef NANONOTE
         // Only left Alt and right Ctrl: KMOD_RALT defines the red shift key,
         // and KMOD_LCTRL "Fn" (for numbers).
         (event->key.keysym.mod & KMOD_LALT) &&
         (event->key.keysym.mod & KMOD_RCTRL)
#else
         // Left or right Alt, AND left or right Ctrl.
         (event->key.keysym.mod & (KMOD_LALT | KMOD_RALT)) &&
         (event->key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
#endif
         ) {
         registered_key_func[event->key.keysym.sym - 'a']();
         continue;
      }

#ifdef NANONOTE
      // NanoNote speficic keys. Unfortunately not handled by SDL.
      // KMOD_LCTRL defines the "Fn" key. */
      if(event->type == SDL_KEYDOWN &&
         (event->key.keysym.mod & KMOD_LCTRL) == KMOD_LCTRL) {
         switch (event->key.keysym.sym) {
         case '/': event->key.keysym.unicode = '0'; return;
         case 'n': event->key.keysym.unicode = '1'; return;
         case 'm': event->key.keysym.unicode = '2'; return;
         case '=': event->key.keysym.unicode = '3'; return;
         case 'j': event->key.keysym.unicode = '4'; return;
         case 'k': event->key.keysym.unicode = '5'; return;
         case 'l': event->key.keysym.unicode = '6'; return;
         case 'u': event->key.keysym.unicode = '7'; return;
         case 'i': event->key.keysym.unicode = '8'; return;
         case 'o': event->key.keysym.unicode = '9'; return;
         default: return;
         }
      }
#endif

      return;
   }
}

/*
 * Save a region within an SDL surface. Useful for dialogs etc.
 */
void nano_save_window(SDL_Surface *surface, int x, int y, int w, int h)
{
   struct saved_window *sw =
      (struct saved_window*)nano_malloc(sizeof(struct saved_window));
   sw->prev = saved_window;
   saved_window = sw;

   saved_window->r.x = x;
   saved_window->r.y = y;
   saved_window->r.w = w;
   saved_window->r.h = h;
   SDL_Rect r = { 0,  0, w, h };
   // TODO some arguments
   saved_window->saved =
      SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 24, 0, 0, 0, 0);
   SDL_BlitSurface(surface, &saved_window->r, saved_window->saved, &r);
}

/*
 * Restore the last saved region.
 */
void nano_restore_window(SDL_Surface *surface)
{
   if(saved_window) {
      SDL_Rect r = { 0,  0, saved_window->r.w, saved_window->r.h };
      SDL_BlitSurface(saved_window->saved, &r, surface, &saved_window->r);
      SDL_FreeSurface(saved_window->saved);

      struct saved_window *sw = saved_window;
      saved_window = saved_window->prev;
      free(sw);
   }
}

