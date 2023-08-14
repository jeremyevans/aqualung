# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.60])
AC_INIT([aqualung],[git],[https://github.com/jeremyevans/aqualung])
AM_INIT_AUTOMAKE(subdir-objects)
AC_CONFIG_SRCDIR([src/core.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    doc/Makefile
    po/Makefile.in
    skin/Makefile
    skin/dark/Makefile
    skin/default/Makefile
    skin/metal/Makefile
    skin/ocean/Makefile
    skin/plain/Makefile
    skin/woody/Makefile
    skin/no_skin/Makefile
    src/Makefile
])
AC_CANONICAL_HOST


# Checks for programs.
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_CXX
AM_PROG_CC_C_O
AM_PROG_AR
m4_ifndef([AM_GNU_GETTEXT],
    [m4_fatal([GNU gettext is required to prepare the Aqualung build])])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])
m4_ifndef([PKG_PROG_PKG_CONFIG],
    [m4_fatal([pkg-config is required to prepare the Aqualung build])])
PKG_PROG_PKG_CONFIG


# Checks for libraries.
# opportunistic
AC_CHECK_LIB([z], [gzopen])
AC_CHECK_LIB([bz2], [BZ2_bzopen])
# required
AC_SEARCH_LIBS([floor], [m], [],
    [AC_MSG_ERROR([POSIX math.h functions are required to build Aqualung])])
PKG_CHECK_MODULES([xml], [libxml-2.0])
PKG_CHECK_MODULES([glib], [glib-2.0 >= 2.14 gthread-2.0])
PKG_CHECK_MODULES([gtk], [gtk+-2.0 >= 2.18])


# Checks for header files.
AC_CHECK_HEADERS([dlfcn.h errno.h fcntl.h sys/ioctl.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T


# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset mkdir psiginfo strcasestr strdup strndup strrchr strstr])


# Platform-specific tweaks.
AC_SYS_LARGEFILE
AS_CASE([$host_os],
    [cygwin*],
        [AS_VAR_IF([GCC], [yes], [CPPFLAGS="-mwin32 $CPPFLAGS"])],
    [AC_CHECK_LIB([pthread], [pthread_create], [],
        [AC_MSG_ERROR([pthreads are required to build Aqualung])])])


dnl
dnl Convenience macros for expressing common autoconf idioms used below.
dnl (These macro definitions themselves do not directly add code to the
dnl configure script.)
dnl
dnl AQUALUNG_DETECT(package, brief-description, [detect-actions])
dnl -----------------------------------------------------------------------
AC_DEFUN([AQUALUNG_DETECT], [
    m4_pushdef([_aqualung_cond], [AS_TR_CPP([HAVE_$1])])[]dnl
    AC_ARG_WITH([$1],
        [AS_HELP_STRING([--with-$1],
            [compile with $2 support (default: detect)])],
        [], [with_$1='detect'])
    AS_VAR_IF([with_$1], [no], [], [
        AC_MSG_NOTICE([$2 Support])
        m4_default([$3], [:])
        AS_CASE([$with_$1],
            [detect], [with_$1='not found'],
            [yes*], [AC_DEFINE(_aqualung_cond, [1],
                        [Define to build with $2 support])])
    ])
    AM_CONDITIONAL(_aqualung_cond, [expr "X$with_$1" : 'Xyes' >/dev/null])
    m4_popdef([_aqualung_cond])[]dnl
])dnl
dnl AQUALUNG_FOUND(package, [package_LIBS-value])
dnl -----------------------------------------------------------------------
AC_DEFUN([AQUALUNG_FOUND], [
    AS_VAR_IF([with_$1], [detect], [with_$1='yes (found)'])
    m4_ifnblank([$2], [AC_SUBST([$1_LIBS], ['$2'])])
])dnl
dnl AQUALUNG_MISSING(package, error-description)
dnl -----------------------------------------------------------------------
AC_DEFUN([AQUALUNG_MISSING],
    [AS_VAR_IF([with_$1], [yes], [AC_MSG_ERROR([$2])])])dnl


#
# Feature Options
#
AC_MSG_CHECKING([for type of build])
buildtype='release'
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug], [compile with debugging support])],
    [AS_VAR_IF([enable_debug], [yes], [buildtype='debug'])])
AC_MSG_RESULT([$buildtype])
AS_VAR_IF([buildtype], [release],
    [AC_DEFINE([RELEASE_BUILD], [1], [Define if doing a release build])],
    [
        dnl
        dnl backtracing is fairly non-standard, BSD users will likely have to
        dnl install libexecinfo first
        dnl
        AC_CHECK_FUNC([backtrace_symbols], [],
            [AC_CHECK_LIB([execinfo], [backtrace_symbols],
                [AC_SUBST([debug_LIBS], [-lexecinfo])],
                [AC_MSG_ERROR([debug builds need backtrace support])])])
        dnl
        dnl We must ensure we have frame pointers for backtrace() to work with.
        dnl As such, we must override any previously set optimizations (such as
        dnl those set by the AC_PROG_CC macro), so while setting user variables
        dnl directly is normally considered bad form, it's necessary here.
        dnl
        AS_VAR_IF([GCC], [yes], [
            AC_SUBST([debug_LDFLAGS], [-rdynamic])
            CFLAGS="$CFLAGS -ggdb -g -O0"
        ])
        AS_VAR_IF([GXX], [yes], [CXXFLAGS="$CXXFLAGS -ggdb -g -O0"])
        AC_DEFINE([DEBUG_BUILD], [1], [Define if doing a debug build])
    ])

AC_ARG_ENABLE([jack_mgmt],
    [AS_HELP_STRING([--disable-jack-mgmt],
        [compile without JACK port management support])],
    [], [enable_jack_mgmt='yes'])
# see JACK output driver check below

AC_ARG_ENABLE([loop],
    [AS_HELP_STRING([--disable-loop], [compile without loop playback support])],
    [], [enable_loop='yes'])
AS_VAR_IF([enable_loop], [yes],
    [AC_DEFINE([HAVE_LOOP], [1], [Define to build with loop playback support])])
AM_CONDITIONAL([HAVE_LOOP], [test "x$enable_loop" = 'xyes'])

AC_ARG_ENABLE([podcast],
    [AS_HELP_STRING([--disable-podcast], [compile without podcast support])],
    [], [enable_podcast='yes'])
AS_VAR_IF([enable_podcast], [yes],
    [AC_DEFINE([HAVE_PODCAST], [1], [Define to build with podcast support])])
AM_CONDITIONAL([HAVE_PODCAST], [test "x$enable_podcast" = 'xyes'])

AC_ARG_ENABLE([systray],
    [AS_HELP_STRING([--disable-systray], [compile without systray support])],
    [], [enable_systray='yes'])
AS_VAR_IF([enable_systray], [yes],
    [AC_DEFINE([HAVE_SYSTRAY], [1], [Define to build with GTK+ systray])])

AC_ARG_ENABLE([transcoding],
    [AS_HELP_STRING([--disable-transcoding],
        [compile without transcoding support])],
    [], [enable_transcoding='yes'])
AS_VAR_IF([enable_transcoding], [yes], [], [
    dnl
    dnl --disable-transcoding implies --without-lame --without-vorbisenc
    dnl setting these variables now ensures detection is forced off below
    dnl
    with_lame='no'
    with_vorbisenc='no'
])


#
# Output Driver Library Options
#
AQUALUNG_DETECT([alsa], [ALSA],
    [PKG_CHECK_MODULES([alsa], [alsa],
        [AQUALUNG_FOUND([alsa])],
        [AQUALUNG_MISSING([alsa], [ALSA support requires libasound])])])

AQUALUNG_DETECT([jack], [JACK],
    [AC_CHECK_LIB([jack], [jack_on_info_shutdown],
        [AQUALUNG_FOUND([jack], [-ljack])],
        [AQUALUNG_MISSING([jack], [JACK support requires libjack])])])
AS_CASE([$enable_jack_mgmt/$with_jack],
    [yes/yes*], [AC_DEFINE([HAVE_JACK_MGMT], [1],
                    [Define to build with JACK port management support])],
    [enable_jack_mgmt='no'])
AM_CONDITIONAL([HAVE_JACK_MGMT], [test "x$enable_jack_mgmt" = 'xyes'])

AC_ARG_WITH([oss],
    [AS_HELP_STRING([--with-oss],
        [compile with OSS support (default: detect)])],
    [], [with_oss='detect'])
AS_VAR_IF([with_oss], [no], [], [
    AC_MSG_NOTICE([OSS Support])
    dnl
    dnl OSS detection is a wee bit squirrely, sorry if this hard to follow...
    dnl
    AC_CHECK_HEADERS([sys/soundcard.h soundcard.h linux/soundcard.h],
        [AS_CASE([$with_oss], [*yes], [], [with_oss='yes (found)'])],
        [with_oss="Z$with_oss"])
    AS_CASE([$with_oss],
        [ZZZyes], [AC_MSG_ERROR([OSS support requires soundcard.h])],
        [*found*], [with_oss='yes (found)'],
        [*yes*], [with_oss='yes'],
        [with_oss='not found'])
    AS_CASE([$with_oss], [yes*], [
        AC_CHECK_LIB([ossaudio], [_oss_ioctl],
            [AC_SUBST([oss_LIBS], [-lossaudio])])
        AC_DEFINE([HAVE_OSS], [1], [Define to build with OSS support])
    ])
])
AM_CONDITIONAL([HAVE_OSS], [expr "X$with_oss" : 'Xyes' >/dev/null])

AQUALUNG_DETECT([pulse], [PulseAudio],
    [PKG_CHECK_MODULES([pulse], [libpulse-simple],
        [AQUALUNG_FOUND([pulse])],
        [AQUALUNG_MISSING([pulse], [$pulse_PKG_ERRORS])])])

AQUALUNG_DETECT([sndio], [sndio],
    [AC_CHECK_LIB([sndio], [sio_open],
        [AQUALUNG_FOUND([sndio], [-lsndio])],
        [AQUALUNG_MISSING([sndio], [sndio support requires libsndio])])])

AQUALUNG_DETECT([winmm], [Windows Multimedia Extensions API], [
    AC_MSG_CHECKING([for platform support])
    AS_CASE([$host_os], [cygwin*], [AC_MSG_RESULT([yes])], [
        AC_MSG_RESULT([no])
        AQUALUNG_MISSING([winmm],
            [WinMM is only supported on Microsoft Windows])
        with_winmm="not supported on $host_os"
    ])
    AS_CASE([$with_winmm], [yes|detect],
        [AC_CHECK_HEADER([mmsystem.h],
            [AQUALUNG_FOUND([winmm], [-lwinmm])],
            [AQUALUNG_MISSING([winmm], [WinMM support requires mmsystem.h])],
            [[#include <windows.h>]])])
])


#
# Codec Library Options
#
AQUALUNG_DETECT([flac], [Free Lossless Audio Codec],
    [PKG_CHECK_MODULES([flac], [flac],
        [AQUALUNG_FOUND([flac])],
        [AQUALUNG_MISSING([flac], [$flac_PKG_ERRORS])])])
AS_VAR_IF([enable_transcoding], [yes],
    [with_flac_enc="$with_flac"], [with_flac_enc='no'])
AS_CASE([$with_flac_enc],
    [yes*], [AC_DEFINE([HAVE_FLAC_ENC], [1],
                [Define to build with flac encoding support])])

AQUALUNG_DETECT([lame], [LAME MP3 Encoding],
    [AC_CHECK_LIB([mp3lame], [lame_init],
        [AQUALUNG_FOUND([lame], [-lmp3lame])],
        [AQUALUNG_MISSING([lame], [LAME support requires libmp3lame])])])

AQUALUNG_DETECT([lavc], [Libav / FFmpeg],
    [PKG_CHECK_MODULES([lavc], [libavcodec libavformat libavutil],
        [AQUALUNG_FOUND([lavc])],
        [AQUALUNG_MISSING([lavc], [$lavc_PKG_ERRORS])])])

AQUALUNG_DETECT([MAC], [Monkey's Audio Codec],
    [AC_CHECK_LIB([MAC], [CreateIAPEDecompress],
        [AQUALUNG_FOUND([MAC], [-lMAC -lstdc++])],
        [AQUALUNG_MISSING([MAC],
            [Monkey's Audio Codec support requires libMAC])],
        [-lstdc++])])

AQUALUNG_DETECT([mod], [MOD Audio],
    [PKG_CHECK_MODULES([mod], [libmodplug >= 0.8.4],
        [AQUALUNG_FOUND([mod])],
        [AQUALUNG_MISSING([mod], [$mod_PKG_ERRORS])])])

AQUALUNG_DETECT([mpc], [Musepack], [
    AC_CHECK_LIB([mpcdec], [mpc_demux_init],
        [AQUALUNG_FOUND([mpc], [-lmpcdec])],
        [AC_CHECK_LIB([mpcdec], [mpc_streaminfo_init],
            [
                AQUALUNG_FOUND([mpc], [-lmpcdec])
                AC_DEFINE([MPC_OLD_API], [1],
                    [Defined if old Musepack API is used])
            ],
            [AQUALUNG_MISSING([mpc], [Musepack support requires libmpcdec])])])
])

AQUALUNG_DETECT([mpeg], [MPEG Audio],
    [PKG_CHECK_MODULES([mad], [mad],
        [AQUALUNG_FOUND([mpeg])],
        [AQUALUNG_MISSING([mpeg], [$mad_PKG_ERRORS])])])

AQUALUNG_DETECT([sndfile], [sndfile (WAV, AIFF, etc.)], [
    PKG_CHECK_MODULES([sndfile], [sndfile >= 1.0.17],
        [AQUALUNG_FOUND([sndfile])],
        [AQUALUNG_MISSING([sndfile], [$sndfile_PKG_ERRORS])])
    PKG_CHECK_EXISTS([sndfile >= 1.0.18],
        [AC_DEFINE([HAVE_SNDFILE_1_0_18], [1], [Defined if sndfile >= 1.0.18])])
])
AS_VAR_IF([enable_transcoding], [yes],
    [with_sndfile_enc="$with_sndfile"], [with_sndfile_enc='no'])
AS_CASE([$with_sndfile_enc],
    [yes*], [AC_DEFINE([HAVE_SNDFILE_ENC], [1],
                [Define to build with sndfile encoding support])])

AQUALUNG_DETECT([speex], [Ogg Speex],
    [PKG_CHECK_MODULES([speex], [oggz speex ogg],
        [AQUALUNG_FOUND([speex])],
        [AQUALUNG_MISSING([speex], [$speex_PKG_ERRORS])])])

AQUALUNG_DETECT([vorbis], [Ogg Vorbis],
    [PKG_CHECK_MODULES([vorbis], [vorbis vorbisfile ogg],
        [AQUALUNG_FOUND([vorbis])],
        [AQUALUNG_MISSING([vorbis], [$vorbis_PKG_ERRORS])])])

AQUALUNG_DETECT([vorbisenc], [Ogg Vorbis Encoding],
    [PKG_CHECK_MODULES([vorbisenc], [vorbisenc ogg],
        [AQUALUNG_FOUND([vorbisenc])],
        [AQUALUNG_MISSING([vorbisenc], [$vorbisenc_PKG_ERRORS])])])
AS_CASE([$with_flac_enc/$with_lame/$with_sndfile_enc/$with_vorbisenc],
    [*yes*], [AC_DEFINE([HAVE_TRANSCODING], [1],
                 [Define to build with transcoding support])],
    [enable_transcoding='no'])
AM_CONDITIONAL([HAVE_TRANSCODING], [test "x$enable_transcoding" = 'xyes'])

AQUALUNG_DETECT([wavpack], [WavPack],
    [PKG_CHECK_MODULES([wavpack], [wavpack >= 4.40.0],
        [AQUALUNG_FOUND([wavpack])],
        [AQUALUNG_MISSING([wavpack], [$wavpack_PKG_ERRORS])])])


#
# DSP Library Options
#
AQUALUNG_DETECT([ladspa], [LADSPA Plugin], [
    PKG_CHECK_MODULES([lrdf], [lrdf >= 0.4.0],
        [AQUALUNG_FOUND([ladspa])],
        [AQUALUNG_MISSING([ladspa],
            [LADSPA support requires liblrdf >= 0.4.0])])
    AS_CASE([$with_ladspa], [yes*],
        [AC_CHECK_FUNC([dlopen], [],
            [AC_CHECK_LIB([dl], [dlopen], [lrdf_LIBS="$lrdf_LIBS -ldl"], [
                AQUALUNG_MISSING([ladspa],
                    [LADSPA support requires dynamic loading])
                with_ladspa='no (missing dlopen)'
            ])])])
])

AQUALUNG_DETECT([src], [Sample Rate Converter],
    [PKG_CHECK_MODULES([src], [samplerate],
        [AQUALUNG_FOUND([src])],
        [AQUALUNG_MISSING([src], [$src_PKG_ERRORS])])])


#
# Hardware Library Options
#
AQUALUNG_DETECT([cdda], [Compact Disc Digital Audio],
    [PKG_CHECK_MODULES([cdio], [libcdio >= 0.76 libcdio_paranoia],
        [
            AQUALUNG_FOUND([cdda])
            AC_CHECK_HEADERS([cdio/paranoia/cdda.h cdio/paranoia/paranoia.h])
        ],
        [AQUALUNG_MISSING([cdda], [$cdio_PKG_ERRORS])])])

AQUALUNG_DETECT([ifp], [iRiver iFP],
    [AC_CHECK_LIB([usb], [usb_init],
        [AC_CHECK_LIB([ifp], [ifp_find_device],
            [AQUALUNG_FOUND([ifp], [-lusb -lifp])],
            [AQUALUNG_MISSING([ifp], [iRiver iFP support requires libifp])])],
        [AQUALUNG_MISSING([ifp], [iRiver iFP support requires libusb])])])


#
# Miscellaneous Library Options
#
AQUALUNG_DETECT([cddb], [Compact Disc Database], [
    PKG_CHECK_MODULES([cddb], [libcddb >= 1.2.1],
        [AQUALUNG_FOUND([cddb])],
        [AQUALUNG_MISSING([cddb], [$cddb_PKG_ERRORS])])
    AC_CHECK_LIB([cddb], [cddb_disc_get_revision],
        [AC_DEFINE([LIBCDDB_REVISION], [1],
            [Define if libcddb has cddb_disc_get_revision function])])
])

AQUALUNG_DETECT([lua], [Lua Extension],
    [PKG_CHECK_MODULES([lua], [lua5.2],
        [AQUALUNG_FOUND([lua])],
        [PKG_CHECK_MODULES([lua], [lua-5.2],
            [AQUALUNG_FOUND([lua])],
            [PKG_CHECK_MODULES([lua], [lua52],
                [AQUALUNG_FOUND([lua])],
                [PKG_CHECK_MODULES([lua], [lua5.1],
                    [AQUALUNG_FOUND([lua])],
                    [PKG_CHECK_MODULES([lua], [lua-5.1],
                        [AQUALUNG_FOUND([lua])],
                        [PKG_CHECK_MODULES([lua], [lua51],
                            [AQUALUNG_FOUND([lua])],
                            [PKG_CHECK_MODULES([lua], [lua >= 5.2],
                                [AQUALUNG_FOUND([lua])],
                                    [PKG_CHECK_MODULES([lua], [lua >= 5.1],
                                    [AQUALUNG_FOUND([lua])],
                                [AQUALUNG_MISSING([lua], [$lua_PKG_ERRORS])])])])])])])])])])


# Compiler and linker variables.
AS_VAR_IF([GCC], [yes], [CFLAGS="-Wall $CFLAGS"])
AS_VAR_IF([GXX], [yes], [CXXFLAGS="-Wall $CXXFLAGS"])


AC_MSG_NOTICE([summary
---------------------------------------------------------------------------

  Build type / target platform            :  $buildtype / $host_os

  Optional features:
      LADSPA plugin support               :  $with_ladspa
      CDDA (Audio CD) support             :  $with_cdda
      CDDB support                        :  $with_cddb
      Sample Rate Converter support       :  $with_src
      iRiver iFP driver support           :  $with_ifp
      JACK port management support        :  $enable_jack_mgmt
      Loop playback support               :  $enable_loop
      Systray support                     :  $enable_systray
      Podcast support                     :  $enable_podcast
      Lua extension support               :  $with_lua

  Decoding support:
      sndfile (WAV, AIFF, AU, etc.)       :  $with_sndfile
      Free Lossless Audio Codec (FLAC)    :  $with_flac
      Ogg Vorbis                          :  $with_vorbis
      Ogg Speex                           :  $with_speex
      MPEG Audio (MPEG 1-2.5 Layer I-III) :  $with_mpeg
      MOD Audio (MOD, S3M, XM, IT, etc.)  :  $with_mod
      Musepack                            :  $with_mpc
      Monkey's Audio Codec                :  $with_mac
      WavPack                             :  $with_wavpack
      LAVC (AC3, AAC, WavPack, WMA, etc.) :  $with_lavc

  Encoding support:
      sndfile (WAV)                       :  $with_sndfile_enc
      Free Lossless Audio Codec (FLAC)    :  $with_flac_enc
      Ogg Vorbis                          :  $with_vorbisenc
      LAME (MP3)                          :  $with_lame

  Output driver support:
      sndio Audio                         :  $with_sndio
      OSS Audio                           :  $with_oss
      ALSA Audio                          :  $with_alsa
      JACK Audio Server                   :  $with_jack
      PulseAudio                          :  $with_pulse
      Win32 Sound API                     :  $with_winmm

---------------------------------------------------------------------------])
AC_OUTPUT
