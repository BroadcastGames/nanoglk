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
 * Single header file for everything in the "misc" directory.
 */

#ifndef __MISC_H__

// TODO: Somehow, strdup() and basename() are not defined otherwise.
// (Are these non-standard?)
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

// compatibly with other compilers than GCC
#ifndef __GNUC__
#  define  __attribute__(x)  /* nothing */
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MIN3(a, b, c) MIN(a, MIN(b, c))
#define MAX3(a, b, c) MAX(a, MAX(b, c))

#ifndef FALSE
#define    FALSE (0)
#endif

#ifndef TRUE
#define    TRUE  (1)
#endif

#define strlen16 nano_strlen16
#define strcpy16 nano_strcpy16
#define strcat16 nano_strcat16
#define strdup16 nano_strdup16
#define strcmp16 nano_strcmp16
#define strschr6 nano_strchr16
#define strrchr16 nano_strrchr16
#define strdup16fromutf8 nano_strdup16fromutf8
#define strduputf8from16 nano_strduputf8from16

void nano_init(int argc, char *argv[], int allow_suspend);

FILE *nano_logfile(void);
void nano_trace(const char *fmt, ...)  __attribute__((format(printf, 1, 2)));
void nano_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void nano_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void nano_fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void nano_warnunless(int b, const char *fmt, ...)
   __attribute__((format(printf, 2, 3)));
void nano_failunless(int b, const char *fmt, ...)
   __attribute__((format(printf, 2, 3)));

void *nano_malloc(size_t size);

int nano_strlen16(const Uint16 *text);
void nano_strcpy16(Uint16 *dest, const Uint16 *src);
void nano_strcat16(Uint16 *dest, const Uint16 *src);
Uint16 *nano_strdup16(const Uint16 *src);
int nano_strcmp16(const Uint16 *s1, const Uint16 *s2);
Uint16 *nano_strchr16(const Uint16 *s, Uint16 c);
Uint16 *nano_strrchr16(const Uint16 *s, Uint16 c);

Uint16 *nano_strdup16fromutf8(const char *src);
char *nano_strduputf8from16(const Uint16 *src);

void nano_expand_env(const char *src, char *dest, int maxlen);

typedef struct conf *conf_t;

#define CONF_WILD_ONE ((char*)0x01)
#define CONF_WILD_ANY ((char*)0x02)

conf_t nano_conf_init(void);
void nano_conf_read(conf_t conf, const char *filename);
void nano_conf_read_line(conf_t conf, const char *line,
                         const char *filename, int lineno);
void nano_conf_free(conf_t conf);
void nano_conf_put(conf_t conf, const char **pattern, const char *value);
const char *nano_conf_get(conf_t conf, const char **path, const char *def);

void nano_register_key(char key, void (*func)(void));
void nano_wait_event(SDL_Event *event);
void nano_reg_surface(SDL_Surface **surface);
void nano_unreg_surface(SDL_Surface **surface);
void nano_save_window(SDL_Surface *surface, int x, int y, int w, int h);
void nano_restore_window(SDL_Surface *surface);

#define ITALICS 1
#define OBLIQUE 2

SDL_Surface *nano_scale_surface(SDL_Surface *surface,
                                Uint16 width, Uint16 height);
TTF_Font *nano_load_font(const char *path, const char *family,
                         int weight, int style, int size);
TTF_Font *nano_load_font_str(const char *path, const char *family,
                             const char *weight, const char *style,
                             const char *size);
int nano_parse_int(const char *s);
double nano_parse_double(const char *s);
void nano_parse_color(const char *s, SDL_Color *c);
void nano_fill_rect(SDL_Surface *surface, SDL_Color c,
                    int x, int y, int w, int h);
void nano_fill_3d_inset(SDL_Surface *surface, SDL_Color bg,
                        int x, int y, int w, int h);
void nano_fill_3d_outset(SDL_Surface *surface, SDL_Color bg,
                         int x, int y, int w, int h);

void nano_show_message(SDL_Surface *surface, Uint16 **msg,
                       SDL_Color dfg, SDL_Color dbg, TTF_Font *font);
int nano_ask_yes_no(SDL_Surface *surface, Uint16 **msg, int default_answer,
                    SDL_Color dfg, SDL_Color dbg, TTF_Font *font);
void nano_input_text16(SDL_Surface *surface, SDL_Event *event,
                       Uint16 *text, int max_len, int max_char,
                       int x, int y, int w, int h, TTF_Font *font,
                       SDL_Color fg, SDL_Color bg, int *state);
char *nano_input_file(char *path, Uint16 *title, SDL_Surface *surface,
                      TTF_Font *font, int line_height,
                      SDL_Color dfg, SDL_Color dbg,
                      SDL_Color lifg, SDL_Color libg,
                      SDL_Color lafg, SDL_Color labg,
                      SDL_Color ifg, SDL_Color ibg,
                      int x, int y, int w, int h, int must_exist,
                      int warn_replace, int warn_modify, int warn_append);

#endif // __MISC_H__
