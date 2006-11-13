SUBDIRS = decoder img po

bin_PROGRAMS = aqualung

aqualung_SOURCES = \
about.h about.c \
build_store.h build_store.c \
common.h \
cddb_lookup.h cddb_lookup.c \
core.h core.c \
cover.h cover.c \
file_info.h file_info.cpp \
gui_main.h gui_main.c \
i18n.h \
ifp_device.h ifp_device.c \
meta_decoder.h meta_decoder.cpp \
music_browser.h music_browser.c \
options.h options.c \
playlist.h playlist.c \
plugin.h plugin.c \
ports.h ports.c \
rb.h rb.c \
search.h search.c \
search_playlist.h search_playlist.c \
segv.h segv.c \
skin.h skin.c \
transceiver.c transceiver.h \
trashlist.c trashlist.h \
version.h \
volume.c volume.h

all-local:
	if test -e version.h; then svn_v1="`cat version.h`"; else svn_v1=""; fi; \
	if test -e .svn; then svn_v2="#define AQUALUNG_VERSION \"R-`cd ..; svn info | grep '^Revision' | awk '{print $$2}'`\""; else svn_v2="$$svn_v1"; fi; \
	if test "$$svn_v1" != "$$svn_v2"; then echo "$$svn_v2" > version.h; fi;
