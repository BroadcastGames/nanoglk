#include <stdio.h>
#include "glk.h"
#include "glkstart.h"

static void wait_for_key(winid_t win)
{
   glk_request_char_event(win);
   
   int f = 1;
   while(f) {
      event_t ev;
      glk_select(&ev);
      switch(ev.type) {
      case evtype_CharInput:
         if(ev.val1 == ' ' || ev.val1 == '\n')
            f = 0;
         else
            glk_request_char_event(win);
      }
   }
}

void glk_main()
{
   winid_t win1 = glk_window_open(NULL, 0, 0, wintype_Graphics, 0);
   glk_window_set_background_color(win1, 0xff8080);
   glk_window_clear(win1);
   wait_for_key(win1);

   winid_t win2 = glk_window_open(win1,
                                  winmethod_Left | winmethod_Proportional
                                  | winmethod_Border,
                                  50, wintype_Graphics, 0);
   glk_window_set_background_color(win2, 0xffff60);
   glk_window_clear(win2);
   wait_for_key(win2);
   
   glk_exit();
}

int glkunix_startup_code(glkunix_startup_t *data)
{
   return 1;
}
