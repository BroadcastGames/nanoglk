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
 * Functions for strings. Most of them are 16-bit variants of the functions
 * defined in <string.h>. For convenience, "misc.h" defines aliases without
 * the "nano_" prefix.
 */

#include "misc.h"

int nano_strlen16(const Uint16 *text)
{
   int n = 0;
   while(text[n])
      n++;
   return n;
}

void nano_strcpy16(Uint16 *dest, const Uint16 *src)
{
   int i;
   for(i = 0; src[i]; i++)
      dest[i] = src[i];
   dest[i] = 0;
}

void nano_strcat16(Uint16 *dest, const Uint16 *src)
{
   nano_strcpy16(dest + nano_strlen16(dest), src);
}

Uint16 *nano_strdup16(const Uint16 *src)
{
   Uint16 *dest =
      (Uint16*)nano_malloc((nano_strlen16(src) + 1) * sizeof(Uint16));
   nano_strcpy16(dest, src);
   return dest;
}

int nano_strcmp16(const Uint16 *s1, const Uint16 *s2)
{
   while(*s1 && *s1 == *s2) {
      s1++;
      s2++;
   }

   return *s1 - *s2;
}

Uint16 *nano_strchr16(const Uint16 *s, Uint16 c)
{
   for(; *s; s++) {
      if(*s == c)
         return (Uint16*)s;
   }

   return NULL;
}

Uint16 *nano_strrchr16(const Uint16 *s, Uint16 c)
{
   for(const Uint16 *t = s + nano_strlen16(s); t >= s; t--) {
      if(*t == c)
         return (Uint16*)t;
   }

   return NULL;
}

/*
 * Converts UTF-8 to a 16-bit string, which is newly allocated (so the caller
 * has to free(3) the return value).
 */
Uint16 *nano_strdup16fromutf8(const char *src)
{
   int l = 0, error = 0;
   for(int i = 0; !error && src[i]; ) {
      if((src[i] & 0x80) == 0)
         i++;
      else {
         int mask = 0xe0, bits = 0xc0, done = 0;
         for(int j = 1; !done && !error && j < 7;
             j++, mask = 0x80 | (mask >> 1), bits = 0x80 | (bits >> 1)) {
            if(((unsigned char)src[i] & mask) == bits) {
               done = 1;
               i++;
               for(int k = 0; !error && k < j; k++) {
                  if(((unsigned char)src[i] & 0xc0) == 0x80)
                     i++;
                  else
                     error = 1;
               }
            }
         }

         if(!done)
            error = 1;
      }
         
      if(!error)
         l++;
   }

   if(error)
      return NULL;

   Uint16 *dest = (Uint16*)nano_malloc((l + 1) * sizeof(Uint16));
   
   int di = 0;
   for(int i = 0; di < l; ) {
      if((src[i] & 0x80) == 0) {
         dest[di] = src[i];
         i++;
      } else {
         int mask = 0xe0, bits = 0xc0, done = 0;
         for(int j = 1; !done && j < 7;
             j++, mask = 0x80 | (mask >> 1), bits = 0x80 | (bits >> 1)) {
            if(((unsigned char)src[i] & mask) == bits) {
               done = 1;
               dest[di] = (unsigned char)src[i] & ~mask & 0xff;
               i++;
               for(int k = 0; k < j; k++) {
                  dest[di] = (dest[di] << 6) | ((unsigned char)src[i] & 0x3f);
                  i++;
               }
            }
         }
      }
      
      di++;
   }
   
   dest[l] = 0;
   return dest;
}

/*
 * Converts a 16-bit string to UTF-8, which is newly allocated (so the caller
 * has to free(3) the return value).
 */
char *nano_strduputf8from16(const Uint16 *src)
{
   int l = 0;
   for(int i = 0; src[i]; i++) {
      if(src[i] < 0x80)
         l++;
      else {
         int done = 0;
         for(int j = 0; !done; j++) {
            if(src[i] < (2 << (5 -j + (j + 1) * 6))) {
               l += j + 2;
               done = 1;
            }
         }
      }
   }

   char *dest = (char*)nano_malloc((l + 1) * sizeof(char));

   int di = 0;
   for(int i = 0; src[i]; i++) {
      if(src[i] < 0x80)
         dest[di++] = src[i];
      else {
         int done = 0, mask = 0xc0;
         for(int j = 0; !done; j++, mask = 0x80 | (mask >> 1)) {
            if(src[i] < (2 << (5 -j + (j + 1) * 6))) {
               dest[di++] = mask | src[i] >> ((j + 1) * 6);
               for(int k = j; k >= 0; k--)
                  dest[di++] = 0x80 | ((src[i] >> (j * 6)) & 0x3f);
               done = 1;
            }
         }
      }
   }

   dest[l] = 0;
   return dest;
}

/*
 * Replace occurences of "${var}" by the value of the environment variable
 * "var", in "src"; result is written in "dest", with "maxlen" characters
 * (excluding 0-terminator) at max.
 */
void nano_expand_env(const char *src, char *dest, int maxlen)
{
   int id = 0;
   for(int is = 0; src[is]; is++) {
      char *end;
      if(src[is] == '$' && src[is + 1] == '{' &&
         (end = strchr(src + is + 2, '}')) != NULL) {
         char *key = strndup(src + is +2, end - (src + is +2));
         char *value = getenv(key);
         if(value) {
            // TODO check maxlen
            strcpy(dest + id, value);
            id += strlen(value);
            is = end - src;
            continue;
         }
      }
        
      dest[id++] = src[is];
   }

   dest[id] = 0;
}
