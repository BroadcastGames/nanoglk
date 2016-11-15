# ----- BEGINNING OF CONFIGURATION SECTION -----

# The directory where the terps can be found. If you use another
# version of gargoyle, or something different than gargoyle, some more
# changes below may be necessary.

TERPS_DIR = ../gargoyle-2011.1-sources/terps

# Logging. In short:
#
# - LOG_FILE for logging into a file;
# - LOG_STD to print to stdout;
# - LOG_TRACE to log also trace messages;
# - LOG_GLK to print Glk log messages (via nanoglk_log()).
#
# More in README and misc/misc.c.

#LOG = -DLOG_FILE -DLOG_STD -DLOG_TRACE -DLOG_GLK

# Compilation for the Ben NanoNote. This may become a bit tricky. This
# configuration depends on some symbolic links, so that it is
# independant of the version.
#
# When in doubt, make sure  that these files (among others) exist (use
# find(1)):
#
# - $(ROOT_DIR_NN)/usr/include/SDL/SDL.h,
# - $(ROOT_DIR_NN)/usr/lib/libSDL.a
#
# and, of course,
#
# - $(CC_NN).

STAGING_DIR_NN = $(HOME)/openwrt-xburst/staging_dir
ROOT_DIR_NN = $(STAGING_DIR_NN)/target
CC_NN = $(STAGING_DIR_NN)/toolchain/bin/mipsel-openwrt-linux-uclibc-gcc

# Options for scp, to copy the results onto the Ben.

# I use this to prevent errors after reflashing the Ben.
NN_SCP_OPTS = -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no
NN_USER = root
NN_HOST = 192.168.254.101

# ----- END OF CONFIGURATION SECTION (hopefully) -----

VERSION = 0.0.1

DIST_FILES = \
   COPYING Makefile README test.cfg gargoyle.diff `find -name "*.[ch]"`

DIST_DIR = nanoglk-$(VERSION)

FROTZ_DIR = $(TERPS_DIR)/frotz

FROTZ_PARTS = $(FROTZ_DIR)/buffer.o $(FROTZ_DIR)/err.o			\
   $(FROTZ_DIR)/fastmem.o $(FROTZ_DIR)/files.o $(FROTZ_DIR)/glkmisc.o	\
   $(FROTZ_DIR)/glkscreen.o $(FROTZ_DIR)/input.o $(FROTZ_DIR)/main.o	\
   $(FROTZ_DIR)/math.o $(FROTZ_DIR)/object.o $(FROTZ_DIR)/process.o	\
   $(FROTZ_DIR)/quetzal.o $(FROTZ_DIR)/random.o				\
   $(FROTZ_DIR)/redirect.o $(FROTZ_DIR)/sound.o $(FROTZ_DIR)/stream.o	\
   $(FROTZ_DIR)/table.o $(FROTZ_DIR)/text.o $(FROTZ_DIR)/variable.o

GLULXE_DIR = $(TERPS_DIR)/glulxe

GLULXE_PARTS = $(GLULXE_DIR)/accel.o $(GLULXE_DIR)/exec.o		\
   $(GLULXE_DIR)/files.o $(GLULXE_DIR)/float.o $(GLULXE_DIR)/funcs.o	\
   $(GLULXE_DIR)/gestalt.o $(GLULXE_DIR)/glkop.o $(GLULXE_DIR)/heap.o	\
   $(GLULXE_DIR)/main.o $(GLULXE_DIR)/operand.o				\
   $(GLULXE_DIR)/osdepend.o $(GLULXE_DIR)/profile.o			\
   $(GLULXE_DIR)/search.o $(GLULXE_DIR)/serial.o			\
   $(GLULXE_DIR)/string.o $(GLULXE_DIR)/unixstrt.o $(GLULXE_DIR)/vm.o

GIT_DIR = $(TERPS_DIR)/git

GIT_PARTS = $(GIT_DIR)/accel.o $(GIT_DIR)/compiler.o			\
    $(GIT_DIR)/gestalt.o $(GIT_DIR)/git.o $(GIT_DIR)/git_unix.o		\
    $(GIT_DIR)/glkop.o $(GIT_DIR)/heap.o $(GIT_DIR)/memory.o		\
    $(GIT_DIR)/opcodes.o $(GIT_DIR)/operands.o $(GIT_DIR)/peephole.o	\
    $(GIT_DIR)/savefile.o $(GIT_DIR)/saveundo.o $(GIT_DIR)/search.o	\
    $(GIT_DIR)/terp.o

TERPS = nanofrotz nanoglulxe nanogit

# These are some test programs, which should not be installed
# anywhere.
TESTS = nanotest-filesel nanotest-styles nanotest-windows1	\
   nanotest-imgtest nanotest-conftest nanotest-misctest

PROGRAMS = $(TERPS) $(TESTS)

# The pars of the "misc" subset of nanoglk.
MISC_PARTS = misc/misc.o misc/string.o misc/ui.o misc/filesel.o	\
   misc/conf.o

# All pars of nanoglk, including "misc", as well as the blorb and the
# dispatching layer.
NANOGLK_PARTS = nanoglk/main.o nanoglk/event.o nanoglk/window.o		\
   nanoglk/wintextbuffer.o nanoglk/wintextgrid.o			\
   nanoglk/wingraphics.o nanoglk/stream.o nanoglk/sound.o		\
   nanoglk/fileref.o nanoglk/image.o nanoglk/dispatch.o			\
   nanoglk/blorb.o nanoglk/unsorted.o $(MISC_PARTS) glk/gi_blorb.o	\
   glk/gi_dispa.o

ALL_PARTS = $(NANOGLK_PARTS) $(FROTZ_PARTS) $(GLULXE_PARTS) $(GIT_PARTS)

# This works so far for all. May be differiencated in the future.
CFLAGS_ALL = -Wall -std=c99 -DZTERP_GLK -DGLK -DOS_UNIX $(LOG)
NANOGLK_LIBS_ALL = -lSDL -lSDL_ttf -lSDL_image

CFLAGS = $(CFLAGS_ALL) -g
INCLUDES = -I. -Iglk
NANOGLK_LIBS = $(NANOGLK_LIBS_ALL) -lduma

# If the goal is "copy-nn", switch to NanoNote options.
ifeq ($(MAKECMDGOALS), copy-nn)
   ROOT_DIR = $(ROOT_DIR_NN)
   CC = $(CC_NN)

   INCLUDES = -I. -Iglk -I$(ROOT_DIR)/usr/include
   CFLAGS = -DNANONOTE $(CFLAGS_ALL)
   NANOGLK_LIBS = $(NANOGLK_LIBS_ALL) -L$(ROOT_DIR)/lib -L$(ROOT_DIR)/usr/lib
endif

LDFLAGS = -Wl,-rpath-link=$(ROOT_DIR)/usr/lib

.c.o:
	$(RM) $@
	$(CC) -c $(CFLAGS) $(INCLUDES) -o $*.o $*.c

all: $(PROGRAMS)

nanofrotz: $(NANOGLK_PARTS) $(FROTZ_PARTS)
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanofrotz $(NANOGLK_PARTS) $(FROTZ_PARTS)

nanoglulxe: $(NANOGLK_PARTS) $(GLULXE_PARTS)
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanoglulxe $(NANOGLK_PARTS) $(GLULXE_PARTS)

nanogit: $(NANOGLK_PARTS) $(GIT_PARTS)
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanogit $(NANOGLK_PARTS) $(GIT_PARTS)

nanotest-filesel: $(NANOGLK_PARTS) test/test-filesel.o
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanotest-filesel $(NANOGLK_PARTS) test/test-filesel.o

nanotest-styles: $(NANOGLK_PARTS) test/test-styles.o
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanotest-styles $(NANOGLK_PARTS) test/test-styles.o

nanotest-windows1: $(NANOGLK_PARTS) test/test-windows1.o
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanotest-windows1 $(NANOGLK_PARTS) test/test-windows1.o

nanotest-imgtest: $(MISC_PARTS) test/imgtest.o
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanotest-imgtest $(MISC_PARTS) test/imgtest.o

nanotest-conftest: $(MISC_PARTS) test/conftest.o
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanotest-conftest $(MISC_PARTS) test/conftest.o

nanotest-misctest: $(MISC_PARTS) test/misctest.o
	$(CC) $(LDFLAGS) $(NANOGLK_LIBS) -o nanotest-misctest $(MISC_PARTS) test/misctest.o

clean:
	rm -f $(ALL_PARTS) test/*.o  $(PROGRAMS)

install: $(TERPS)
	cp $(TERPS) /usr/local/bin/

copy-nn: $(TERPS)
	scp $(NN_SCP_OPTS) $(TERPS) $(NN_USER)@$(NN_HOST):/usr/bin/

dist:
	mkdir $(DIST_DIR)
	(for f in $(DIST_FILES); do dirname $$f; done) | sort | uniq | \
	   grep -v '^\.$$' | sed 's/^/$(DIST_DIR)\//' | xargs mkdir
	for f in $(DIST_FILES); do cp $$f $(DIST_DIR)/$$f; done
	tar cfz $(DIST_DIR).tar.gz $(DIST_DIR)
	rm -rf $(DIST_DIR)
