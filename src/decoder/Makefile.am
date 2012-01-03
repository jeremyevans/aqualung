noinst_LIBRARIES = libdecoder.a

AM_CFLAGS = $(glib_CFLAGS) \
            $(lavc_CFLAGS) $(mad_CFLAGS) $(sndfile_CFLAGS) $(speex_CFLAGS) \
            $(vorbis_CFLAGS) $(wavpack_CFLAGS)

AM_CXXFLAGS = $(glib_CFLAGS)

libdecoder_a_SOURCES = dec_null.h dec_null.c file_decoder.h file_decoder.c

if HAVE_CDDA
libdecoder_a_SOURCES += dec_cdda.h dec_cdda.c
endif

if HAVE_FLAC
libdecoder_a_SOURCES += dec_flac.h dec_flac.c
endif

if HAVE_LAVC
libdecoder_a_SOURCES += dec_lavc.h dec_lavc.c
endif

if HAVE_MAC
libdecoder_a_SOURCES += dec_mac.h dec_mac.cpp
endif

if HAVE_MOD
libdecoder_a_SOURCES += dec_mod.h dec_mod.c
endif

if HAVE_MPC
libdecoder_a_SOURCES += dec_mpc.h dec_mpc.c
endif

if HAVE_MPEG
libdecoder_a_SOURCES += dec_mpeg.h dec_mpeg.c
endif

if HAVE_SNDFILE
libdecoder_a_SOURCES += dec_sndfile.h dec_sndfile.c
endif

if HAVE_SPEEX
libdecoder_a_SOURCES += dec_speex.h dec_speex.c
endif

if HAVE_VORBIS
libdecoder_a_SOURCES += dec_vorbis.h dec_vorbis.c
endif

if HAVE_WAVPACK
libdecoder_a_SOURCES += dec_wavpack.h dec_wavpack.c
endif
