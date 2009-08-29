# Process this file with autoconf to produce a configure script.

AC_INIT(aqualung, SVN, http://aqualung.factorial.hu/mantis)
AM_INIT_AUTOMAKE(aqualung, SVN)
AC_CONFIG_SRCDIR([src/core.c])
AM_CONFIG_HEADER([config.h])



AC_MSG_CHECKING(for type of build)
AC_ARG_ENABLE(
	debug,
	[  --enable-debug=yes,no   compile with debugging support (default: no)],
	debug="$enableval",
	debug="no")
if test "$debug" = "yes"; then
	buildtype=debug
	BUILD_CFLAGS="-rdynamic -ggdb -g -O0"
	AC_DEFINE([DEBUG_BUILD], [1], [Defined if doing a debug build])
	AC_MSG_RESULT(debug)
else
	buildtype=release
	BUILD_CFLAGS="-O2"
	AC_DEFINE([RELEASE_BUILD], [1], [Defined if doing a release build])
	AC_MSG_RESULT(release)
fi


AC_MSG_CHECKING(for compilation platform)
if test `uname -a | grep CYGWIN | wc -l` = "1" ; then
   PLATFORM_CFLAGS="-mwin32 -mwindows"
   PLATFORM_LIBS="-lintl -lwinmm"
   win32="yes"
   freebsd="no"
   openbsd="no"
   linux="no"
   if test "$buildtype" = "debug"; then
	   AC_MSG_ERROR([Sorry, debug build under Cygwin is unsupported!])
   fi
   platform="Cygwin"
   AC_MSG_RESULT(Cygwin)
fi
if test `uname -a | grep FreeBSD | wc -l` = "1"  ; then
   PLATFORM_CFLAGS=""
   PLATFORM_LIBS=""
   win32="no"
   freebsd="yes"
   openbsd="no"
   linux="no"
   if test "$buildtype" = "debug"; then
	   AC_MSG_ERROR([Sorry, debug build under FreeBSD is unsupported!])
   fi
   platform="FreeBSD"
   AC_MSG_RESULT(FreeBSD)
fi
if test `uname -a | grep OpenBSD | wc -l` = "1"  ; then
   PLATFORM_CFLAGS=""
   PLATFORM_LIBS=""
   win32="no"
   freebsd="no"
   openbsd="yes"
   linux="no"
   if test "$buildtype" = "debug"; then
	   AC_MSG_ERROR([Sorry, debug build under OpenBSD is unsupported!])
   fi
   platform="OpenBSD"
   AC_MSG_RESULT(OpenBSD)
fi
if test `uname -a | grep Linux | wc -l` = "1"  ; then
   PLATFORM_CFLAGS=""
   PLATFORM_LIBS=""
   win32="no"
   freebsd="no"
   openbsd="no"
   linux="yes"
   platform="Linux"
   AC_MSG_RESULT(Linux)
fi



# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_CXX


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h fcntl.h stdlib.h string.h sys/ioctl.h unistd.h dlfcn.h])



# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T



# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_CHECK_FUNCS([floor memset mkdir strdup strrchr strstr strndup strcasestr])



AC_CHECK_PROG([var], [pkg-config], [yes], [no])
if test ! "$var" = "yes"; then
	AC_MSG_ERROR(You do not appear to have pkg-config installed)
fi



if test $win32 = "no" ; then
AC_CHECK_LIB(pthread,
	pthread_create,
	[],
	[AC_MSG_ERROR(You do not appear to have a usable pthread library)])
fi

if test $win32 = "no" -a $freebsd = "no" -a $openbsd = "no" ; then
AC_CHECK_LIB(dl,
	dlopen,
	[],
	[AC_MSG_ERROR(You do not appear to have a usable interface to the dynamic linking loader.)])
fi

z_LIBS=""
AC_CHECK_LIB(z,	gzopen, [lib=yes], [lib=no])
if test "$lib" = "yes"; then
    z_LIBS="-lz"
    AC_DEFINE([HAVE_LIBZ], [1], [Defined if compile with libz])
fi

bz2_LIBS=""
AC_CHECK_LIB(bz2, BZ2_bzopen, [lib=yes], [lib=no])
if test "$lib" = "yes"; then
    bz2_LIBS="-lbz2"
    AC_DEFINE([HAVE_LIBBZ2], [1], [Defined if compile with libbz2])
fi

# Checks for libraries.
AC_MSG_CHECKING(whether GTK+ version >= 2.6)
if pkg-config --exists 'gtk+-2.0 >= 2.6'; then

        gtk_CFLAGS=`pkg-config --cflags gtk+-2.0`
	gtk_LIBS=`pkg-config --libs gtk+-2.0`

	glib_CFLAGS=`pkg-config --cflags gthread-2.0`
	glib_LIBS=`pkg-config --libs gthread-2.0`

        AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(GTK+ not found or too old (version < 2.6))
fi


AC_CHECK_PROG([var], [xml2-config], [yes], [no])
if test ! "$var" = "yes"; then
	AC_MSG_ERROR(You do not appear to have xml2-config installed. This usually means that the libxml2 library is missing or not properly installed on your system. Get libxml2 from http://xmlsoft.org)
fi

xml_CFLAGS=`xml2-config --cflags`
xml_LIBS=`xml2-config --libs`

AC_CHECK_LIB(xml2,
	xmlNewDoc,
	[],
	[AC_MSG_ERROR(You do not appear to have libxml2 installed. Grab it from http://xmlsoft.org)])


AC_MSG_CHECKING(for sndio support)
AC_ARG_WITH(
	sndio,
	[  --with-sndio=no,yes     compile with sndio support (default: no)],
	sndio="$withval",
	sndio="no")
if test "$oss" = no; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_HEADER([sndio.h], [hdr=yes], [hdr=no])
	if test "$hdr" = "yes"; then
		AC_DEFINE([HAVE_SNDIO], [1], [Defined if compile with sndio support])
		if test $openbsd = "yes" ; then
			AC_CHECK_LIB(sndio, sio_open, [sndio_LIBS="-lsndio"], [sndio_LIBS=""])
		else
		        sndio_LIBS=""
		fi
	fi
	if test "$hdr" = "no" -a "$sndio" = "yes"; then
			AC_MSG_ERROR(You do not appear to have sndio.h needed by sndio sound support.)
  fi
fi

AC_MSG_CHECKING(for OSS support)
AC_ARG_WITH(
	oss,
	[  --with-oss=yes,no       compile with OSS support (default: yes)],
	oss="$withval",
	oss="detect")
if test "$oss" = no; then
	AC_MSG_RESULT(no)
else
	if test $freebsd = "yes" ; then
	        AC_CHECK_HEADER([sys/soundcard.h], [hdr=yes], [hdr=no])
	elif test $openbsd = "yes" ; then
	        AC_CHECK_HEADER([soundcard.h], [hdr=yes], [hdr=no])
	elif test $linux = "yes" ; then
	        AC_CHECK_HEADER([linux/soundcard.h], [hdr=yes], [hdr=no])
	else
		hdr="no"
		AC_MSG_RESULT(no)
	fi
	if test "$hdr" = "yes"; then
		AC_DEFINE([HAVE_OSS], [1], [Defined if compile with OSS support])
		if test $openbsd = "yes" ; then
			AC_CHECK_LIB(ossaudio, _oss_ioctl, [oss_LIBS="-lossaudio"], [oss_LIBS=""])
		else
		        oss_LIBS=""
		fi
	fi
	if test "$hdr" = "no" -a "$oss" = "yes"; then
	        if test $freebsd = "yes" ; then
		        AC_MSG_ERROR(You do not appear to have sys/soundcard.h needed by OSS sound support.)
		elif test $openbsd = "yes" ; then
			AC_MSG_ERROR(You do not appear to have soundcard.h needed by OSS sound support.)
		elif test $linux = "yes" ; then
		        AC_MSG_ERROR(You do not appear to have linux/soundcard.h needed by OSS sound support.)
		else
		        AC_MSG_ERROR(OSS output is unsupported on current build platform.)
		fi
	fi
	if test "$oss" = "detect"; then
	        oss=$hdr
	fi
fi


AC_MSG_CHECKING(for ALSA support)
AC_ARG_WITH(
	alsa,
	[  --with-alsa=yes,no      compile with ALSA support (default: yes)],
	alsa="$withval",
	alsa="detect")

if test "$alsa" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(asound, snd_pcm_open, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
	        alsa_CFLAGS=`pkg-config --cflags alsa`
		alsa_LIBS=`pkg-config --libs alsa`
		AC_DEFINE([HAVE_ALSA], [1], [Defined if compile with ALSA support])
	fi
	if test "$lib" = "no" -a "$alsa" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the ALSA library installed)
	fi
	if test "$alsa" = "detect"; then
	        alsa=$lib
	fi
fi


AC_MSG_CHECKING(for JACK support)
AC_ARG_WITH(
	jack,
	[  --with-jack=yes,no      compile with JACK support (default: yes)],
	jack="$withval",
	jack="detect")

if test "$jack" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(jack, jack_client_new, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
	        jack_CFLAGS=`pkg-config --cflags jack`
		jack_LIBS=`pkg-config --libs jack`
		AC_DEFINE([HAVE_JACK], [1], [Defined if compile with JACK support])
	fi
	if test "$lib" = "no" -a "$jack" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the JACK library installed. Grab it from http://jackaudio.org)
	fi
	if test "$jack" = "detect"; then
	        jack=$lib
	fi
fi

AC_MSG_CHECKING(for PulseAudio support)
AC_ARG_WITH(
	pulse,
	[  --with-pulse=yes,no     compile with PulseAudio support (default: yes)],
	pulse="$withval",
	pulse="detect")

if test "$pulse" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(pulse-simple, pa_simple_new, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
	        pulse_CFLAGS=`pkg-config --cflags libpulse-simple`
		pulse_LIBS=`pkg-config --libs libpulse-simple`
		AC_DEFINE([HAVE_PULSE], [1], [Defined if compile with PulseAudio support])
	fi
	if test "$lib" = "no" -a "$pulse" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the PulseAudio installed. Grab it from http://pulseaudio.org/)
	fi
	if test "$pulse" = "detect"; then
	        pulse=$lib
	fi
fi

AC_MSG_CHECKING(for Sample Rate Converter support)
AC_ARG_WITH(
	src,
	[  --with-src=yes,no       compile with Sample Rate Converter support (default: yes)],
	src="$withval",
	src="detect")

if test "$src" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(samplerate, src_new, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
	        src_LIBS=`pkg-config --libs samplerate`
		AC_DEFINE([HAVE_SRC], [1], [Defined if compile with Sample Rate Converter support])
	fi
	if test "$lib" = "no" -a "$src" = "yes"; then
	        AC_MSG_ERROR(You do not appear to have libsamplerate (aka. Secret Rabbit Code) installed. Grab it from  http://www.mega-nerd.com/SRC/ )
	fi
	if test "$src" = "detect"; then
	        src=$lib
	fi
fi


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
	AC_CHECK_LIB(mpcdec, mpc_streaminfo_init, [lib=yes], [lib=no], [-lstdc++])
	if test "$lib" = "yes"; then
	        mpc_LIBS="-lmpcdec -lstdc++"
		AC_DEFINE([HAVE_MPC], [1], [Defined if compile with Musepack support])
	fi
	if test "$lib" = "no" -a "$mpc" = "yes"; then
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


AC_MSG_CHECKING(for LADSPA plugin support)
AC_ARG_WITH(
	ladspa,
	[  --with-ladspa=yes,no    compile with LADSPA plugin support (default: yes)],
	ladspa="$withval",
	ladspa="detect")
if test "$ladspa" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(lrdf, lrdf_init, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
		AC_MSG_CHECKING(whether liblrdf version >= 0.4.0)
		if `pkg-config --exists 'lrdf >= 0.4.0'`; then
			AC_MSG_RESULT(yes)
		        lrdf_LIBS=`pkg-config --libs lrdf`
			AC_DEFINE([HAVE_LADSPA], [1], [Defined if compile with LADSPA plugin support])
		else
			AC_MSG_RESULT(no)
			lib="no"
		fi
	fi
	if test "$lib" = "no" -a "$ladspa" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the LRDF library installed (needed for LADSPA support). Grab it from http://sourceforge.net/projects/lrdf/)
	fi
	if test "$ladspa" = "detect"; then
		ladspa=$lib
	fi
fi


AC_MSG_CHECKING(for CDDA support)
AC_ARG_WITH(
	cdda,
	[  --with-cdda=yes,no      compile with CDDA support (default: yes)],
	cdda="$withval",
	cdda="detect")
if test "$cdda" = "no"; then
	AC_MSG_RESULT(no)
else
        if test $win32 = "no" ; then
	        AC_CHECK_LIB(cdio, cdio_open, [lib=yes], [lib=no])
        else
	        AC_CHECK_LIB(cdio, cdio_open, [lib=yes], [lib=no], [-lwinmm])
        fi
	if test "$lib" = "yes"; then
		AC_MSG_CHECKING(whether libcdio_paranoia version >= 0.76)
		if `pkg-config --exists 'libcdio_paranoia >= 0.76'`; then
		        cdda_CFLAGS=`pkg-config --cflags libcdio_paranoia`
		        cdda_LIBS=`pkg-config --libs libcdio_paranoia`
			AC_DEFINE([HAVE_CDDA], [1], [Defined if compile with CDDA support])
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
			lib="no"
		fi
	fi
	if test "$lib" = "no" -a "$cdda" = "yes"; then
		AC_MSG_ERROR(libcdio and/or libcdio_paranoia library not found or too old. Grab it from http://www.gnu.org/software/libcdio/)
	fi
	if test "$cdda" = "detect"; then
		cdda=$lib
	fi
fi


AC_MSG_CHECKING(for CDDB support)
AC_ARG_WITH(
	cddb,
	[  --with-cddb=yes,no      compile with CDDB support (default: yes)],
	cddb="$withval",
	cddb="detect")
if test "$cddb" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(cddb, cddb_new, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
		AC_MSG_CHECKING(whether libcddb version >= 1.2.1)
		if `pkg-config --exists 'libcddb >= 1.2.1'`; then
			AC_MSG_RESULT(yes)
		        cddb_LIBS=`pkg-config --libs libcddb`
			AC_DEFINE([HAVE_CDDB], [1], [Defined if compile with CDDB support])
		        AC_CHECK_LIB(cddb, cddb_disc_get_revision, [libcddb_rev=yes], [libcddb_rev=no])
			if test "$libcddb_rev" = "yes"; then
				AC_DEFINE([LIBCDDB_REVISION], [1], [Defined if libcddb handles disc revision number])
			fi
		else
			AC_MSG_RESULT(no)
			lib="no"
		fi
	fi
	if test "$lib" = "no" -a "$cddb" = "yes"; then
		AC_MSG_ERROR(CDDB library not found or too old (version < 1.2.1). Grab it from http://libcddb.sf.net/)
	fi
	if test "$cddb" = "detect"; then
		cddb=$lib
	fi
fi


AC_MSG_CHECKING(for iRiver iFP support)
AC_ARG_WITH(
	ifp,
	[  --with-ifp=yes,no       compile with iRiver iFP driver support (default: yes)],
	ifp="$withval",
	ifp="detect")
if test "$ifp" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(usb, usb_init, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
        AC_CHECK_LIB(ifp, ifp_find_device, [lib=yes], [lib=no])
        if test "$lib" = "yes"; then
            ifp_LIBS="-lifp -lusb"
            AC_DEFINE([HAVE_IFP], [1], [Defined if compile with iRiver iFP driver support])
        fi
        if test "$lib" = "no" -a "$ifp" = "yes"; then
            AC_MSG_ERROR(You do not appear to have the iFP driver library installed. Grab it from http://ifp-driver.sourceforge.net/)
        fi
	fi
    if test "$ifp" = "detect"; then
            ifp=$lib
    fi
fi


AC_MSG_CHECKING(for Systray support)
AC_ARG_WITH(
	systray,
	[  --with-systray=yes,no   compile with Systray support (default: yes)],
	systray="$withval",
	systray="detect")
if test "$systray" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_MSG_CHECKING(whether GTK+ version >= 2.10)
	if pkg-config --exists 'gtk+-2.0 >= 2.10'; then
	        AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_SYSTRAY], [1], [Defined if version(GTK+-2.0) >= 2.10])
		lib="yes"
	else
		AC_MSG_RESULT(no)
		lib="no"
	fi

	if test "$lib" = "no" -a "$systray" = "yes"; then
		AC_MSG_ERROR(You need GTK+ version 2.10 or higher for systray support.)
	fi
	if test "$systray" = "detect"; then
	        systray=$lib
	fi
fi

AC_MSG_CHECKING(for loop playback support)
AC_ARG_WITH(
	loop,
	[  --with-loop=yes,no      compile with loop playback support (default: yes)],
	loop="$withval",
	loop="detect")
if test "$loop" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_MSG_CHECKING(whether GTK+ version >= 2.8)
	if pkg-config --exists 'gtk+-2.0 >= 2.8'; then
	        AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_LOOP], [1], [Defined if version(GTK+-2.0) >= 2.8])
		lib="yes"
	else
		AC_MSG_RESULT(no)
		lib="no"
	fi

	if test "$lib" = "no" -a "$loop" = "yes"; then
		AC_MSG_ERROR(You need GTK+ version 2.8 or higher for loop playback support.)
	fi
	if test "$loop" = "detect"; then
	        loop=$lib
	fi
fi

AC_MSG_CHECKING(for podcast support)
AC_ARG_WITH(
	podcast,
	[  --with-podcast=yes,no   compile with podcast support (default: yes)],
	podcast="$withval",
	podcast="yes")
if test "$podcast" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_DEFINE([HAVE_PODCAST], [1], [Defined if compile with podcast support])
	AC_MSG_RESULT(yes)
fi

AC_MSG_CHECKING(for Lua support)
AC_ARG_WITH(
	lua,
	[  --with-lua=yes,no       compile with Lua (programmable title formatting) support (default: yes)],
	lua="$withval",
	lua="detect")
if test "$lua" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(lua, lua_load, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
		lua_LIBS="-llua"
		AC_DEFINE([HAVE_LUA], [1], [Defined if compiled with Lua support])
  else
    AC_CHECK_LIB(lua5.1, lua_load, [lib=yes], [lib=no])
	  if test "$lib" = "yes"; then
		  lua_LIBS="-llua5.1"
		  AC_DEFINE([HAVE_LUA], [1], [Defined if compiled with Lua support])
		  AC_DEFINE([LUA_HEADER_lua5_1], [1], [Defined if lua headers detected in a nonstandard location])
    fi
	fi
	if test "$lib" = "no" -a "$lua" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the Lua 5.1 library installed. Grab it from http://www.lua.org/.)
	fi
	if test "$lua" = "detect"; then
		lua=$lib
	fi
fi


# Compiler and linker variables
AQUALUNG_SKINDIR="-DAQUALUNG_SKINDIR=\\\"$datadir/aqualung/skin\\\""
AQUALUNG_LOCALEDIR="-DAQUALUNG_LOCALEDIR=\\\"$datadir/locale\\\""
AQUALUNG_DATADIR="-DAQUALUNG_DATADIR=\\\"$datadir/aqualung\\\""

CFLAGS="$CFLAGS $BUILD_CFLAGS -Wall $PLATFORM_CFLAGS $AQUALUNG_SKINDIR $AQUALUNG_LOCALEDIR $AQUALUNG_DATADIR -D_GNU_SOURCE"
CXXFLAGS="$CFLAGS"
CPPFLAGS="$gtk_CFLAGS $glib_CFLAGS $xml_CFLAGS $alsa_CFLAGS $jack_CFLAGS $cdda_CFLAGS $pulse_CFLAGS"
LIBS="decoder/libdecoder.a encoder/libencoder.a $gtk_LIBS $glib_LIBS $xml_LIBS $jack_LIBS $lrdf_LIBS $src_LIBS $alsa_LIBS $sndio_LIBS $oss_LIBS $sndfile_LIBS $flac_LIBS $ogg_LIBS $wavpack_LIBS $speex_LIBS $mad_LIBS $mod_LIBS $mpc_LIBS $mac_LIBS $lavc_LIBS $vorbisenc_LIBS $lame_LIBS $cdda_LIBS $cddb_LIBS $ifp_LIBS $PLATFORM_LIBS $z_LIBS $bz2_LIBS $lua_LIBS $pulse_LIBS"


AC_OUTPUT([Makefile \
	   doc/Makefile \
	   skin/Makefile \
	   skin/dark/Makefile \
	   skin/default/Makefile \
	   skin/metal/Makefile \
	   skin/ocean/Makefile \
	   skin/plain/Makefile \
	   skin/woody/Makefile \
	   skin/no_skin/Makefile \
	   src/Makefile \
	   src/decoder/Makefile \
	   src/encoder/Makefile \
	   src/img/Makefile \
	   src/po/Makefile])


echo "----------------------------------------------------------------------"
echo "  Configuration summary"
echo "  ====================="
echo
echo "  Build type / target platform            :  $buildtype / $platform"
echo
echo "  Optional features:"
echo "      LADSPA plugin support               :  $ladspa"
echo "      CDDA (Audio CD) support             :  $cdda"
echo "      CDDB support                        :  $cddb"
echo "      Sample Rate Converter support       :  $src"
echo "      iRiver iFP driver support           :  $ifp"
echo "      Loop playback support               :  $loop"
echo "      Systray support                     :  $systray"
echo "      Podcast support                     :  $podcast"
echo "      Lua (prog. title format) support    :  $lua"
echo
echo "  Decoding support:"
echo "      sndfile (WAV, AIFF, AU, etc.)       :  $sndfile"
echo "      Free Lossless Audio Codec (FLAC)    :  $flac"
echo "      Ogg Vorbis                          :  $ogg"
echo "      Ogg Speex                           :  $speex"
echo "      MPEG Audio (MPEG 1-2.5 Layer I-III) :  $mpeg"
echo "      MOD Audio (MOD, S3M, XM, IT, etc.)  :  $mod"
echo "      Musepack                            :  $mpc"
echo "      Monkey's Audio Codec                :  $mac"
echo "      WavPack                             :  $wavpack"
echo "      LAVC (AC3, AAC, WavPack, WMA, etc.) :  $lavc"
echo
echo "  Encoding support:"
echo "      sndfile (WAV)                       :  $sndfile"
echo "      Free Lossless Audio Codec (FLAC)    :  $flac"
echo "      Ogg Vorbis                          :  $vorbisenc"
echo "      LAME (MP3)                          :  $lame"
echo
echo "  Output driver support:"
echo "      sndio Audio                         :  $sndio"
echo "      OSS Audio                           :  $oss"
echo "      ALSA Audio                          :  $alsa"
echo "      JACK Audio Server                   :  $jack"
echo "      PulseAudio                          :  $pulse"
echo "      Win32 Sound API                     :  $win32"
echo
echo "----------------------------------------------------------------------"
