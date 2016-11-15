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
 * Handling configuration files, as described in README.
 */

#include "misc.h"
#include <errno.h>

#define MAX_LINE 8192
#define MAX_PATTERN 64

/*
 * A single configuration definition, as part of a linked list.
 */
struct node
{
   struct node *next;    // next in the linked list.
   const char **pattern; /* NULL-terminated; also, CONF_WILD_ONE and
                            CONF_WILD_ANY are used */
   const char *value;    // the result, if this pattern is selected
   int specifity;        // result of specifity(pattern)
};

/*
 * This acts as a container for all configuration. In "misc.h", a pointer to
 * this is defined as "cont_t". More is not exposed to the outside.
 */
struct conf
{
   struct node *first;
};

/*
 * Calculate the specifity of a pattern.
 */
static int specifity(const char **pattern)
{
   int s = 0;

   for(int i = 0; pattern[i]; i++) {
      if(pattern[i] == CONF_WILD_ANY)
         s += 1;
      else if(pattern[i] == CONF_WILD_ONE)
         s += 100;
      else
         s += 10000;
   }

   return s;
}

/*
 * Construct a new node for a definition. Anything expect "next" is defined.
 * "pattern" and "value" are copied.
 */
static struct node *new_node(const char **pattern, const char *value)
{
   struct node *node = (struct node *)nano_malloc(sizeof(struct node));

   node->next = NULL;
   
   int l = 0; while(pattern[l]) l++;
   node->pattern = (const char**)nano_malloc((l + 1) * sizeof(char*));
   for(int i = 0; i < l; i++) {
      if(pattern[i] == CONF_WILD_ONE || pattern[i] == CONF_WILD_ANY)
         node->pattern[i] = pattern[i];
      else
         node->pattern[i] = strdup(pattern[i]);
   }
   node->pattern[l] = NULL;

   node->value = value != NULL ? strdup(value) : NULL;
   node->specifity = specifity(node->pattern);

   return node;
}

/*
 * Return whether a path matches a given pattern.
 */
static int match(const char **pattern, const char **path)
{
   nano_trace("      testing %s against %s",
              (path[0] == NULL ? "<end>" : path[0]),
              (pattern[0] == NULL ? "<end>" :
               (pattern[0] == CONF_WILD_ONE ? "<?>" :
                pattern[0] == CONF_WILD_ANY ? "<*>" : pattern[0])));

   if(pattern[0] == NULL)
      // empty path matches empty pattern (or non-empty path does NOT match
      // empty pattern)
      return path[0] == NULL;
   else if(pattern[0] == CONF_WILD_ONE)
      // "?" matches anything non-empty; continue recursively
      return path[0] != NULL && match(pattern + 1, path + 1);
   else if(pattern[0] == CONF_WILD_ANY) {
      // for "*", several possibilities have to be tested
      if(pattern[1] == NULL)
         // simpest case: end of path
         return 1;
      else {
         // Test different possibilities: "*" represents no part of the pattern
         // (i = 0), one part of the pattern (i = 1) etc.
         for(int i = 0; path[i]; i++)
            if(match(pattern + 1, path + i))
               return 1;
         return 0;
      }
   } else
      // exact match; continue recursively
      return path[0] != NULL && strcmp(path[0], pattern[0]) == 0 
         && match(pattern + 1, path + 1);
}

/*
// useful for debugging
static void print(conf_t conf)
{
   for(struct node *i = conf->first; i; i = i->next) {
      for(int j  = 0; i->pattern[j]; j++) {
         if(j != 0)
            printf(".");
         printf("%s",
                (i->pattern[j] == NULL ? "<end>" :
                 (i->pattern[j] == CONF_WILD_ONE ? "<?>" :
                      i->pattern[j] == CONF_WILD_ANY ? "<*>" : i->pattern[j])));
      }
      printf("=%s [%d]\n", i->value, i->specifity);
   }
}
*/

/*
 * Create an empty configuration.
 */
conf_t nano_conf_init(void)
{
   conf_t conf = (conf_t)nano_malloc(sizeof(struct conf));
   conf->first = NULL;
   return conf;
}

/*
 * Parse a line (used internally). WARNING: "line" may be destroyed. "filename"
 * and "lineno" are used for messages.
 */
static void read_line(conf_t conf, char *line, const char *filename, int lineno)
{
   // skip white spaces at the beginning
   char *start = line;
   while(*start == ' ' || *start == '\t')
      start++;
         
   // Ignore comments.
   if(*start != '#') {
      // "Remove" newline and white spaces at the end.
      char *end = start + strlen(start);
      if(end > start && end[-1] == '\n')
         end--;
      while(end > start && (end[-1] == ' ' || end[-1] == '\t'))
         end--;
      *end = 0;
      
      if(strlen(start) >= 8 && memcmp(start, "!include", 8) == 0) {
         // include an other file
         char *arg = start + 8;
         while(*arg == ' ' || *arg == '\t')
            arg++;

         char filename[FILENAME_MAX + 1];
         nano_expand_env(arg, filename, FILENAME_MAX);
         nano_conf_read(conf, filename);
      } else {
         char *eq = strchr(start, '=');
         if(eq == NULL) {
            // Do not warn if line contains only white spaces.
            if(*start)
               nano_warn("file '%s', line %d: no '=' found", filename, lineno);
         } else {
            // assignment
            char *eq2 = eq;
            while(eq2 > start && (eq2[-1] == ' ' || eq2[-1] == '\t'))
               eq2--;
            
            if(eq2 == start) {
               nano_warn("file '%s', line %d: no identifier", filename, lineno);
            } else {
               
               *eq2 = 0;
               
               char *val = eq + 1;
               while(*val == ' ' || *val == '\t')
                  val++;
               
               char *t1, *t2, *pattern[MAX_PATTERN + 1];
               int n = 0;
               for(t1 = start; t1; t1 = t2) {
                  t2 = strchr(t1, '.');
                  if(t2) {
                     *t2 = 0;
                     t2++;
                  }
                  if(n < MAX_PATTERN) {
                     if(strcmp(t1, "*") == 0)
                        pattern[n] = CONF_WILD_ANY;
                     else if(strcmp(t1, "?") == 0)
                        pattern[n] = CONF_WILD_ONE;
                     else
                        pattern[n] = t1;
                     n++;
                  } else
                     nano_warn("file '%s', line %d: identifier too long",
                               filename, lineno);
               }
               
               pattern[n] = NULL;
               // TODO get usage of "const" right
               nano_conf_put(conf, (const char**)pattern, val);
            }
         }
      }
   }
}

/*
 * Read a configuration file.
 */
void nano_conf_read(conf_t conf, const char *filename)
{
   FILE *file = fopen(filename, "r");
   if(file == NULL)
      nano_warn("cannot open file '%s': %s", filename, strerror(errno));
   else {
      char buf[MAX_LINE + 1];
      int lineno = 0;

      while(fgets(buf, MAX_LINE, file)) {
         lineno++;
         read_line(conf, buf, filename, lineno);
      }

      fclose(file);
   }
}

/*
 * Parse a line (used externally). "filename" and "lineno" are used for
 * messages.
 */
void nano_conf_read_line(conf_t conf, const char *line, const char *filename,
                         int lineno)
{
   // TODO Not very elegant: read_line() modifies the line, so a copy is
   // passed here, in nano_conf_read_line(). Perhaps read_line() should
   // reverse the changes.
   char *copy = strdup(line);
   read_line(conf, copy, filename, lineno);
   free(copy);
}


/*
 * Free a configuration.
 */
void nano_conf_free(conf_t conf)
{
   for(struct node *i = conf->first; i; ) {
      struct node *n = i->next;
      
      for(int j = 0; i->pattern[j]; j++)
         if(i->pattern[j] != CONF_WILD_ONE && i->pattern[j] != CONF_WILD_ANY)
            // TODO get usage of "const" right!
            free((char*)i->pattern[j]);

      free(i->pattern);
      free((char*)i->value); // TODO get usage of "const" right!
      free(i);

      i = n;
   }
   
   free(conf);
}

/*
 * Add a definition to a configuration. "pattern" is NULL-terminated. Both
 * "pattern" and "value" are copied.
 */
void nano_conf_put(conf_t conf, const char **pattern, const char *value)
{
   struct node *node = new_node(pattern, value);

   // Sort it, so that the more specific patterns are at the beginning.
   // When two patterns are equally specific, the second preceeds the first,
   // so that it is preferred.
   int done = 0;
   struct node *p, *i;
   for(i = conf->first, p = NULL; !done && i; p = i, i = i->next) {
      if(i->specifity <= node->specifity) {
         // Found a position to insert the new node. "=" in "<=" makes sure
         // that newer, equally specific patterns preceed the older ones.
         if(p)
            p->next = node;
         else
            conf->first = node;
         node->next = i;
         done = 1;
      }
   }

   if(!done) {
      if(p)
         p->next = node;
      else
         conf->first = node;
      node->next = NULL;
   }
}

/*
 * Search the value for a NULL-terminated path. If not found return "def".
 */
const char *nano_conf_get(conf_t conf, const char **path, const char *def)
{
   nano_trace("searching for path:");
   for(int i = 0; path[i]; i++)
      nano_trace("      %s [sp %d]", path[i], i);
   
   // Patterns are sorted by specifity (see nano_conf_put()), so searching
   // is rather simple.
   for(struct node *i = conf->first; i; i = i->next) {
      nano_trace("   testing pattern:");
      for(int j = 0; i->pattern[j]; j++)
         nano_trace("         %s [tp %d]",
                    (i->pattern[j] == NULL ? "<end>" :
                     (i->pattern[j] == CONF_WILD_ONE ? "<?>" :
                      i->pattern[j] == CONF_WILD_ANY ? "<*>" : i->pattern[j])),
                    j);
      
      if(match(i->pattern, path)) {
         nano_trace("   => matches");
         return i->value;
      } else
         nano_trace("   => does not match");
   }

   return def;         
}

/*
 * Wrapper, which is (better: will once be) more robust than atoi().
 */
int nano_parse_int(const char *s)
{
   // TODO error handling
   return atoi(s);
}

/*
 * Wrapper, which is (better: will once be) more robust than atof().
 */
double nano_parse_double(const char *s)
{
   // TODO error handling
   return atof(s);
}
