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
 * This is the place, where everything else from the "misc" directoty
 * can be tested.
 */

#include "misc/misc.h"

int main(int argc, char **argv)
{
   nano_init(argc, argv, 0);

   char buf[1024];
   nano_expand_env("The value of ${HOME}/foo/bar", buf, 1023);
   printf("'The value of ${HOME}/foo/bar' is: %s\n", buf);
   nano_expand_env("The value of ${HOPEFULLYNOTDEFINED}/foo/bar", buf, 1023);
   printf("'The value of ${HOPEFULLYNOTDEFINED}/foo/bar' is: %s\n", buf);

   return 0;
}

