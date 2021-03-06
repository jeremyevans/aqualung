SUBDIRS = decoder encoder img

AM_CPPFLAGS = -DAQUALUNG_DATADIR=\"$(pkgdatadir)\" -DLOCALEDIR=\"$(localedir)\"
AM_LDFLAGS = $(debug_LDFLAGS)
LDADD = $(LIBINTL) $(debug_LIBS)

bin_PROGRAMS = aqualung

aqualung_CPPFLAGS = $(AM_CPPFLAGS) -DAQUALUNG_SKINDIR=\"$(pkgdatadir)/skin\"

aqualung_CFLAGS = $(xml_CFLAGS) $(gtk_CFLAGS) $(glib_CFLAGS) \
                  $(alsa_CFLAGS) $(pulse_CFLAGS) \
                  $(lavc_CFLAGS) $(mad_CFLAGS) $(sndfile_CFLAGS) \
                  $(speex_CFLAGS) $(vorbis_CFLAGS) $(vorbisenc_CFLAGS) \
                  $(wavpack_CFLAGS) \
                  $(lrdf_CFLAGS) $(src_CFLAGS) \
                  $(cdio_CFLAGS) \
                  $(cddb_CFLAGS) $(lua_CFLAGS)

aqualung_LDADD = decoder/libdecoder.a encoder/libencoder.a \
                 $(LDADD) $(xml_LIBS) $(gtk_LIBS) $(glib_LIBS) \
                 $(alsa_LIBS) $(jack_LIBS) $(oss_LIBS) $(pulse_LIBS) \
                 $(sndio_LIBS) $(winmm_LIBS) \
                 $(flac_LIBS) $(lame_LIBS) $(lavc_LIBS) $(mac_LIBS) \
                 $(mad_LIBS) $(mod_LIBS) $(mpc_LIBS) $(sndfile_LIBS) \
                 $(speex_LIBS) $(vorbis_LIBS) $(vorbisenc_LIBS) \
                 $(wavpack_LIBS) \
                 $(lrdf_LIBS) $(src_LIBS) \
                 $(cdio_LIBS) $(ifp_LIBS) \
                 $(cddb_LIBS) $(lua_LIBS)

aqualung_SOURCES = \
about.h about.c \
athread.h athread.c \
build_store.h build_store.c \
common.h \
core.h core.c \
cover.h cover.c \
ext_lua.h ext_lua.c \
file_info.h file_info.c \
gui_main.h gui_main.c \
httpc.h httpc.c \
i18n.h \
metadata.h metadata.c \
metadata_api.h metadata_api.c \
metadata_ape.h metadata_ape.c \
metadata_id3v1.h metadata_id3v1.c \
metadata_id3v2.h metadata_id3v2.c \
metadata_ogg.h metadata_ogg.c \
music_browser.h music_browser.c \
options.h options.c \
playlist.h playlist.c \
rb.h rb.c \
search.h search.c \
search_playlist.h search_playlist.c \
segv.h segv.c \
skin.h skin.c \
store_file.h store_file.c \
transceiver.c transceiver.h \
trashlist.c trashlist.h \
utils.c utils.h \
utils_gui.c utils_gui.h \
utils_xml.h utils_xml.c \
version.h \
volume.c volume.h

if HAVE_CDDA
aqualung_SOURCES += cdda.h cdda.c store_cdda.h store_cdda.c
if HAVE_TRANSCODING
aqualung_SOURCES += cd_ripper.h cd_ripper.c
endif
endif

if HAVE_CDDB
aqualung_SOURCES += cddb_lookup.h cddb_lookup.c
endif

if HAVE_FLAC
aqualung_SOURCES += metadata_flac.h metadata_flac.c
endif

if HAVE_IFP
aqualung_SOURCES += ifp_device.h ifp_device.c
endif

if HAVE_JACK_MGMT
aqualung_SOURCES += ports.h ports.c
endif

if HAVE_LADSPA
aqualung_SOURCES += plugin.h plugin.c
endif

if HAVE_LOOP
aqualung_SOURCES += loop_bar.h loop_bar.c
endif

if HAVE_PODCAST
aqualung_SOURCES += podcast.h podcast.c store_podcast.h store_podcast.c
endif

if HAVE_TRANSCODING
aqualung_SOURCES += export.h export.c
endif
