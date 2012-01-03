noinst_LIBRARIES = libdecoder.a

AM_CFLAGS = $(glib_CFLAGS) \
            $(lavc_CFLAGS) $(wavpack_CFLAGS)

AM_CXXFLAGS = $(glib_CFLAGS)

libdecoder_a_SOURCES = \
dec_cdda.h dec_cdda.c \
dec_flac.h dec_flac.c \
dec_lavc.h dec_lavc.c \
dec_mac.h dec_mac.cpp \
dec_mod.h dec_mod.c \
dec_mpc.h dec_mpc.c \
dec_mpeg.h dec_mpeg.c \
dec_null.h dec_null.c \
dec_sndfile.h dec_sndfile.c \
dec_speex.h dec_speex.c \
dec_vorbis.h dec_vorbis.c \
dec_wavpack.h dec_wavpack.c \
file_decoder.h file_decoder.c
