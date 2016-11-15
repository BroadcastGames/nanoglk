/* Copied from CheapGlk version 1.0.3.
 *
 * Original notice:
 *
 * The source code in this package is copyright 1998-2012 by Andrew
 * Plotkin. You may copy and distribute it freely, by any means and
 * under any conditions, as long as the code and documentation is not
 * changed. You may also incorporate this code into your own program
 * and distribute that, or modify this code and use and distribute the
 * modified version, as long as you retain a notice in your program or
 * documentation which mentions my name and the URL shown above.
 *
 * Modified by me, Sebastian Geerken: added logging.
 */

#include "nanoglk.h"

/* We'd like to be able to deal with game files in Blorb files, even
   if we never load a sound or image. We'd also like to be able to
   deal with Data chunks. So we're willing to set a map here. */

static giblorb_map_t *blorbmap = 0; /* NULL */

giblorb_err_t giblorb_set_resource_map(strid_t file)
{
   giblorb_err_t err;
  
   err = giblorb_create_map(file, &blorbmap);
   if (err) {
      blorbmap = 0; /* NULL */
      nanoglk_log("giblorb_set_resource_map(%p) => %d", file, err);
      return err;
   }
   
   return giblorb_err_None;
}

giblorb_map_t *giblorb_get_resource_map()
{
   nanoglk_log("giblorb_get_resource_map() => %p", blorbmap);
   return blorbmap;
}
