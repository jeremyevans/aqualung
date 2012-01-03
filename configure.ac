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
AC_MSG_CHECKING(for sndfile support)
AC_ARG_WITH(
	sndfile,
	[  --with-sndfile=yes,no   compile with sndfile (WAV, AIFF, etc.) support (default: yes)],
	sndfile="$withval",
	sndfile="detect")
if test "$sndfile" = "no"; then
	AC_MSG_RESULT(no)
else
        AC_CHECK_LIB(sndfile, sf_open, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
		sndfile_LIBS=`pkg-config --libs sndfile`
		AC_DEFINE([HAVE_SNDFILE], [1], [Defined if compile with sndfile (WAV, AIFF, etc.) support])

		AC_MSG_CHECKING(whether sndfile version >= 1.0.12)
		if pkg-config --exists 'sndfile >= 1.0.12'; then
		        AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_SNDFILE_1_0_12], [1], [Defined if version(sndfile) >= 1.0.12])
		else
			AC_MSG_RESULT(no)
		fi

		AC_MSG_CHECKING(whether sndfile version >= 1.0.18)
		if pkg-config --exists 'sndfile >= 1.0.18'; then
		        AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_SNDFILE_1_0_18], [1], [Defined if version(sndfile) >= 1.0.18])
		else
			AC_MSG_RESULT(no)
		fi
	fi
	if test "$lib" = "no" -a "$sndfile" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the sndfile library installed. Grab it from  http://www.mega-nerd.com/libsndfile/ )
	fi
	if test "$sndfile" = "detect"; then
	        sndfile=$lib
	fi
fi


AC_MSG_CHECKING(for FLAC support)
AC_ARG_WITH(
	flac,
	[  --with-flac=yes,no      compile with FLAC support (default: yes)],
	flac="$withval",
	flac="detect")
if test "$flac" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(FLAC, FLAC__stream_decoder_init_file, [lib=yes], [lib=no], [-logg])
	if test "$lib" = "yes"; then
	        flac_LIBS="-lFLAC"
		AC_DEFINE([HAVE_FLAC_8], [1], [Defined if compile with FLAC (1.1.3 and newer API)])
		AC_DEFINE([HAVE_FLAC], [1], [Defined if compile with some kind of FLAC support])
	fi
	AC_CHECK_LIB(FLAC, FLAC__file_decoder_new, [lib2=yes], [lib2=no])
	if test "$lib2" = "yes"; then
	        flac_LIBS="-lFLAC"
		AC_DEFINE([HAVE_FLAC_7], [1], [Defined if compile with FLAC (1.1.2 and older API)])
		AC_DEFINE([HAVE_FLAC], [1], [Defined if compile with some kind of FLAC support])
	fi
	if test "$lib" = "no" -a "$lib2" = "no" -a "$flac" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the FLAC library installed. Grab it from http://flac.sourceforge.net)
	fi
	if test "$flac" = "detect"; then
		if test "$lib2" = "yes" -o "$lib" = "yes"; then
			flac="yes"
		else
			flac="no"
		        AC_MSG_RESULT(no)
		fi
	fi
fi


AC_MSG_CHECKING(for Ogg Vorbis support)
AC_ARG_WITH(
	ogg,
	[  --with-ogg=yes,no       compile with Ogg Vorbis support (default: yes)],
	ogg="$withval",
	ogg="detect")
if test "$ogg" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(vorbis, ov_open, [lib=yes], [lib=no], [-lvorbisfile -logg])
	if test "$lib" = "yes"; then
		ogg_LIBS="-logg -lvorbis -lvorbisfile"
		AC_DEFINE([HAVE_OGG_VORBIS], [1], [Defined if compile with Ogg Vorbis support])
	fi
	if test "$lib" = "no" -a "$ogg" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the ogg, vorbis and vorbisfile libraries installed. Grab them from http://www.xiph.org/ogg/vorbis)
	fi
	if test "$ogg" = "detect"; then
		ogg=$lib
	fi
fi


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


AC_MSG_CHECKING(for Ogg Speex support)
AC_ARG_WITH(
	speex,
	[  --with-speex=yes,no     compile with Ogg Speex support (default: yes)],
	speex="$withval",
	speex="detect")
if test "$speex" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(oggz, oggz_open, [oggzlib=yes], [oggzlib=no], [-loggz -logg])
	AC_CHECK_LIB(speex, speex_bits_init, [speexlib=yes], [speexlib=no], [-lspeex])
	if test "$oggzlib" = "yes"; then
		if test "$speexlib" = "yes"; then
			speex_LIBS="-loggz -logg -lspeex"
			lib="yes"
			AC_DEFINE([HAVE_SPEEX], [1], [Defined if compile with Ogg Speex support])
		else
			lib="no"
		fi
	else
		lib="no"
	fi
	if test "$lib" = "no" -a "$speex" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the oggz and speex libraries installed. Grab them from http://www.annodex.net/software/liboggz and http://speex.org)
	fi
	if test "$speex" = "detect"; then
		speex=$lib
	fi
fi


AC_MSG_CHECKING(for MPEG audio support)
AC_ARG_WITH(
	mpeg,
	[  --with-mpeg=yes,no      compile with MPEG Audio support (default: yes)],
	mpeg="$withval",
	mpeg="detect")
if test "$mpeg" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_HEADER([mad.h], [hdr=yes], [hdr=no])
	if test "$hdr" = "no"; then
		lib="no"
	else
		AC_CHECK_LIB(mad, mad_decoder_init, [lib=yes], [lib=no])
		if test "$lib" = "yes"; then
		        mad_LIBS="-lmad"
			AC_DEFINE([HAVE_MPEG], [1], [Defined if compile with MPEG Audio support])
		fi
	fi
	if test "$lib" = "no" -a "$mpeg" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the MAD library installed. Grab it from http://www.underbit.com/products/mad/)
	fi
	if test "$mpeg" = "detect"; then
		mpeg=$lib
	fi
fi


AC_MSG_CHECKING(for MOD audio support)
AC_ARG_WITH(
	mod,
	[  --with-mod=yes,no       compile with MOD Audio support (default: yes)],
	mod="$withval",
	mod="detect")
if test "$mod" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(modplug, ModPlug_Load, [lib=yes], [lib=no], [-lstdc++ -lm])
	if test "$lib" = "yes"; then
	        mod_LIBS=`pkg-config --libs libmodplug`
		AC_DEFINE([HAVE_MOD], [1], [Defined if compile with MOD Audio support])
	fi
	if test "$lib" = "no" -a "$mod" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the libmodplug library installed. Grab it from http://modplug-xmms.sourceforge.net/)
	fi
	if test "$mod" = "detect"; then
		mod=$lib
	fi

	if test "$mod" = "yes"; then
	        AC_MSG_CHECKING(for Module info)
		AC_MSG_CHECKING(whether libmodplug version >= 0.8)
		if pkg-config --exists 'libmodplug >= 0.8'; then
                        AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_MOD_INFO], [1], [Defined if version(libmodplug) >= 0.8])
		else
			AC_MSG_RESULT(no)
		fi
	fi
fi

AC_MSG_CHECKING(for Musepack support)
AC_ARG_WITH(
	mpc,
	[  --with-mpc=yes,no       compile with Musepack support (default: yes)],
	mpc="$withval",
	mpc="detect")
if test "$mpc" = "no"; then
	AC_MSG_RESULT(no)
else
	lib=no
	AC_CHECK_LIB([mpcdec], [mpc_demux_init], [lib=yes],
	  [AC_CHECK_LIB([mpcdec], [mpc_streaminfo_init], [lib=yes
	    AC_DEFINE([MPC_OLD_API], [1],
            [Defined if old Musepack API is used])], [], [-lstdc++])],
	  [-lstdc++])
	if test "$lib" = "yes"; then
	        mpc_LIBS="-lmpcdec -lstdc++"
		AC_DEFINE([HAVE_MPC], [1], [Defined if compile with Musepack support])
	fi
	if test "$lib" != "yes" -a "$mpc" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the Musepack decoder library (libmpcdec) installed. Grab it from http://www.musepack.net)
	fi
	if test "$mpc" = "detect"; then
		mpc=$lib
	fi
fi


AC_MSG_CHECKING(for Monkey's Audio Codec support)
AC_ARG_WITH(
	mac,
	[  --with-mac=yes,no       compile with Monkey's Audio Codec support (default: yes)],
	mac="$withval",
	mac="detect")
if test "$mac" = "no"; then
	AC_MSG_RESULT(no)
        mac_LIBS="-lstdc++"
else
	AC_CHECK_LIB(mac, CreateIAPEDecompress, [lib=yes], [lib=no], [-lstdc++])
	if test "$lib" = "yes"; then
	        mac_LIBS="-lmac -lstdc++"
		AC_DEFINE([HAVE_MAC], [1], [Defined if compile with Monkey's Audio Codec support])
	else
	        mac_LIBS="-lstdc++"
	fi
	if test "$lib" = "no" -a "$mac" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the Monkey's Audio Codec decoder library installed. Grab it from http://etree.org/shnutils/shntool/support/formats/ape/unix/)
	fi
	if test "$mac" = "detect"; then
		mac=$lib
	fi
fi


AC_MSG_CHECKING(for LAVC support)
AC_ARG_WITH(
	lavc,
	[  --with-lavc=yes,no      compile with lavc (FFmpeg) support (default: yes)],
	lavc="$withval",
	lavc="detect")
if test "$lavc" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_HEADER([avcodec.h], [avc_hdr=yes], [avc_hdr=no])
	if test "$avc_hdr" = "yes"; then
		AC_DEFINE([HAVE_AVCODEC_H], [1], [Define to 1 if you have the <avcodec.h> header file.])
	else
		AC_CHECK_HEADER([ffmpeg/avcodec.h], [avc_hdr=yes], [avc_hdr=no])
		if test "$avc_hdr" = "yes"; then
		        AC_DEFINE([HAVE_FFMPEG_AVCODEC_H], [1], [Define to 1 if you have the <ffmpeg/avcodec.h> header file.])
		else
			AC_CHECK_HEADER([libavcodec/avcodec.h], [avc_hdr=yes], [avc_hdr=no])
			if test "$avc_hdr" = "yes"; then
		                AC_DEFINE([HAVE_LIBAVCODEC_AVCODEC_H], [1], [Define to 1 if you have the <libavcodec/avcodec.h> header file.])
			else
				AC_CHECK_HEADER([ffmpeg/libavcodec/avcodec.h], [avc_hdr=yes], [avc_hdr=no])
				if test "$avc_hdr" = "yes"; then
		                        AC_DEFINE([HAVE_FFMPEG_LIBAVCODEC_AVCODEC_H], [1], [Define to 1 if you have the <ffmpeg/libavcodec/avcodec.h> header file.])
				else
					PKG_CHECK_MODULES(LIBAVCODEC, libavcodec, [avc_hdr=yes], [avc_hdr=no])
					if test "$avc_hdr" = "yes"; then
		                AC_DEFINE([HAVE_LIBAVCODEC_AVCODEC_H], [1], [Define to 1 if you have the <libavcodec/avcodec.h> header file.])
					fi
				fi
			fi
		fi
	fi

	AC_CHECK_HEADER([avformat.h], [avf_hdr=yes], [avf_hdr=no])
	if test "$avf_hdr" = "yes"; then
		AC_DEFINE([HAVE_AVFORMAT_H], [1], [Define to 1 if you have the <avformat.h> header file.])
	else
		AC_CHECK_HEADER([ffmpeg/avformat.h], [avf_hdr=yes], [avf_hdr=no])
		if test "$avf_hdr" = "yes"; then
		        AC_DEFINE([HAVE_FFMPEG_AVFORMAT_H], [1], [Define to 1 if you have the <ffmpeg/avformat.h> header file.])
		else
			AC_CHECK_HEADER([libavformat/avformat.h], [avf_hdr=yes], [avf_hdr=no])
			if test "$avf_hdr" = "yes"; then
		                AC_DEFINE([HAVE_LIBAVFORMAT_AVFORMAT_H], [1], [Define to 1 if you have the <libavformat/avformat.h> header file.])
			else
				AC_CHECK_HEADER([ffmpeg/libavformat/avformat.h], [avf_hdr=yes], [avf_hdr=no])
				if test "$avf_hdr" = "yes"; then
		                        AC_DEFINE([HAVE_FFMPEG_LIBAVFORMAT_AVFORMAT_H], [1], [Define to 1 if you have the <ffmpeg/libavformat/avformat.h> header file.])
				else
					PKG_CHECK_MODULES(LIBAVFORMAT, libavformat, [avf_hdr=yes], [avf_hdr=no])
					if test "$avf_hdr" = "yes"; then
		                AC_DEFINE([HAVE_LIBAVFORMAT_AVFORMAT_H], [1], [Define to 1 if you have the <libavformat/avformat.h> header file.])
					fi
				fi
			fi
		fi
	fi

	AC_CHECK_LIB(avformat, av_open_input_file, [avf_lib=yes], [avf_lib=no], [-lavcodec -lavutil -lz])
	AC_CHECK_LIB(avcodec, avcodec_open, [avc_lib=yes], [avc_lib=no], [-lavformat -lavutil -lz])

	if test "$avc_hdr" = "yes" -a "$avf_hdr" = "yes" -a "$avc_lib" = "yes" -a "$avf_lib" = "yes" ; then
	        lavc_LIBS="-lavformat -lavcodec -lavutil -lz"
		AC_DEFINE([HAVE_LAVC], [1], [Defined if compile with LAVC support])
	        lavc="yes"
	else
	        if test "$lavc" = "yes"; then
		        AC_MSG_ERROR(You do not appear to have the LAVC decoder library (FFmpeg) installed. Grab it from http://ffmpeg.mplayerhq.hu/)
		fi
		lavc="no"
	fi
fi


AC_MSG_CHECKING(for LAME (MP3 encoding) support)
AC_ARG_WITH(
	lame,
	[  --with-lame=yes,no      compile with LAME (MP3 encoding) support (default: yes)],
	lame="$withval",
	lame="detect")
if test "$lame" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(mp3lame, lame_init, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
		lame_LIBS="-lmp3lame"
		AC_DEFINE([HAVE_LAME], [1], [Defined if compile with LAME support])
	fi
	if test "$lib" = "no" -a "$lame" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the LAME library installed. Grab it from http://lame.sourceforge.net/)
	fi
	if test "$lame" = "detect"; then
		lame=$lib
	fi
fi

AC_MSG_CHECKING(for WavPack support)
AC_ARG_WITH(
	wavpack,
	[  --with-wavpack=yes,no   compile with WavPack support (default: yes)],
	wavpack="$withval",
	wavpack="detect")
if test "$wavpack" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(wavpack, WavpackOpenFileInput, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
		wavpack_LIBS=`pkg-config --libs wavpack`
		AC_MSG_CHECKING(whether WavPack version >= 4.40.0)
		if pkg-config --exists 'wavpack >= 4.40.0'; then
		        AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_WAVPACK], [1], [Defined if compile with Wavpack support])
		else
			lib="no"
		        AC_MSG_RESULT(no)
		fi
	fi
	if test "$lib" = "no" -a "$wavpack" = "yes"; then
		AC_MSG_ERROR([You do not appear to have the WavPack library installed, or it is older than the required version (4.40.0). Grab it from http://www.wavpack.com])
	fi
	if test "$wavpack" = "detect"; then
		wavpack=$lib
	fi
fi


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
LIBS="$LIBS $sndfile_LIBS $flac_LIBS $ogg_LIBS $wavpack_LIBS $speex_LIBS $mad_LIBS $mod_LIBS $mpc_LIBS $mac_LIBS $lavc_LIBS $vorbisenc_LIBS $lame_LIBS"


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
      sndfile (WAV, AIFF, AU, etc.)       :  $sndfile
      Free Lossless Audio Codec (FLAC)    :  $flac
      Ogg Vorbis                          :  $ogg
      Ogg Speex                           :  $speex
      MPEG Audio (MPEG 1-2.5 Layer I-III) :  $mpeg
      MOD Audio (MOD, S3M, XM, IT, etc.)  :  $mod
      Musepack                            :  $mpc
      Monkey's Audio Codec                :  $mac
      WavPack                             :  $wavpack
      LAVC (AC3, AAC, WavPack, WMA, etc.) :  $lavc

  Encoding support:
      sndfile (WAV)                       :  $sndfile
      Free Lossless Audio Codec (FLAC)    :  $flac
      Ogg Vorbis                          :  $vorbisenc
      LAME (MP3)                          :  $lame

  Output driver support:
      sndio Audio                         :  $with_sndio
      OSS Audio                           :  $with_oss
      ALSA Audio                          :  $with_alsa
      JACK Audio Server                   :  $with_jack
      PulseAudio                          :  $with_pulse
      Win32 Sound API                     :  $with_winmm

---------------------------------------------------------------------------])
AC_OUTPUT
