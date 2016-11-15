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
 * Handling events. (Some functions related to events are found in
 * other sources.)
 *
 * Most of this is likely to be rewritten. Currently, only user
 * requested events are handled at all, and the implementation may not
 * be fully compliant with the specification. There is no focus, but
 * instead, the user must input something in the window, for which an
 * event was requested first. For other events than character and line
 * input (other event types are not yet supported) this works halfway,
 * but handlung mouse events will not be possible this way.
 */

#include "nanoglk.h"
#include <unistd.h>

/* All requested events are put in a queue, and returned in the same order. */
struct queued_event
{
   struct queued_event *prev;
   int type;       // one of evtype_* defined in "glk_h"
   int uni;        // 1 when unicode character or line is requested
   winid_t win;
   void *buf;      // this (actually char* or glui32*) ...
   glui32 maxlen;  // ... and this ...
   glui32 initlen; // ... and this is used for line input requests.
};

static struct queued_event *first_qe = NULL, *last_qe = NULL;
static glui32 timer_millisecs = 0; // set by glk_request_timer_events()

static void put_event(int type, int uni, winid_t win, void *buf,
                      glui32 maxlen, glui32 initlen);
static struct queued_event *get_event(void);

void glk_select(event_t *event)
{
   nanoglk_window_flush_all();

   event->win = NULL;
   event->val1 = event->val2 = 0;
 
   struct queued_event *qe = get_event();
   if(qe == NULL) {
      nano_trace("glk_select: nothing in queue");
      // Very simple implemention; does not consider the time which has passed
      // since glk_request_timer_events(), and timer events are returned at the
      // lowest priority (only when there is no other requested event).
      if(timer_millisecs > 0) {
         nano_info("glk_select: wait %d msecs", timer_millisecs);
         usleep(1000 * timer_millisecs);
         event->type = evtype_Timer;
      } else
         event->type = evtype_None;
   } else {
      // Generally, take the first requested event from the queue, and
      // let the user input what is requested, in this window. The user
      // has no control where to input something, a focus does not exist.
      event->win = qe->win;
      event->type = qe->type;
      nano_trace("glk_select: %d in queue", qe->type);

      switch(qe->type) {
      case evtype_CharInput:
         event->val1 = qe->uni ? nanoglk_window_get_char_uni(qe->win)
            : nanoglk_window_get_char(qe->win);
         break;

      case evtype_LineInput:
         if(qe->uni) {
            event->val1 = nanoglk_window_get_line_uni(qe->win, (glui32*)qe->buf,
                                                      qe->maxlen, qe->initlen);
            nanoglk_call_unregi_arr(qe->buf, qe->maxlen, "&+#!Iu",
                                    qe->win->arrrock);
         } else {
            event->val1 = nanoglk_window_get_line(qe->win, (char*)qe->buf,
                                                  qe->maxlen, qe->initlen);
            nanoglk_call_unregi_arr(qe->buf, qe->maxlen, "&+#!Cn",
                                    qe->win->arrrock);
         }
         break;
      }

      free(qe);
   }

   nanoglk_log("glk_select(...) => (%d, %p, %d, %d)",
               event->type, event->win, event->val1, event->val2);
}

void glk_select_poll(event_t *event)
{
   nanoglk_log("glk_select_poll(...)");
   nano_fail("glk_select_poll not implemented");
}

void glk_request_timer_events(glui32 millisecs)
{
   nanoglk_log("glk_request_timer_event(%d)", millisecs);
   timer_millisecs = millisecs; // See glk_select().
}

void glk_request_char_event(winid_t win)
{
   nanoglk_log("glk_request_char_event(%p)", win);
   put_event(evtype_CharInput, 0, win, NULL, 0, 0);
}

void glk_request_char_event_uni(winid_t win)
{
   nanoglk_log("glk_request_char_event_uni(%p)", win);
   put_event(evtype_CharInput, 1, win, NULL, 0, 0);
}

void glk_request_line_event(winid_t win, char *buf, glui32 maxlen,
                            glui32 initlen)
{
   nanoglk_log("glk_request_line_event(%p, ..., %d, %d)", win, maxlen, initlen);
   put_event(evtype_LineInput, 0, win, buf, maxlen, initlen);
   win->arrrock = nanoglk_call_regi_arr(buf, maxlen, "&+#!Cn");
}

void glk_request_line_event_uni(winid_t win, glui32 *buf, glui32 maxlen,
                                glui32 initlen)
{
   nano_info("glk_request_line_event_uni(%p, ..., %d, %d)",
             win, maxlen, initlen);
   put_event(evtype_LineInput, 1, win, buf, maxlen, initlen);
   win->arrrock = nanoglk_call_regi_arr(buf, maxlen, "&+#!Iu");
}

void glk_request_mouse_event(winid_t win)
{
   nanoglk_log("glk_request_mouse_event(%p)", win);

   nano_warn("glk_request_mouse_event not implemented");
}

/*
 * Removes an event of a given type and for a given window from the
 * queue.
 *
 * Returns the queued event, or NULL, when no event has been
 * found. The event is removed from the queue, but not freed, so the
 * caller has to check whether it is NULL or not, and, in the latter
 * case, call free().
 */
static struct queued_event *cancel_event(winid_t win, int type)
{
   static struct queued_event *qe, *pr;
   for(qe = last_qe, pr = NULL; qe; pr = qe, qe = qe->prev) {
      if((qe->type == type) && qe->win == win) {
         if(pr) {
            pr->prev = qe->prev;
            if(pr->prev == NULL)
               last_qe = pr;
         } else {
            first_qe = qe->prev;
            if(first_qe == NULL || first_qe->prev == NULL)
               last_qe = first_qe;
         }
         
         return qe;
      }
   }
   
   return NULL;
}

void glk_cancel_line_event(winid_t win, event_t *event)
{
   nanoglk_log("glk_cancel_line_event(%p, ...)", win);

   struct queued_event *qe = cancel_event(win, evtype_LineInput);
   if(qe && event) {
      nanoglk_call_unregi_arr(qe->buf, qe->maxlen,
                              qe->uni ? "&+#!Iu" : "&+#!Cn", qe->win->arrrock);
      event->win = win;
      event->type = evtype_LineInput;
      event->val1 = qe->initlen;
      event->val2 = 0;
   }
   if(qe)
      free(qe);
}

void glk_cancel_char_event(winid_t win)
{
   nanoglk_log("glk_cancel_char_event(%p)", win);

   struct queued_event *qe = cancel_event(win, evtype_CharInput);
   if(qe)
      free(qe);
}

void glk_cancel_mouse_event(winid_t win)
{
   nanoglk_log("glk_cancel_mouse_event(%p)", win);
   nano_warn("glk_cancel_mouse_event not implemented");
}

/*
 * Put a requested event in the queue. Called by all glk_request_*()
 * functions, except glk_request_timer_events().
 */
static void put_event(int type, int uni, winid_t win, void *buf,
                      glui32 maxlen, glui32 initlen)
{
   for(struct queued_event *qe = last_qe; qe; qe = qe->prev)
      if(qe->win == win) {
         nano_warn("event for window %p (type %d) already requested",
                   qe->win, qe->type);
         return;
      }

   struct queued_event *qe =
      (struct queued_event*)nano_malloc(sizeof(struct queued_event));
   qe->type = type;
   qe->uni = uni;
   qe->win = win;
   qe->buf = buf;
   qe->maxlen = maxlen;
   qe->initlen = initlen;

   if(first_qe == NULL) {
      first_qe = last_qe = qe;
      qe->prev = NULL;
   } else {
      first_qe->prev = qe;
      qe->prev = NULL;
      first_qe = qe;
   }
}

/*
 * Returns the first event from the queue.
 *
 * Returns the queued event, or NULL, when the queue is empty. The
 * event is removed from the queue, but not freed, so the caller has
 * to check whether it is NULL or not, and, in the latter case, call
 * free().
 */
static struct queued_event *get_event(void)
{
   if(last_qe == NULL)
      return NULL;
   else {
      struct queued_event *qe = last_qe;
      last_qe = qe->prev;
      if(last_qe == NULL)
         first_qe = NULL;
      return qe;
   }
}

void glk_set_hyperlink(glui32 linkval)
{
   nanoglk_log("glk_set_hyperlink(%d)", linkval);
   nano_warn("glk_set_hyperlink not implemented"); // TODO
}

void glk_set_hyperlink_stream(strid_t str, glui32 linkval)
{
   nanoglk_log("glk_set_hyperlink_stream(%p, %d)", str, linkval);
   nano_warn("glk_set_hyperlink_stream not implemented"); // TODO
}

void glk_request_hyperlink_event(winid_t win)
{
   nanoglk_log("glk_request_hyperlink_event(%p)", win);
   nano_warn("glk_request_hyperlink_event not implemented"); // TODO
}

void glk_cancel_hyperlink_event(winid_t win)
{
   nanoglk_log("glk_cancel_hyperlink_event(%p)", win);
   nano_warn("glk_cancel_hyperlink_event not implemented"); // TODO
}
