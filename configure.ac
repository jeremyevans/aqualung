# Process this file with autoconf to produce a configure script.

AC_INIT(aqualung, 0.9beta5, http://aqualung.sf.net/mantis)
AM_INIT_AUTOMAKE(aqualung, 0.9beta5)
AC_CONFIG_SRCDIR([src/core.c])
AM_CONFIG_HEADER([config.h])



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
AC_CHECK_FUNCS([floor memset mkdir strdup strrchr strstr])



AC_CHECK_PROG([var], [pkg-config], [yes], [no])
if test ! "$var" = "yes"; then
	AC_MSG_ERROR(You do not appear to have pkg-config installed)
fi



# Checks for libraries.
AC_MSG_CHECKING(whether GTK+ version >= 2.4)
if pkg-config --exists 'gtk+-2.0 >= 2.4'; then
        AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(GTK+ not found or too old (version < 2.4))
fi


AC_CHECK_PROG([var], [xml2-config], [yes], [no])
if test ! "$var" = "yes"; then
	AC_MSG_ERROR(You do not appear to have xml2-config installed. This usually means that the libxml2 library is missing or not properly installed on your system. Get libxml2 from http://xmlsoft.org)
fi


AC_CHECK_LIB(xml2,
	xmlNewDoc,
	[],
	[AC_MSG_ERROR(You do not appear to have libxml2 installed. Grab it from http://xmlsoft.org)])


AC_MSG_CHECKING(jack development library)
if pkg-config --exists 'jack'; then
        AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(You do not appear to have libjack (the JACK library) installed. Grab it from http://jackit.sf.net)
fi

AC_CHECK_LIB(jack,
	jack_client_new,
	[],
	[AC_MSG_ERROR(You do not appear to have JACK installed. Grab it from http://jackit.sf.net)])


AC_CHECK_LIB(pthread,
	pthread_create,
	[],
	[AC_MSG_ERROR(You do not appear to have a usable pthread library)])


AC_MSG_CHECKING(whether liblrdf version >= 0.4.0)
if pkg-config --exists 'lrdf >= 0.4.0'; then
        AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(LRDF not found or too old (version < 0.4.0))
fi

AC_CHECK_LIB(lrdf,
	lrdf_init,
	[],
	[AC_MSG_ERROR([You do not appear to have the LRDF library installed. Grab it from http://plugin.org.uk/releases/lrdf])])


AC_CHECK_LIB(dl,
	dlopen,
	[],
	[AC_MSG_ERROR(You do not appear to have a usable interface to the dynamic linking loader.)])



AC_MSG_CHECKING(for OSS support)
AC_ARG_WITH(
	oss,
	[  --with-oss=yes,no       compile with OSS support (default: yes)],
	oss="$withval",
	oss="detect")
if test "$oss" = no; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_HEADER([linux/soundcard.h], [hdr=yes], [hdr=no])
	if test "$hdr" = "yes"; then
		AC_DEFINE([HAVE_OSS], [1], [Defined if compile with OSS support])
	fi
	if test "$hdr" = "no" -a "$oss" = "yes"; then
		AC_MSG_ERROR(You do not appear to have linux/soundcard.h needed by OSS sound support.)
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
	AC_CHECK_LIB(FLAC, FLAC__file_decoder_new, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
	        flac_LIBS="-lFLAC"
		AC_DEFINE([HAVE_FLAC], [1], [Defined if compile with FLAC support])
	fi
	if test "$lib" = "no" -a "$flac" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the FLAC library installed. Grab it from http://flac.sourceforge.net)
	fi
	if test "$flac" = "detect"; then
		flac=$lib
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
	AC_CHECK_LIB(vorbis, ov_open, [lib=yes], [lib=no], [-lvorbisfile])
	if test "$lib" = "yes"; then
		ogg_LIBS="-lvorbis -lvorbisfile"
		AC_DEFINE([HAVE_OGG_VORBIS], [1], [Defined if compile with Ogg Vorbis support])
	fi
	if test "$lib" = "no" -a "$ogg" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the vorbis and vorbisfile libraries installed. Grab them from http://www.xiph.org/ogg/vorbis)
	fi
	if test "$ogg" = "detect"; then
		ogg=$lib
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
	AC_CHECK_LIB(oggz, oggz_open, [oggzlib=yes], [oggzlib=no], [-loggz])
	AC_CHECK_LIB(speex, speex_bits_init, [speexlib=yes], [speexlib=no], [-lspeex])
	if test "$oggzlib" = "yes"; then
		if test "$speexlib" = "yes"; then
			speex_LIBS="-loggz -lspeex"
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
	AC_CHECK_LIB(mad, mad_decoder_init, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
	        mad_LIBS="-lmad"
		AC_DEFINE([HAVE_MPEG], [1], [Defined if compile with MPEG Audio support])
	fi
	if test "$lib" = "no" -a "$mpeg" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the MAD library installed. Grab it from http://www.underbit.com/products/mad/)
	fi
	if test "$mpeg" = "detect"; then
		mpeg=$lib
	fi
fi


AC_MSG_CHECKING(for ID3 tags support)
AC_ARG_WITH(
	id3,
	[  --with-id3=yes,no       compile with ID3 tags support (default: yes)],
	id3="$withval",
	id3="detect")
if test "$id3" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(id3tag, id3_tag_parse, [lib=yes], [lib=no])
	if test "$lib" = "yes"; then
	        id3_LIBS="-lid3tag"
		AC_DEFINE([HAVE_ID3], [1], [Defined if compile with ID3 tags support])
	fi
	if test "$lib" = "no" -a "$id3" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the id3tag library installed. Grab it from http://www.underbit.com/products/mad/)
	fi
	if test "$id3" = "detect"; then
		id3=$lib
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
		AC_MSG_ERROR(You do not appear to have the Monkey's Audio Codec decoder library installed. Grab it from http://sourceforge.net/projects/mac-port/)
	fi
	if test "$mac" = "detect"; then
		mac=$lib
	fi
fi


AC_MSG_CHECKING(for CDDB support)
AC_ARG_WITH(
	cddb,
	[  --with-cddb=yes,no       compile with CDDB support (default: yes)],
	cddb="$withval",
	cddb="detect")
if test "$cddb" = "no"; then
	AC_MSG_RESULT(no)
else
	AC_CHECK_LIB(cddb, cddb_new, [lib=yes], [lib=no], [-lstdc++])
	if test "$lib" = "yes"; then
	        cddb_LIBS=`pkg-config --libs libcddb`
		AC_DEFINE([HAVE_CDDB], [1], [Defined if compile with CDDB support])
	fi
	if test "$lib" = "no" -a "$cddb" = "yes"; then
		AC_MSG_ERROR(You do not appear to have the CDDB library installed. Grab it from http://sourceforge.net/projects/libcddb/)
	fi
	if test "$cddb" = "detect"; then
		cddb=$lib
	fi
fi


# Compiler and linker variables
gtk_CFLAGS=`pkg-config --cflags gtk+-2.0`
gtk_LIBS=`pkg-config --libs gtk+-2.0`

glib_CFLAGS=`pkg-config --cflags gthread-2.0`
glib_LIBS=`pkg-config --libs gthread-2.0`

xml_CFLAGS=`xml2-config --cflags`
xml_LIBS=`xml2-config --libs`

jack_LIBS=`pkg-config --libs jack`

lrdf_LIBS=`pkg-config --libs lrdf`

SKINDIR="-DSKINDIR=\\\"$datadir/aqualung/skin\\\""
LOCALEDIR="-DLOCALEDIR=\\\"$datadir/locale\\\""
DATADIR="-DDATADIR=\\\"$datadir/aqualung\\\""

CFLAGS="-g -O0 -Wall $SKINDIR $LOCALEDIR $DATADIR -D_GNU_SOURCE"
CPPFLAGS="$gtk_CFLAGS $glib_CFLAGS $xml_CFLAGS $alsa_CFLAGS"
LIBS="decoder/libdecoder.a $gtk_LIBS $glib_LIBS $xml_LIBS $jack_LIBS $lrdf_LIBS $src_LIBS $alsa_LIBS $sndfile_LIBS $flac_LIBS $ogg_LIBS $speex_LIBS $mad_LIBS $id3_LIBS $mod_LIBS $mpc_LIBS $mac_LIBS $cddb_LIBS"


AC_OUTPUT([Makefile \
	   doc/Makefile \
	   doc/html/Makefile \
	   skin/Makefile \
	   skin/dark/Makefile \
	   skin/default/Makefile \
	   skin/metal/Makefile \
	   skin/ocean/Makefile \
	   skin/plain/Makefile \
	   skin/woody/Makefile \
	   src/Makefile \
	   src/decoder/Makefile \
	   src/img/Makefile \
	   src/po/Makefile])


echo "----------------------------------------------------------------------"
echo "  Configuration summary"
echo "  ====================="
echo
echo "  File format support:"
echo "      sndfile (WAV, AIFF, etc.)           :  $sndfile"
echo "      Free Lossless Audio Codec (FLAC)    :  $flac"
echo "      Ogg Vorbis                          :  $ogg"
echo "      Ogg Speex                           :  $speex"
echo "      MPEG Audio (MPEG 1-2.5 Layer I-III) :  $mpeg"
echo "      MOD Audio (MOD, S3M, XM, IT, etc.)  :  $mod"
echo "      Musepack                            :  $mpc"
echo "      Monkey's Audio Codec                :  $mac"
echo "      ID3 tags                            :  $id3"
echo
echo "  Output driver support:"
echo "      OSS Audio                           :  $oss"
echo "      ALSA Audio                          :  $alsa"
echo "      JACK Audio Server                   :  yes (always)"
echo
echo "  Internal Sample Rate Converter support  :  $src"
echo
echo "  CDDB support                            :  $cddb"
echo "----------------------------------------------------------------------"
