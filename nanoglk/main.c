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
 * Initialization and some other stuff, which does not fit anywhere else.
 */

#include "nanoglk.h"
#include "glkstart.h"

#include <sys/types.h>
#include <unistd.h>

/*
 * For all styles, and for all relevant window types (buffer and
 * grid), the respective fonts.
 */
struct nanoglk_font *nanoglk_buffer_font[style_NUMSTYLES];
struct nanoglk_font *nanoglk_grid_font[style_NUMSTYLES];

/*
 * General font for UIs (currently only file selection). The actual
 * font is used everywhere, but the foreground and background are only
 * used for dialogs (typically black on light gray.
 */
struct nanoglk_font *nanoglk_ui_font;

/*
 * Further foreground and background colors for the file selection
 * (and, in the future, perhaps used elsewhere): input field, list
 * background ("i"nactive), list selection ("a"ctive). Compare to
 * configuration tree.
 */
SDL_Color nanoglk_ui_input_fg_color, nanoglk_ui_input_bg_color;
SDL_Color nanoglk_ui_list_i_fg_color, nanoglk_ui_list_i_bg_color;
SDL_Color nanoglk_ui_list_a_fg_color, nanoglk_ui_list_a_bg_color;

/* size of the screen (compare to configuration tree) */
int nanoglk_screen_width, nanoglk_screen_height, nanoglk_screen_depth;

/* size of file selection (compare to configuration tree) */
int nanoglk_filesel_width, nanoglk_filesel_height;

/* factors for window sizes (compare to configuration tree) */
double nanoglk_factor_horizontal_fixed, nanoglk_factor_vertical_fixed;
double nanoglk_factor_horizontal_proportional;
double nanoglk_factor_vertical_proportional;

static void log_line(void);
static void init_properties(void);

static char *binname; // basename of argv[0], used for configuration
static conf_t conf;   // the nanoglk configuration

// The standard configuration, which provides basicly useful values when
// no configuration file is found.
static const char *std_conf[] = {
   "*.font-path = "
#ifdef NANONOTE
   "/usr/share/fonts/ttf-dejavu",
#else
   "/usr/share/fonts/truetype/ttf-dejavu",
#endif
   
   "?.buffer.?.font-family = DejaVuSerif",
   "?.buffer.?.font-size = 12",
   "?.buffer.preformatted.font-family = DejaVuSansMono",
   "?.buffer.preformatted.font-size = 9",
   
   "?.grid.?.font-family = DejaVuSansMono",
   "?.grid.?.font-size = 9",
   
   "?.?.emphasized.font-style = italics",
   "?.?.header.font-weight = bold",
   "?.?.subheader.font-weight = bold",
   "?.?.subheader.font-style = italics",
   "?.?.alert.font-weight = bold",
   "?.?.alert.foreground = 800000",
   "?.?.input.font-weight = bold",
   "?.?.input.foreground = 008000",

   "?.screen.width = "
#ifdef NANONOTE
   "320",
#else
   "640",
#endif
   "?.screen.height = "
#ifdef NANONOTE
   "240",
#else
   "480",
#endif
   "?.screen.depth = 24",
   
   "?.ui.font-family = DejaVuSans",
   "?.ui.font-size = 12",
   "?.ui.dialog.foreground = 000000",
   "?.ui.dialog.background = c0c0c0",
   "?.ui.input.foreground = 000000",
   "?.ui.input.background = ffffff",
   "?.ui.list.inactive.foreground = 000000",
   "?.ui.list.inactive.background = ffffff",
   "?.ui.list.active.foreground = ffffff",
   "?.ui.list.active.background = 0000c0",
   
   "!include /etc/nanoglkrc",
   "!include ${HOME}/.nanoglkrc",

   "?.ui.file-selection.width = "
#ifdef NANONOTE
   "310",
#else
   "560",
#endif
   "?.ui.file-selection.height = "
#ifdef NANONOTE
   "230"
#else
   "400"
#endif
};

int main(int argc, char *argv[])
{
   nanoglk_log("main: ctrl-alt-q will quit the app");

   nano_init(argc, argv, TRUE);
   nanoglk_log("main: after non_init");
   printf("did nanglk_log just say... main: after non_init\n");
   nano_register_key('q', glk_exit);
   nano_register_key('l', log_line);

   char *copy = strdup(argv[0]);
   binname = strdup(basename(copy));
   free(copy);

   conf = nano_conf_init();

   // Read internal configuration.
   for(int i = 0; i < sizeof(std_conf) / sizeof(std_conf[0]); i++)
      nano_conf_read_line(conf, std_conf[i], "<internal>", i + 1);

   // Initialise SDL.
   if(SDL_Init(SDL_INIT_VIDEO) != 0) {
      printf("Unable to initialize SDL: %s\n", SDL_GetError());
      return 1;
   }

   atexit(SDL_Quit);
   
#ifdef NANONOTE
   // The NanoNote does not have a pointing device, so a cursor would be
   // annoying.
   SDL_ShowCursor(SDL_DISABLE);
#endif

   TTF_Init();

   init_properties();
   nanoglk_window_init(nanoglk_screen_width, nanoglk_screen_height,
                       nanoglk_screen_depth);

   glkunix_startup_t startdata = { argc, argv };
   if(glkunix_startup_code(&startdata))
      glk_main();

   glk_exit();
   nanoglk_log("after glk_exit()");
   
   // compiler warnings
   return 0;
}

void glk_exit(void)
{
   nanoglk_log("glk_exit()");

   // SDL_Quit is called automatically.
   nano_conf_free(conf);
   free(binname);

   exit(0);
}

void glk_set_interrupt_handler(void (*func)(void))
{
   nanoglk_log("glk_set_interrupt_handler(%p)", func);
   // TODO
}

void glk_tick(void)
{
   nanoglk_log("glk_tick()");
}

// Called when the user presses Alt+Ctrl+L. Useful when logs are printed to a
// file.
static void log_line(void)
{
   nano_info("-----------------------------------------------------------------"
             "--------------");
}

/*
 * Create a new font wrapper, using data mainly from the configuration.
 */
static struct nanoglk_font *new_font(const char *path, const char *family,
                                     const char *weight, const char *style,
                                     const char *size,
                                     const char *fg, const char *bg)
{
   struct nanoglk_font *font =
      (struct nanoglk_font*)nano_malloc(sizeof(struct nanoglk_font));

   font->font = nano_load_font_str(path, family, weight, style, size);
   nano_parse_color(fg, &font->fg);
   nano_parse_color(bg, &font->bg);

   // Determine dimensions by rendering.
   SDL_Surface* t = TTF_RenderText_Shaded(font->font, " ", font->fg, font->bg);
   font->space_width = t->w;
   font->text_height = t->h;
   SDL_FreeSurface(t);

   return font;
}

// Read values from the configuration and store it in variables (which are
// actually then not variable anymore).
void init_properties(void)
{
   char *window_type[2] = { "buffer", "grid" };
   int styles[style_NUMSTYLES] = {
      style_Normal, style_Emphasized, style_Preformatted, style_Header,
      style_Subheader, style_Alert, style_Note, style_BlockQuote, style_Input,
      style_User1, style_User2 };
   char *style_name[style_NUMSTYLES] = {
      "normal", "emphasized", "preformatted", "header", "subheader", "alert",
      "note", "blockquote", "input", "user1", "user2" };

   for(int i = 0; i < 2; i++)
      for(int j = 0; j < style_NUMSTYLES; j++) {
         const char *path_path[] = {
            binname, window_type[i], style_name[j], "font-path", NULL };
         const char *path_family[] = {
            binname, window_type[i], style_name[j], "font-family", NULL };
         const char *path_weight[] = {
            binname, window_type[i], style_name[j], "font-weight", NULL };
         const char *path_style[] = {
            binname, window_type[i], style_name[j], "font-style", NULL };
         const char *path_size[] = {
            binname, window_type[i], style_name[j], "font-size", NULL };
         const char *path_bg[] = {
            binname, window_type[i], style_name[j], "background", NULL };
         const char *path_fg[] = {
            binname, window_type[i], style_name[j], "foreground", NULL };

         const char *path = nano_conf_get(conf, path_path, "");
         const char *family = nano_conf_get(conf, path_family, "");
         const char *weight = nano_conf_get(conf, path_weight, "normal");
         const char *style = nano_conf_get(conf, path_style, "normal");
         const char *size = nano_conf_get(conf, path_size, "");
         const char *fg = nano_conf_get(conf, path_fg, "000000");
         const char *bg = nano_conf_get(conf, path_bg, "ffffff");

         switch(i) {
         case 0:
            nanoglk_buffer_font[styles[j]] =
               new_font(path, family, weight, style, size, fg, bg);
            break;

         case 1:
            nanoglk_grid_font[styles[j]] =
               new_font(path, family, weight, style, size, fg, bg);
            break;
         }
      }

   const char *path_path[] = {  binname, "ui", "font-path", NULL };
   const char *path_family[] = { binname, "ui", "font-family", NULL };
   const char *path_weight[] = { binname, "ui", "font-weight", NULL };
   const char *path_style[] = { binname, "ui", "font-style", NULL };
   const char *path_size[] = { binname, "ui", "font-size", NULL };
   const char *path_dlg_bg[] = { binname, "ui", "dialog", "background", NULL };
   const char *path_dlg_fg[] = { binname, "ui", "dialog", "foreground", NULL };

   const char *path = nano_conf_get(conf, path_path, "");
   const char *family = nano_conf_get(conf, path_family, "");
   const char *weight = nano_conf_get(conf, path_weight, "normal");
   const char *style = nano_conf_get(conf, path_style, "normal");
   const char *size = nano_conf_get(conf, path_size, "");
   const char *fg = nano_conf_get(conf, path_dlg_fg, "000000");
   const char *bg = nano_conf_get(conf, path_dlg_bg, "ffffff");
   
   nanoglk_ui_font = new_font(path, family, weight, style, size, fg, bg);

   const char *path_input_fg[] = { binname, "ui", "input", "foreground", NULL };
   const char *path_input_bg[] = { binname, "ui", "input", "background", NULL };
   const char *path_list_a_fg[] = {
      binname, "ui", "list", "active", "foreground", NULL };
   const char *path_list_a_bg[] = {
      binname, "ui", "list", "active", "background", NULL };
   const char *path_list_i_fg[] = {
      binname, "ui", "list", "inactive", "foreground", NULL };
   const char *path_list_i_bg[] = {
      binname, "ui", "list", "inactive", "background", NULL };

   nano_parse_color(nano_conf_get(conf, path_input_fg, "000000"),
                    &nanoglk_ui_input_fg_color);
   nano_parse_color(nano_conf_get(conf, path_input_bg, "ffffff"),
                    &nanoglk_ui_input_bg_color);
   nano_parse_color(nano_conf_get(conf, path_list_a_fg, "ffffff"),
                    &nanoglk_ui_list_a_fg_color);
   nano_parse_color(nano_conf_get(conf, path_list_a_bg, "000000"),
                    &nanoglk_ui_list_a_bg_color);
   nano_parse_color(nano_conf_get(conf, path_list_i_fg, "000000"),
                    &nanoglk_ui_list_i_fg_color);
   nano_parse_color(nano_conf_get(conf, path_list_i_bg, "ffffff"),
                    &nanoglk_ui_list_i_bg_color);

   const char *path_swidth[] = { binname, "screen", "width", NULL };
   const char *path_sheight[] = { binname, "screen", "height", NULL };
   const char *path_sdepth[] = { binname, "screen", "depth", NULL };
   const char *path_fwidth[] = {
      binname, "ui", "file-selection", "width", NULL };
   const char *path_fheight[] = {
      binname, "ui", "file-selection", "height", NULL };

   nanoglk_screen_width = nano_parse_int(nano_conf_get(conf, path_swidth, ""));
   nanoglk_screen_height =
      nano_parse_int(nano_conf_get(conf, path_sheight, ""));
   nanoglk_screen_depth = nano_parse_int(nano_conf_get(conf, path_sdepth, ""));
   nanoglk_filesel_width = nano_parse_int(nano_conf_get(conf, path_fwidth, ""));
   nanoglk_filesel_height =
      nano_parse_int(nano_conf_get(conf, path_fheight, ""));

   // Check for maximal size of the file selection window.
   // TODO Check also for a minimal size?
   if(nanoglk_filesel_width > nanoglk_screen_width) {
      nano_warn("%s.ui.file-selection.width should not be greater than "
                "%s.screen.width", binname, binname);
      nanoglk_filesel_width = nanoglk_screen_width;
   }
   if(nanoglk_filesel_height > nanoglk_screen_height) {
      nano_warn("%s.ui.file-selection.height should not be greater than "
                "%s.screen.height", binname, binname);
      nanoglk_filesel_height = nanoglk_screen_height;
   }

   const char *path_fhf[] =
      { binname, "window-size-factor", "horizontal", "fixed", NULL };
   const char *path_fvf[] =
      { binname, "window-size-factor", "vertical", "fixed", NULL };
   const char *path_fhp[] =
      { binname, "window-size-factor", "horizontal", "proportional", NULL };
   const char *path_fvp[] =
      { binname, "window-size-factor", "vertical", "proportional", NULL };

   nanoglk_factor_horizontal_fixed =
      nano_parse_double(nano_conf_get(conf, path_fhf, "1"));
   nanoglk_factor_vertical_fixed =
      nano_parse_double(nano_conf_get(conf, path_fvf, "1"));
   nanoglk_factor_horizontal_proportional
      = nano_parse_double(nano_conf_get(conf, path_fhp, "1"));
   nanoglk_factor_vertical_proportional
      = nano_parse_double(nano_conf_get(conf, path_fvp, "1"));
}

/*
 * Log a Glk call. See README.
 */
void nanoglk_log(const char *fmt, ...)
{
#ifdef LOG_GLK
   FILE *log = nano_logfile();
   if(log) {
      fprintf(log, "GLK: ");

      va_list argp;
      va_start(argp, fmt);
      vfprintf(log, fmt, argp);
      va_end(argp);
      
      fprintf(log, "\n");
      fflush(log);
   }
#endif
}
