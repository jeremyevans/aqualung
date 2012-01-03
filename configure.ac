# Process this file with autoconf to produce a configure script.
AC_PREREQ([2.60])
AC_INIT([aqualung],[SVN],[http://aqualung.factorial.hu/mantis])
AM_INIT_AUTOMAKE
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
    src/decoder/Makefile
    src/encoder/Makefile
    src/img/Makefile
])
AC_CANONICAL_HOST


# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_CXX
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS
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
PKG_CHECK_MODULES([xml], [libxml-2.0])
PKG_CHECK_MODULES([glib], [glib-2.0 gthread-2.0])
PKG_CHECK_MODULES([gtk], [gtk+-2.0 > 2.8])


# Checks for header files.
AC_CHECK_HEADERS([dlfcn.h errno.h fcntl.h sys/ioctl.h])


# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T


# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([floor memset mkdir psiginfo strcasestr strdup strndup strrchr strstr])


# Platform-specific tweaks.
AC_SYS_LARGEFILE
AS_CASE([$host_os],
    [cygwin*],
        [AS_IF([test "x$GCC" = 'xyes'], [CPPFLAGS="-mwin32 $CPPFLAGS"])],
    [AC_CHECK_LIB([pthread], [pthread_create], [],
        [AC_MSG_ERROR([pthreads are required to build Aqualung])])])




#
# Feature Options
#
AC_MSG_CHECKING([for type of build])
buildtype='release'
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug], [compile with debugging support])],
    [AS_IF([test "x$enable_debug" = 'xyes'], [buildtype='debug'])])
AC_MSG_RESULT([$buildtype])
AS_IF([test "x$buildtype" = 'xrelease'],
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
        AS_IF([test "x$GCC" = 'xyes'], [
            AC_SUBST([debug_LDFLAGS], [-rdynamic])
            CFLAGS="$CFLAGS -ggdb -g -O0"
        ])
        AS_IF([test "x$GXX" = 'xyes'], [CXXFLAGS="$CXXFLAGS -ggdb -g -O0"])
        AC_DEFINE([DEBUG_BUILD], [1], [Define if doing a debug build])
    ])

AC_MSG_CHECKING([for loop playback support])
AC_ARG_ENABLE([loop],
    [AS_HELP_STRING([--disable-loop], [compile without loop playback support])],
    [], [enable_loop='yes'])
AC_MSG_RESULT([$enable_loop])
AS_IF([test "x$enable_loop" = 'xyes'],
    [AC_DEFINE([HAVE_LOOP], [1], [Define to build with loop playback support])])

AC_MSG_CHECKING([for podcast support])
AC_ARG_ENABLE([podcast],
    [AS_HELP_STRING([--disable-podcast], [compile without podcast support])],
    [], [enable_podcast='yes'])
AC_MSG_RESULT([$enable_podcast])
AS_IF([test "x$enable_podcast" = 'xyes'],
    [AC_DEFINE([HAVE_PODCAST], [1], [Define to build with podcast support])])

AC_MSG_CHECKING([for systray support])
AC_ARG_ENABLE([systray],
    [AS_HELP_STRING([--disable-systray], [compile without systray support])],
    [], [enable_systray='yes'])
AS_IF([test "x$enable_systray" = 'xyes'],
    [PKG_CHECK_EXISTS([gtk+-2.0 >= 2.10],
        [AC_DEFINE([HAVE_SYSTRAY], [1], [Define to build with GTK+ systray])],
        [enable_systray='no (GTK+ too old)'])])
AC_MSG_RESULT([$enable_systray])


#
# Output Driver Library Options
#
AC_ARG_WITH([alsa],
    [AS_HELP_STRING([--with-alsa],
        [compile with ALSA support (default: detect)])],
    [], [with_alsa='detect'])
AS_IF([test "x$with_alsa" != 'xno'], [
    AC_MSG_NOTICE([ALSA Support])
    PKG_CHECK_MODULES([alsa], [alsa],
        [AS_IF([test "x$with_alsa" = 'xdetect'], [with_alsa='yes (found)'])],
        [AS_IF([test "x$with_alsa" = 'xyes'],
            [AC_MSG_ERROR([ALSA support requires libasound])])])
    AS_CASE([$with_alsa],
        [detect], [with_alsa='not found'],
        [yes*], [AC_DEFINE([HAVE_ALSA], [1],
                    [Define to build with ALSA support])])
])

AC_ARG_WITH([jack],
    [AS_HELP_STRING([--with-jack],
        [compile with JACK support (default: detect)])],
    [], [with_jack='detect'])
AS_IF([test "x$with_jack" != 'xno'], [
    AC_MSG_NOTICE([JACK Support])
    AC_CHECK_LIB([jack], [jack_on_info_shutdown],
        [
            AS_IF([test "x$with_jack" = 'xdetect'], [with_jack='yes (found)'])
            AC_SUBST([jack_LIBS], [-ljack])
        ],
        [AS_IF([test "x$with_jack" = 'xyes'],
            [AC_MSG_ERROR([JACK support requires libjack])])])
    AS_CASE([$with_jack],
        [detect], [with_jack='not found'],
        [yes*], [AC_DEFINE([HAVE_JACK], [1],
                    [Define to build with JACK support])])
])

AC_ARG_WITH([oss],
    [AS_HELP_STRING([--with-oss],
        [compile with OSS support (default: detect)])],
    [], [with_oss='detect'])
AS_IF([test "x$with_oss" != 'xno'], [
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

AC_ARG_WITH([pulse],
    [AS_HELP_STRING([--with-pulse],
        [compile with PulseAudio support (default: detect)])],
    [], [with_pulse='detect'])
AS_IF([test "x$with_pulse" != 'xno'], [
    AC_MSG_NOTICE([PulseAudio Support])
    PKG_CHECK_MODULES([pulse], [libpulse-simple],
        [AS_IF([test "x$with_pulse" = 'xdetect'], [with_pulse='yes (found)'])],
        [AS_IF([test "x$with_pulse" = 'xyes'],
            [AC_MSG_ERROR([$pulse_PKG_ERRORS])])])
    AS_CASE([$with_pulse],
        [detect], [with_pulse='not found'],
        [yes*], [AC_DEFINE([HAVE_PULSE], [1],
                    [Define to build with PulseAudio support])])
])

AC_ARG_WITH([sndio],
    [AS_HELP_STRING([--with-sndio],
        [compile with sndio support (default: detect)])],
    [], [with_sndio='detect'])
AS_IF([test "x$with_sndio" != 'xno'], [
    AC_MSG_NOTICE([sndio Support])
    AC_CHECK_LIB([sndio], [sio_open],
        [
            AS_IF([test "x$with_sndio" = 'xdetect'], [with_sndio='yes (found)'])
            AC_SUBST([sndio_LIBS], [-lsndio])
        ],
        [AS_IF([test "x$with_sndio" = 'xyes'],
            [AC_MSG_ERROR([sndio support requires libsndio])])])
    AS_CASE([$with_sndio],
        [detect], [with_sndio='not found'],
        [yes*], [AC_DEFINE([HAVE_SNDIO], [1],
                    [Define to build with sndio support])])
])

AC_ARG_WITH([winmm],
    [AS_HELP_STRING([--with-winmm],
        [compile with WinMM support (default: detect)])],
    [], [with_winmm='detect'])
AS_IF([test "x$with_winmm" != 'xno'], [
    AC_MSG_NOTICE([Windows Multimedia Extensions API Support])
    AC_MSG_CHECKING([for platform support])
    AS_CASE([$host_os], [cygwin*], [AC_MSG_RESULT([yes])], [
        AC_MSG_RESULT([no])
        AS_IF([test "x$with_winmm" = 'xyes'],
            [AC_MSG_ERROR([WinMM is only supported on Microsoft Windows])])
        with_winmm="not supported on $host_os"
    ])
    AS_CASE([$with_winmm], [yes|detect],
        [AC_CHECK_HEADER([mmsystem.h],
            [
                AS_IF([test "x$with_winmm" = 'xdetect'],
                    [with_winmm='yes (found)'])
                AC_SUBST([winmm_LIBS], [-lwinmm])
            ],
            [AS_IF([test "x$with_winmm" = 'xyes'],
                [AC_MSG_ERROR([WinMM support requires mmsystem.h])])],
            [[#include <windows.h>]])])
    AS_CASE([$with_winmm],
        [detect], [with_winmm='not found'],
        [yes*], [AC_DEFINE([HAVE_WINMM], [1],
                    [Define to build with WinMM support])])
])


#
# Codec Library Options
#
AC_ARG_WITH([flac],
    [AS_HELP_STRING([--with-flac],
        [compile with FLAC support (default: detect)])],
    [], [with_flac='detect'])
AS_IF([test "x$with_flac" != 'xno'], [
    AC_MSG_NOTICE([Free Lossless Audio Codec Support])
    PKG_CHECK_MODULES([flac], [flac],
        [AS_IF([test "x$with_flac" = 'xdetect'], [with_flac='yes (found)'])],
        [AS_IF([test "x$with_flac" = 'xyes'],
            [AC_MSG_ERROR([$flac_PKG_ERRORS])])])
    AS_CASE([$with_flac],
        [detect], [with_flac='not found'],
        [yes*], [AC_DEFINE([HAVE_FLAC], [1],
                    [Define to build with FLAC support])])
])

AC_ARG_WITH([lame],
    [AS_HELP_STRING([--with-lame],
        [compile with LAME MP3 Encoding support (default: detect)])],
    [], [with_lame='detect'])
AS_IF([test "x$with_lame" != 'xno'], [
    AC_MSG_NOTICE([LAME MP3 Encoding Support])
    AC_CHECK_LIB([mp3lame], [lame_init],
        [
            AS_IF([test "x$with_lame" = 'xdetect'], [with_lame='yes (found)'])
            AC_SUBST([lame_LIBS], [-lmp3lame])
        ],
        [AS_IF([test "x$with_lame" = 'xyes'],
            [AC_MSG_ERROR([LAME support requires libmp3lame])])])
    AS_CASE([$with_lame],
        [detect], [with_lame='not found'],
        [yes*], [AC_DEFINE([HAVE_LAME], [1],
                    [Define to build with LAME MP3 Encoding support])])
])

AC_ARG_WITH([lavc],
    [AS_HELP_STRING([--with-lavc],
        [compile with Libav / FFmpeg support (default: detect)])],
    [], [with_lavc='detect'])
AS_IF([test "x$with_lavc" != 'xno'], [
    AC_MSG_NOTICE([Libav / FFmpeg Support])
    PKG_CHECK_MODULES([lavc], [libavcodec libavformat],
        [AS_IF([test "x$with_lavc" = 'xdetect'], [with_lavc='yes (found)'])],
        [AS_IF([test "x$with_lavc" = 'xyes'],
            [AC_MSG_ERROR([$lavc_PKG_ERRORS])])])
    AS_CASE([$with_lavc],
        [detect], [with_lavc='not found'],
        [yes*], [AC_DEFINE([HAVE_LAVC], [1],
                    [Define to build with Libav / FFmpeg support])])
])

AC_ARG_WITH([mac],
    [AS_HELP_STRING([--with-mac],
        [compile with Monkey's Audio Codec support (default: detect)])],
    [], [with_mac='detect'])
AS_IF([test "x$with_mac" != 'xno'], [
    AC_MSG_NOTICE([Monkey's Audio Codec Support])
    AC_CHECK_LIB([mac], [CreateIAPEDecompress],
        [
            AS_IF([test "x$with_mac" = 'xdetect'], [with_mac='yes (found)'])
            AC_SUBST([mac_LIBS], ['-lmac -lstdc++'])
        ],
        [AS_IF([test "x$with_mac" = 'xyes'],
            [AC_MSG_ERROR([Monkey's Audio Codec support requires libmac])])],
        [-lstdc++])
    AS_CASE([$with_mac],
        [detect], [with_mac='not found'],
        [yes*], [AC_DEFINE([HAVE_MAC], [1],
                    [Define to build with Monkey's Audio Codec support])])
])

AC_ARG_WITH([mod],
    [AS_HELP_STRING([--with-mod],
        [compile with MOD Audio support (default: detect)])],
    [], [with_mod='detect'])
AS_IF([test "x$with_mod" != 'xno'], [
    AC_MSG_NOTICE([MOD Audio Support])
    PKG_CHECK_MODULES([mod], [libmodplug >= 0.8.4],
        [AS_IF([test "x$with_mod" = 'xdetect'], [with_mod='yes (found)'])],
        [AS_IF([test "x$with_mod" = 'xyes'],
            [AC_MSG_ERROR([$mod_PKG_ERRORS])])])
    AS_CASE([$with_mod],
        [detect], [with_mod='not found'],
        [yes*], [AC_DEFINE([HAVE_MOD], [1],
                    [Define to build with MOD Audio support])])
])

AC_ARG_WITH([mpc],
    [AS_HELP_STRING([--with-mpc],
        [compile with Musepack support (default: detect)])],
    [], [with_mpc='detect'])
AS_IF([test "x$with_mpc" != 'xno'], [
    AC_MSG_NOTICE([Musepack Support])
    AC_CHECK_LIB([mpcdec], [mpc_demux_init],
        [
            AS_IF([test "x$with_mpc" = 'xdetect'], [with_mpc='yes (found)'])
            AC_SUBST([mpc_LIBS], ['-lmpcdec'])
        ],
        [AC_CHECK_LIB([mpcdec], [mpc_streaminfo_init],
            [
                AS_IF([test "x$with_mpc" = 'xdetect'], [with_mpc='yes (found)'])
                AC_SUBST([mpc_LIBS], ['-lmpcdec'])
                AC_DEFINE([MPC_OLD_API], [1],
                    [Defined if old Musepack API is used])
            ],
            [AS_IF([test "x$with_mpc" = 'xyes'],
                [AC_MSG_ERROR([Musepack support requires libmpcdec])])])])
    AS_CASE([$with_mpc],
        [detect], [with_mpc='not found'],
        [yes*], [AC_DEFINE([HAVE_MPC], [1],
                    [Define to build with Musepack support])])
])

AC_ARG_WITH([mpeg],
    [AS_HELP_STRING([--with-mpeg],
        [compile with MPEG Audio support (default: detect)])],
    [], [with_mpeg='detect'])
AS_IF([test "x$with_mpeg" != 'xno'], [
    AC_MSG_NOTICE([MPEG Audio Support])
    PKG_CHECK_MODULES([mad], [mad],
        [AS_IF([test "x$with_mpeg" = 'xdetect'], [with_mpeg='yes (found)'])],
        [AS_IF([test "x$with_mpeg" = 'xyes'],
            [AC_MSG_ERROR([$mad_PKG_ERRORS])])])
    AS_CASE([$with_mpeg],
        [detect], [with_mpeg='not found'],
        [yes*], [AC_DEFINE([HAVE_MPEG], [1],
                    [Define to build with MPEG Audio support])])
])

AC_ARG_WITH([sndfile],
    [AS_HELP_STRING([--with-sndfile],
        [compile with sndfile (WAV, AIFF, etc.) support (default: detect)])],
    [], [with_sndfile='detect'])
AS_IF([test "x$with_sndfile" != 'xno'], [
    AC_MSG_NOTICE([sndfile (WAV, AIFF, etc.) Support])
    PKG_CHECK_MODULES([sndfile], [sndfile],
        [AS_IF([test "x$with_sndfile" = 'xdetect'],
            [with_sndfile='yes (found)'])],
        [AS_IF([test "x$with_sndfile" = 'xyes'],
            [AC_MSG_ERROR([$sndfile_PKG_ERRORS])])])
    PKG_CHECK_EXISTS([sndfile >= 1.0.12],
        [AC_DEFINE([HAVE_SNDFILE_1_0_12], [1], [Defined if sndfile >= 1.0.12])])
    PKG_CHECK_EXISTS([sndfile >= 1.0.18],
        [AC_DEFINE([HAVE_SNDFILE_1_0_18], [1], [Defined if sndfile >= 1.0.18])])
    AS_CASE([$with_sndfile],
        [detect], [with_sndfile='not found'],
        [yes*], [AC_DEFINE([HAVE_SNDFILE], [1],
                    [Define to build with sndfile (WAV, AIFF, etc.) support])])
])

AC_ARG_WITH([speex],
    [AS_HELP_STRING([--with-speex],
        [compile with Ogg Speex support (default: detect)])],
    [], [with_speex='detect'])
AS_IF([test "x$with_speex" != 'xno'], [
    AC_MSG_NOTICE([Ogg Speex Support])
    PKG_CHECK_MODULES([speex], [oggz speex],
        [AS_IF([test "x$with_speex" = 'xdetect'], [with_speex='yes (found)'])],
        [AS_IF([test "x$with_speex" = 'xyes'],
            [AC_MSG_ERROR([$speex_PKG_ERRORS])])])
    AS_CASE([$with_speex],
        [detect], [with_speex='not found'],
        [yes*], [AC_DEFINE([HAVE_SPEEX], [1],
                    [Define to build with Ogg Speex support])])
])

AC_ARG_WITH([vorbis],
    [AS_HELP_STRING([--with-vorbis],
        [compile with Ogg Vorbis support (default: detect)])],
    [], [with_vorbis='detect'])
AS_IF([test "x$with_vorbis" != 'xno'], [
    AC_MSG_NOTICE([Ogg Vorbis Support])
    PKG_CHECK_MODULES([vorbis], [vorbis vorbisfile],
        [AS_IF([test "x$with_vorbis" = 'xdetect'],
            [with_vorbis='yes (found)'])],
        [AS_IF([test "x$with_vorbis" = 'xyes'],
            [AC_MSG_ERROR([$vorbis_PKG_ERRORS])])])
    AS_CASE([$with_vorbis],
        [detect], [with_vorbis='not found'],
        [yes*], [AC_DEFINE([HAVE_VORBIS], [1],
                    [Define to build with Ogg Vorbis support])])
])

AC_MSG_CHECKING(for Ogg Vorbis encoding support)
AC_ARG_WITH(
	vorbisenc,
	[  --with-vorbisenc=yes,no compile with Ogg Vorbis encoding support (default: yes)],
	vorbisenc="$withval",
	vorbisenc="detect")
if test "$vorbisenc" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(vorbisenc, vorbis_encode_setup_init, [lib=yes], [lib=no], [-lvorbis -logg])
	if test "$lib" = "yes"; then
		vorbisenc_LIBS="-logg -lvorbis -lvorbisenc"
		AC_DEFINE([HAVE_VORBISENC], [1], [Defined if compile with Ogg Vorbis encoding support])
	fi
	if test "$lib" = "no" -a "$vorbisenc" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the ogg, vorbis and vorbisenc libraries installed. Grab them from http://www.xiph.org/ogg/vorbis)
	fi
	if test "$vorbisenc" = "detect"; then
		vorbisenc=$lib
	fi
fi


AC_ARG_WITH([wavpack],
    [AS_HELP_STRING([--with-wavpack],
        [compile with WavPack support (default: detect)])],
    [], [with_wavpack='detect'])
AS_IF([test "x$with_wavpack" != 'xno'], [
    AC_MSG_NOTICE([WavPack Support])
    PKG_CHECK_MODULES([wavpack], [wavpack >= 4.40.0],
        [AS_IF([test "x$with_wavpack" = 'xdetect'],
            [with_wavpack='yes (found)'])],
        [AS_IF([test "x$with_wavpack" = 'xyes'],
            [AC_MSG_ERROR([$wavpack_PKG_ERRORS])])])
    AS_CASE([$with_wavpack],
        [detect], [with_wavpack='not found'],
        [yes*], [AC_DEFINE([HAVE_WAVPACK], [1],
                    [Define to build with WavPack support])])
])


#
# DSP Library Options
#
AC_ARG_WITH([ladspa],
    [AS_HELP_STRING([--with-ladspa],
        [compile with LADSPA Plugin support (default: detect)])],
    [], [with_ladspa='detect'])
AS_IF([test "x$with_ladspa" != 'xno'], [
    AC_MSG_NOTICE([LADSPA Plugin Support])
    PKG_CHECK_MODULES([lrdf], [lrdf >= 0.4.0],
        [AS_IF([test "x$with_ladspa" = 'xdetect'],
            [with_ladspa='yes (found)'])],
        [AS_IF([test "x$with_ladspa" = 'xyes'],
            [AC_MSG_ERROR([LADSPA support requires liblrdf >= 0.4.0])])])
    AS_CASE([$with_ladspa], [yes*],
        [AC_CHECK_FUNC([dlopen], [],
            [AC_CHECK_LIB([dl], [dlopen], [lrdf_LIBS="$lrdf_LIBS -ldl"], [
                AS_IF([test "x$with_ladspa" = 'xyes'],
                    [AC_MSG_ERROR([LADSPA support requires dynamic loading])])
                with_ladspa='no (missing dlopen)'
            ])])])
    AS_CASE([$with_ladspa],
        [detect], [with_ladspa='not found'],
        [yes*], [AC_DEFINE([HAVE_LADSPA], [1],
                    [Define to build with LADSPA Plugin support])])
])

AC_ARG_WITH([src],
    [AS_HELP_STRING([--with-src],
	[compile with Sample Rate Converter support (default: detect)])],
    [], [with_src='detect'])
AS_IF([test "x$with_src" != 'xno'], [
    AC_MSG_NOTICE([Sample Rate Converter Support])
    PKG_CHECK_MODULES([src], [samplerate],
        [AS_IF([test "x$with_src" = 'xdetect'], [with_src='yes (found)'])],
        [AS_IF([test "x$with_src" = 'xyes'],
            [AC_MSG_ERROR([$src_PKG_ERRORS])])])
    AS_CASE([$with_src],
        [detect], [with_src='not found'],
        [yes*], [AC_DEFINE([HAVE_SRC], [1],
                    [Define to build with Sample Rate Converter support])])
])


#
# Hardware Library Options
#
AC_ARG_WITH([cdda],
    [AS_HELP_STRING([--with-cdda],
        [compile with CDDA support (default: detect)])],
    [], [with_cdda='detect'])
AS_IF([test "x$with_cdda" != 'xno'], [
    AC_MSG_NOTICE([Compact Disc Digital Audio Support])
    PKG_CHECK_MODULES([cdio], [libcdio libcdio_paranoia >= 0.76],
        [AS_IF([test "x$with_cdda" = 'xdetect'], [with_cdda='yes (found)'])],
        [AS_IF([test "x$with_cdda" = 'xyes'],
            [AC_MSG_ERROR([$cdio_PKG_ERRORS])])])
    AS_CASE([$with_cdda],
        [detect], [with_cdda='not found'],
        [yes*], [AC_DEFINE([HAVE_CDDA], [1],
                    [Define to build with CDDA support])])
])

AC_ARG_WITH([ifp],
    [AS_HELP_STRING([--with-ifp],
        [compile with iRiver iFP support (default: detect)])],
    [], [with_ifp='detect'])
AS_IF([test "x$with_ifp" != 'xno'], [
    AC_MSG_NOTICE([iRiver iFP Support])
    AC_CHECK_LIB([usb], [usb_init],
        [AC_CHECK_LIB([ifp], [ifp_find_device],
            [
                AS_IF([test "x$with_ifp" = 'xdetect'], [with_ifp='yes (found)'])
                AC_SUBST([ifp_LIBS], ['-lusb -lifp'])
            ],
            [AS_IF([test "x$with_ifp" = 'xyes'],
                [AC_MSG_ERROR([iRiver iFP support requires libifp])])])],
        [AS_IF([test "x$with_ifp" = 'xyes'],
            [AC_MSG_ERROR([iRiver iFP support requires libusb])])])
    AS_CASE([$with_ifp],
        [detect], [with_ifp='not found'],
        [yes*], [AC_DEFINE([HAVE_IFP], [1],
                    [Define to build with iRiver iFP support])])
])


#
# Miscellaneous Library Options
#
AC_ARG_WITH([cddb],
    [AS_HELP_STRING([--with-cddb],
        [compile with CDDB support (default: detect)])],
    [], [with_cddb='detect'])
AS_IF([test "x$with_cddb" != 'xno'], [
    AC_MSG_NOTICE([Compact Disc Database Support])
    PKG_CHECK_MODULES([cddb], [libcddb >= 1.2.1],
        [AS_IF([test "x$with_cddb" = 'xdetect'], [with_cddb='yes (found)'])],
        [AS_IF([test "x$with_cddb" = 'xyes'],
            [AC_MSG_ERROR([$cddb_PKG_ERRORS])])])
    AC_CHECK_LIB([cddb], [cddb_disc_get_revision],
        [AC_DEFINE([LIBCDDB_REVISION], [1],
            [Define if libcddb has cddb_disc_get_revision function])])
    AS_CASE([$with_cddb],
        [detect], [with_cddb='not found'],
        [yes*], [AC_DEFINE([HAVE_CDDB], [1],
                    [Define to build with CDDB support])])
])

AC_ARG_WITH([lua],
    [AS_HELP_STRING([--with-lua],
        [compile with Lua support (default: detect)])],
    [], [with_lua='detect'])
AS_IF([test "x$with_lua" != 'xno'], [
    AC_MSG_NOTICE([Lua Extension Support])
    PKG_CHECK_MODULES([lua], [lua5.1],
        [AS_IF([test "x$with_lua" = 'xdetect'], [with_lua='yes (found)'])],
        [PKG_CHECK_MODULES([lua], [lua >= 5.1],
            [AS_IF([test "x$with_lua" = 'xdetect'], [with_lua='yes (found)'])],
            [AS_IF([test "x$with_lua" = 'xyes'],
                [AC_MSG_ERROR([$lua_PKG_ERRORS])])])])
    AS_CASE([$with_lua],
        [detect], [with_lua='not found'],
        [yes*], [AC_DEFINE([HAVE_LUA], [1],
                    [Define to build with Lua support])])
])


# Compiler and linker variables
AS_IF([test "x$GCC" = 'xyes'], [CFLAGS="-Wall $CFLAGS"])
AS_IF([test "x$GXX" = 'xyes'], [CXXFLAGS="-Wall $CXXFLAGS"])
LIBS="$LIBS $vorbisenc_LIBS"


AC_MSG_NOTICE([summary
---------------------------------------------------------------------------

  Build type / target platform            :  $buildtype / $host_os

  Optional features:
      LADSPA plugin support               :  $with_ladspa
      CDDA (Audio CD) support             :  $with_cdda
      CDDB support                        :  $with_cddb
      Sample Rate Converter support       :  $with_src
      iRiver iFP driver support           :  $with_ifp
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
      sndfile (WAV)                       :  $with_sndfile
      Free Lossless Audio Codec (FLAC)    :  $with_flac
      Ogg Vorbis                          :  $vorbisenc
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
