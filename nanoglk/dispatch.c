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
 * Functions neccessary for the dispatching layer.
 */

#include "nanoglk.h"

static gidispatch_rock_t dummy_disprock = { 0 };

static gidispatch_rock_t (*regi_obj)(void *obj, glui32 objclass) = NULL;
static void (*unregi_obj)(void *obj, glui32 objclass,
                          gidispatch_rock_t objrock) = NULL;
static gidispatch_rock_t (*regi_arr)(void *array, glui32 len,
                                     char *typecode) = NULL;
static void (*unregi_arr)(void *array, glui32 len, char *typecode,
                          gidispatch_rock_t objrock) = NULL;

void gidispatch_set_object_registry(gidispatch_rock_t (*regi)(void *obj,
                                                              glui32 objclass), 
                                    void (*unregi)(void *obj, glui32 objclass,
                                                   gidispatch_rock_t objrock))
{
   nanoglk_log("gidispatch_set_object_registry(%p, %p)", regi, unregi);

   regi_obj = regi;
   unregi_obj = unregi;

   // Register objects which have already been created.
   for(winid_t win = glk_window_iterate(NULL, NULL);
       win; win = glk_window_iterate(win, NULL))
      win->disprock = nanoglk_call_regi_obj(win, gidisp_Class_Window);
   for(strid_t str = glk_stream_iterate(NULL, NULL);
       str; str = glk_stream_iterate(str, NULL))
      str->disprock = nanoglk_call_regi_obj(str, gidisp_Class_Stream);
   for(frefid_t fref = glk_fileref_iterate(NULL, NULL); 
       fref; fref = glk_fileref_iterate(fref, NULL))
      fref->disprock = nanoglk_call_regi_obj(fref, gidisp_Class_Fileref);
   for(schanid_t sch = glk_schannel_iterate(NULL, NULL); 
       sch; sch = glk_schannel_iterate(sch, NULL))
      sch->disprock = nanoglk_call_regi_obj(sch, gidisp_Class_Schannel);
}

gidispatch_rock_t gidispatch_get_objrock(void *obj, glui32 objclass)
{
   gidispatch_rock_t disprock;

   switch (objclass) {
   case gidisp_Class_Window: disprock = ((winid_t)obj)->disprock; break;
   case gidisp_Class_Stream: disprock = ((strid_t)obj)->disprock; break;
   case gidisp_Class_Fileref: disprock = ((frefid_t)obj)->disprock; break;
   case gidisp_Class_Schannel: disprock = ((schanid_t)obj)->disprock; break;
   default:
      nano_fail("gidispatch_get_objrock: unknown objclass %d", objclass);
      disprock = dummy_disprock;
      break;
   }

   nanoglk_log("gidispatch_get_objrock(%p, %d) => (%d / %p)",
               obj, objclass, disprock.num, disprock.ptr);
   return disprock;
}

/*
 * Wrapper for "regi_obj", as passed to gidispatch_set_object_registry().
 * This function must be called in other parts of nanoglk, whenever an object
 * is created.
 */
gidispatch_rock_t nanoglk_call_regi_obj(void *obj, glui32 objclass)
{
   if(regi_obj) {
      gidispatch_rock_t disprock = regi_obj(obj, objclass);
      nanoglk_log("registering object %p, class %d => (%d / %p)",
                  obj, objclass, disprock.num, disprock.ptr);
      return disprock;
   } else
      return dummy_disprock;
}

/*
 * Wrapper for "unregi_obj", as passed to gidispatch_set_object_registry().
 * This function must be called in other parts of nanoglk, whenever an object
 * is destroyed.
 */
void nanoglk_call_unregi_obj(void *obj, glui32 objclass,
                             gidispatch_rock_t objrock)
{
   if(unregi_obj) {
      nanoglk_log("unregistering object %p, class %d, rock (%d / %p)",
                  obj, objclass, objrock.num, objrock.ptr);
      unregi_obj(obj, objclass, objrock);
   }
}

void gidispatch_set_retained_registry(gidispatch_rock_t (*regi)(void *array,
                                                                glui32 len,
                                                                char *typecode),
 
                                      void (*unregi)(void *array, glui32 len,
                                                     char *typecode, 
                                                     gidispatch_rock_t objrock))
{
   nanoglk_log("gidispatch_set_retained_registry(%p, %p)", regi, unregi);
   regi_arr = regi;
   unregi_arr = unregi;
}

/*
 * Wrapper for "regi_arr", as passed to gidispatch_set_retained_registry().
 * This function must be called in other parts of nanoglk, whenever an array
 * is created.
 */
gidispatch_rock_t nanoglk_call_regi_arr(void *array, glui32 len,
                                        char *typecode)
{
   if(regi_arr) {
      gidispatch_rock_t disprock = regi_arr(array, len, typecode);
      nanoglk_log("registering array ..., len %d, typecode '%s' => (%d / %p)",
                  len, typecode, disprock.num, disprock.ptr);
      return disprock;
   } else
      return dummy_disprock;
}

/*
 * Wrapper for "unregi_arr", as passed to gidispatch_set_retained_registry().
 * This function must be called in other parts of nanoglk, whenever an array
 * is destroyed.
 */
void nanoglk_call_unregi_arr(void *array, glui32 len, char *typecode,
                             gidispatch_rock_t objrock)
{
   if(unregi_arr) {
      nanoglk_log("unregistering array ..., len %d, typecode '%s', "
                  "rock (%d / %p)", len, typecode, objrock.num, objrock.ptr);
      unregi_arr(array, len, typecode, objrock);
   }
}
