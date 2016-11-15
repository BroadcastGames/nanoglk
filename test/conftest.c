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
 * Some tests for handling configuration.
 */

#include "misc/misc.h"

int main(int argc, char **argv)
{
   const char *pattern1[] = { "a", CONF_WILD_ANY, "c", NULL };
   const char *pattern2[] = { "a", CONF_WILD_ONE, "c", NULL };
   const char *pattern3[] = { "a", "b", "c", NULL };

   const char *path1[] = { "a", "x", "y", "c", NULL };
   const char *path2[] = { "a", "x", "c", NULL };
   const char *path3[] = { "a", "b", "c", NULL };
   const char *path4[] = { "a", "b", "x", NULL };

   conf_t conf = nano_conf_init();
   nano_conf_put(conf, pattern1, "val1");
   nano_conf_put(conf, pattern2, "val2");
   nano_conf_put(conf, pattern3, "val3");
   nano_conf_read(conf, "test.cfg");
   
   printf("'%s'\n", nano_conf_get(conf, path1, "unset"));
   printf("'%s'\n", nano_conf_get(conf, path2, "unset"));
   printf("'%s'\n", nano_conf_get(conf, path3, "unset"));
   printf("'%s'\n", nano_conf_get(conf, path4, "unset"));

   nano_conf_free(conf);

   return 0;
}
