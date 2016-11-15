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
 * Sounds. Actually nothing is implemented yet, but empty structures exists,
 * so a program using nanoglk should not notice at all.
 */

#include "nanoglk.h"

static schanid_t first = NULL, last = NULL;

schanid_t glk_schannel_create(glui32 rock)
{
   schanid_t sch = (schanid_t)nano_malloc(sizeof(struct glk_schannel_struct));
   nanoglk_log("glk_schannel_create(%d) => %p", rock, sch);
   sch->rock = rock;
   ADD(sch);
   sch->disprock = nanoglk_call_regi_obj(sch, gidisp_Class_Schannel);
   return sch;
}

void glk_schannel_destroy(schanid_t chan)
{
   nanoglk_log("glk_schannel_destroy(%p)", chan);
   nanoglk_call_unregi_obj(chan, gidisp_Class_Schannel, chan->disprock);
   UNLINK(chan);
   free(chan);
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rockptr)
{
   schanid_t next;

   if(chan == NULL)
      next = first;
   else
      next = chan->next;

   if(next && rockptr)
      *rockptr = next->rock;
   
   nanoglk_log("glk_schannel_iterate(%p, ...) => %p", chan, next);
   return next;
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
   nanoglk_log("glk_schannel_get_rock(%p) => %d", chan, chan->rock);
   return chan->rock;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
   nanoglk_log("glk_schannel_play(%p, %d) => 0", chan, snd);
   nano_warn("glk_schannel_play not implemented");
   return 0;
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats,
                             glui32 notify)
{
   nanoglk_log("glk_schannel_play_ext(%p, %d, %d, %d) => 0",
               chan, snd, repeats, notify);
   nano_warn("glk_schannel_play_ext not implemented");
   return 0;
}

void glk_schannel_stop(schanid_t chan)
{
   nanoglk_log("glk_schannel_stop(%p)", chan);
   nano_warn("glk_schannel_stop not implemented");
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
   nanoglk_log("glk_schannel_set_volume(%p, %d)", chan, vol);
   nano_warn("glk_schannel_set_volume not implemented");
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
   nanoglk_log("glk_schannel_load_hint(%d, %d)", snd, flag);
   nano_warn("glk_sound_load_hint not implemented");
}
