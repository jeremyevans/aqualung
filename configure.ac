# Process this file with autoconf to produce a configure script.

AC_INIT(aqualung, 0.9beta3, tszilagyi@users.sourceforge.net)
AM_INIT_AUTOMAKE(aqualung, 0.9beta3)
AC_CONFIG_SRCDIR([src/core.c])
AM_CONFIG_HEADER([config.h])



# Checks for programs.
AC_PROG_CC


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
AC_MSG_CHECKING(whether GTK+ version >= 2.0)
if pkg-config --exists 'gtk+-2.0 >= 2.0'; then
        AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(GTK+ not found or too old (version < 2.0))
fi


AC_CHECK_PROG([var], [xml2-config], [yes], [no])
if test ! "$var" = "yes"; then
	AC_MSG_ERROR(You do not appear to have xml2-config installed. This usually means that the libxml2 library is missing or not properly installed on your system. Get libxml2 from http://xmlsoft.org)
fi


AC_CHECK_LIB(xml2,
	xmlNewDoc,
	[],
	[AC_MSG_ERROR(You do not appear to have libxml2 installed. Grab it from http://xmlsoft.org)])


AC_CHECK_LIB(jack,
	jack_client_new,
	[],
	[AC_MSG_ERROR(You do not appear to have the JACK library installed. Grab it from http://jackit.sf.net)])


AC_CHECK_LIB(pthread,
	pthread_create,
	[],
	[AC_MSG_ERROR(You do not appear to have a usable pthread library)])


AC_MSG_CHECKING(whether liblrdf version >= 0.3.7)
if pkg-config --exists 'lrdf >= 0.3.7'; then
        AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
	AC_MSG_ERROR(LRDF not found or too old (version < 0.3.7))
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
	oss="yes")
if test "$oss" = "yes"; then
	AC_CHECK_HEADER([linux/soundcard.h],
			[AC_DEFINE([HAVE_OSS], [1], [Defined if compile with OSS support])],
			[AC_MSG_ERROR(You do not appear to have linux/soundcard.h needed by OSS sound support.)])
else
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING(for ALSA support)
AC_ARG_WITH(
	alsa,
	[  --with-alsa=yes,no      compile with ALSA support (default: yes)],
	alsa="$withval",
	alsa="yes")
if test "$alsa" = "yes"; then
	AC_CHECK_LIB(asound,
		snd_pcm_open,
		[],
		[AC_MSG_ERROR(You do not appear to have the ALSA library installed)])
        alsa_CFLAGS=`pkg-config --cflags alsa`
        alsa_LIBS=`pkg-config --libs alsa`
	AC_DEFINE([HAVE_ALSA], [1], [Defined if compile with ALSA support])
else
        alsa_CFLAGS=""
        alsa_LIBS=""
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING(for Sample Rate Converter support)
AC_ARG_WITH(
	src,
	[  --with-src=yes,no       compile with Sample Rate Converter support (default: yes)],
	src="$withval",
	src="yes")
if test "$src" = "yes"; then

	AC_CHECK_LIB(samplerate,
		src_new,
		[],
		[AC_MSG_ERROR(You do not appear to have libsamplerate (aka. Secret Rabbit Code) installed. Grab it from  http://www.mega-nerd.com/SRC/ )])
        src_LIBS=`pkg-config --libs samplerate`
	AC_DEFINE([HAVE_SRC], [1], [Defined if compile with Sample Rate Converter support])
else
        src_LIBS=""
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING(for sndfile support)
AC_ARG_WITH(
	sndfile,
	[  --with-sndfile=yes,no   compile with sndfile (WAV, AIFF, etc.) support (default: yes)],
	sndfile="$withval",
	sndfile="yes")
if test "$sndfile" = "yes"; then
        AC_CHECK_LIB(sndfile,
		sf_open,
		[],
		[AC_MSG_ERROR(You do not appear to have the sndfile library installed. Grab it from  http://www.mega-nerd.com/libsndfile/ )])
	sndfile_LIBS=`pkg-config --libs sndfile`
	AC_DEFINE([HAVE_SNDFILE], [1], [Defined if compile with sndfile (WAV, AIFF, etc.) support])
else
	sndfile_LIBS=""
	AC_MSG_RESULT(no)
fi



AC_MSG_CHECKING(for FLAC support)
AC_ARG_WITH(
	flac,
	[  --with-flac=yes,no      compile with FLAC support (default: yes)],
	flac="$withval",
	flac="yes")
if test "$flac" = "yes"; then
	AC_CHECK_LIB(FLAC,
		FLAC__file_decoder_new,
		[],
		[AC_MSG_ERROR(You do not appear to have the FLAC library installed. Grab it from http://flac.sourceforge.net)])
        flac_LIBS="-lFLAC"
	AC_DEFINE([HAVE_FLAC], [1], [Defined if compile with FLAC support])
else
        flac_LIBS=""
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING(for Ogg Vorbis support)
AC_ARG_WITH(
	ogg,
	[  --with-ogg=yes,no       compile with Ogg Vorbis support (default: yes)],
	ogg="$withval",
	ogg="yes")
if test "$ogg" = "yes"; then
	AC_CHECK_LIB(vorbis,
		ov_open,
		[],
		[AC_MSG_ERROR(You do not appear to have the vorbis and vorbisfile libraries installed. Grab them from http://www.xiph.org/ogg/vorbis)],
		[-lvorbisfile])
        ogg_LIBS="-lvorbis -lvorbisfile"
	AC_DEFINE([HAVE_OGG_VORBIS], [1], [Defined if compile with Ogg Vorbis support])
else
        ogg_LIBS=""
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING(for MPEG audio support)
AC_ARG_WITH(
	mpeg,
	[  --with-mpeg=yes,no      compile with MPEG Audio support (default: yes)],
	mpeg="$withval",
	mpeg="yes")
if test "$mpeg" = "yes"; then
	AC_CHECK_LIB(mad,
		mad_decoder_init,
		[],
		[AC_MSG_ERROR(You do not appear to have the MAD library installed. Grab it from http://www.underbit.com/products/mad/)])
        mad_LIBS="-lmad"
	AC_DEFINE([HAVE_MPEG], [1], [Defined if compile with MPEG Audio support])
else
        mad_LIBS=""
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING(for MOD audio support)
AC_ARG_WITH(
	mod,
	[  --with-mod=yes,no       compile with MOD Audio support (default: yes)],
	mod="$withval",
	mod="yes")
if test "$mod" = "yes"; then
	AC_CHECK_LIB(modplug,
		ModPlug_Load,
		[],
		[AC_MSG_ERROR(You do not appear to have the libmodplug library installed. Grab it from http://modplug-xmms.sourceforge.net/)],
		[-lstdc++ -lm])
        mod_LIBS=`pkg-config --libs libmodplug`
	AC_DEFINE([HAVE_MOD], [1], [Defined if compile with MOD Audio support])
else
        mod_LIBS=""
	AC_MSG_RESULT(no)
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

CFLAGS="-O3 -Wall $SKINDIR"
CPPFLAGS="$gtk_CFLAGS $glib_CFLAGS $xml_CFLAGS $alsa_CFLAGS"
LIBS="$gtk_LIBS $glib_LIBS $xml_LIBS $jack_LIBS $lrdf_LIBS $src_LIBS $alsa_LIBS $sndfile_LIBS $flac_LIBS $ogg_LIBS $mad_LIBS $mod_LIBS"

AC_CONFIG_FILES([Makefile \
		 doc/Makefile \
		 doc/html/Makefile \
		 skin/Makefile \
		 src/Makefile])

AC_OUTPUT

echo "----------------------------------------------------------------------"
echo "  Configuration summary"
echo "  ====================="
echo
echo "  File format support:"
echo "      sndfile (WAV, AIFF, etc.)           :  $sndfile"
echo "      Free Lossless Audio Codec (FLAC)    :  $flac"
echo "      Ogg Vorbis                          :  $ogg"
echo "      MPEG Audio (MPEG 1-2.5 Layer I-III) :  $mpeg"
echo "      MOD Audio (MOD, S3M, XM, IT, etc.)  :  $mod"
echo
echo "  Output driver support:"
echo "      OSS Audio                           :  $oss"
echo "      ALSA Audio                          :  $alsa"
echo "      JACK Audio Server                   :  yes (always)"
echo
echo "  Internal Sample Rate Converter support  :  $src"
echo "----------------------------------------------------------------------"
