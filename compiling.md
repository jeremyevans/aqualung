---
layout: page
title: Compiling
permalink: /compiling/
weight: 30
---

<p>Here's how to do it. It can be tough, but if you follow the
instructions it shouldn't be a big problem. However, if you have never
built packages from source before, this is probably not the time to
learn. Aqualung depends on quite a few libraries and installing them
the right way before attempting to compile Aqualung is an earnest of
success.</p>

<p>Aqualung uses the GNU autotools (autoconf, automake, autoheader),
make, and relies on <a href="http://pkg-config.freedesktop.org/wiki/">pkg-config</a> to
gain information about your system and installed libraries needed to
setup a working build environment. So, it is the usual <tt>
./configure</tt>,<tt> make</tt>,<tt> make install </tt>stuff, with a
few specialized points.</p>

<p>Part of Aqualung's functionality is optionally built. That means
you can build Aqualung without certain features if you don't need
them, and in such cases you won't need the libraries supporting that
particular functionality either. When running<tt> ./configure </tt>
without options, the script will detect the libraries present on the
system and try to adapt the configuration accordingly. If an optional
library is missing, the feature provided by that library will not be
compiled in Aqualung. The absence of mandatory libs will cause hard
errors.</p>

<p>You can use the appropriate<tt> --without-foo </tt>option from the
table if you want to turn something off that would otherwise be
automatically detected and compiled in.  Conversely, use<tt>
--with-foo </tt>to make sure that the absence of the corresponding
optional library causes a hard error and doesn't get through
unnoticed.</p>

<p>Upon successful completion of the configure step, the configuration
is printed in tabular form. You can also access this info later by
looking in the About box or typing<tt> aqualung --version</tt>.</p>

<p>To get a list of all the valid configuration options, type<tt>
./configure --help</tt>.</p>


  <table rules="all" border="1" cellpadding="5" cellspacing="1">
    <tbody><tr>
      <th colspan="4">Mandatory packages</th>
    </tr>
    <tr>
      <th>package</th>
      <th>provided functionality</th>
      <th>where to get it</th>
      <th>how to turn it off</th>
    </tr>
    <tr>
      <td>GTK+-3.0</td>
      <td>Graphical user interface, requires version 3.24</td>
      <td>
	    <a href="http://www.gtk.org/">http://www.gtk.org</a>
      </td>
      <td>
    </td></tr>
    <tr>
      <td>libXML2</td>
      <td>XML parser library</td>
      <td>
	<a href="http://xmlsoft.org/">http://xmlsoft.org</a>
      </td>
      <td>
    </td></tr>
    <tr>
      <td colspan="4" style="border-left: hidden; border-right: hidden">
	<br>
      </td>
    </tr>
    <tr>
      <th colspan="4">Optional packages providing optional features</th>
    </tr>
    <tr>
      <th>package</th>
      <th>provided functionality</th>
      <th>where to get it</th>
      <th>how to turn it off</th>
    </tr>
    <tr>
      <td>liblrdf</td>
      <td>A library for manipulating RDF files about LADSPA plugins</td>
      <td><a href="http://sourceforge.net/projects/lrdf">http://sourceforge.net/projects/lrdf</a></td>
      <td><tt>--without-ladspa</tt></td>
    </tr>
    <tr>
      <td>libcdio</td>
      <td>Library to read Audio CD</td>
      <td><a href="http://www.gnu.org/software/libcdio">http://www.gnu.org/software/libcdio</a></td>
      <td><tt>--without-cdda</tt></td>
    </tr>
    <tr>
      <td>libcddb</td>
      <td>Library to access CDDB/FreeDB server</td>
      <td><a href="https://libcddb.sf.net">https://libcddb.sf.net</a></td>
      <td><tt>--without-cddb</tt></td>
    </tr>
    <tr>
      <td>libsamplerate</td>
      <td>Library for high quality Sample Rate Conversion</td>
      <td><a href="http://www.mega-nerd.com/SRC">http://www.mega-nerd.com/SRC</a></td>
      <td><tt>--without-src</tt></td>
    </tr>
    <tr>
      <td>libifp</td>
      <td>Library to support iRiver iFP driver</td>
      <td><a href="http://sourceforge.net/projects/ifp-driver">http://sourceforge.net/projects/ifp-driver</a></td>
      <td><tt>--without-ifp</tt></td>
    </tr>
    <tr>
      <td></td>
      <td>Podcast support</td>
      <td></td>
      <td><tt>--without-podcast</tt></td>
    </tr>
    <tr>
      <td>Lua ≥ 5.1</td>
      <td>Lua (scripting extensions) support</td>
      <td><a href="http://www.lua.org/">http://www.lua.org</a></td>
      <td><tt>--without-lua</tt></td>
    </tr>
    <tr>
      <td colspan="4" style="border-left: hidden; border-right: hidden">
	<br>
      </td>
    </tr>
    <tr>
      <th colspan="4">Optional packages providing decoding/encoding support</th>
    </tr>
    <tr>
      <th>package</th>
      <th>provided functionality</th>
      <th>where to get it</th>
      <th>how to turn it off</th>
    </tr>
    <tr>
      <td>libsndfile</td>
      <td>Library to decode uncompressed file formats (WAV, AIFF, AU, etc.)
      and encode WAV audio files</td>
      <td><a href="http://www.mega-nerd.com/libsndfile">http://www.mega-nerd.com/libsndfile</a></td>
      <td><tt>--without-sndfile</tt></td>
    </tr>
    <tr>
      <td>libFLAC</td>
      <td>Library to decode and encode FLAC audio files</td>
      <td><a href="https://xiph.org/flac/">https://xiph.org/flac</a></td>
      <td><tt>--without-flac</tt></td>
    </tr>
    <tr>
      <td>libvorbis<br>libvorbisfile</td>
      <td>Library to decode Ogg Vorbis audio files</td>
      <td><a href="https://xiph.org/vorbis/">https://xiph.org/vorbis</a></td>
      <td><tt>--without-ogg</tt></td>
    </tr>
    <tr>
      <td>libvorbis<br>libvorbisenc</td>
      <td>Library to encode Ogg Vorbis audio files</td>
      <td><a href="https://xiph.org/vorbis/">https://xiph.org/vorbis</a></td>
      <td><code>--without-vorbisenc</code></td>
    </tr>
    <tr>
      <td>liboggz<br>libspeex</td>
      <td>Library to decode Ogg Speex audio files</td>
      <td><a href="http://speex.org/">http://speex.org</a></td>
      <td><tt>--without-speex</tt></td>
    </tr>
    <tr>
      <td>libmad</td>
      <td>Library to decode MPEG Audio files (MP3 and friends)</td>
      <td><a href="http://www.underbit.com/products/mad">http://www.underbit.com/products/mad</a></td>
      <td><tt>--without-mpeg</tt></td>
    </tr>
    <tr>
      <td>libmp3lame</td>
      <td>Library to encode MP3 audio files</td>
      <td><a href="http://lame.sourceforge.net/">http://lame.sourceforge.net</a></td>
      <td><tt>--without-lame</tt></td>
    </tr>
    <tr>
      <td>libmodplug</td>
      <td>Library to decode MOD files (MOD, S3M, XM, IT, etc.)</td>
      <td><a href="http://modplug-xmms.sourceforge.net/">http://modplug-xmms.sourceforge.net</a></td>
      <td><tt>--without-mod</tt></td>
    </tr>
    <tr>
      <td>libmpcdec</td>
      <td>Library to decode Musepack files</td>
      <td><a href="http://www.musepack.net/">http://www.musepack.net</a></td>
      <td><tt>--without-mpc</tt></td>
    </tr>
    <tr>
      <td>libmac</td>
      <td>Library to decode Monkey's Audio files</td>
      <td><a href="https://www.monkeysaudio.com/">https://www.monkeysaudio.com</a></td>
      <td><tt>--without-mac</tt></td>
    </tr>
    <tr>
      <td>libwavpack</td>
      <td>Library to decode WavPack files</td>
      <td><a href="http://www.wavpack.com/">http://www.wavpack.com</a></td>
      <td><tt>--without-wavpack</tt></td>
    </tr>
    <tr>
      <td>FFmpeg</td>
      <td>Numerous audio/video codecs including AC3, AAC, WMA, WavPack 
and movie formats.
      If you don't want to compile this from source, recent distro 
packages of libavcodec, libavformat and libavutil should be OK.</td>
      <td><a href="http://ffmpeg.mplayerhq.hu/">http://ffmpeg.mplayerhq.hu</a></td>
      <td><tt>--without-lavc</tt></td>
    </tr>
    <tr>
      <td colspan="4" style="border-left: hidden; border-right: hidden">
	<br>
      </td>
    </tr>
    <tr>
      <th colspan="4">Optional packages providing output driver support</th>
    </tr>
    <tr>
      <th>package</th>
      <th>provided functionality</th>
      <th>where to get it</th>
      <th>how to turn it off</th>
    </tr>
    <tr>
      <td>sndio.h</td>
      <td>sndio output support</td>
      <td></td>
      <td><tt>--without-sndio</tt></td>
    </tr>
    <tr>
      <td>soundcard.h</td>
      <td>OSS sound support</td>
      <td></td>
      <td><tt>--without-oss</tt></td>
    </tr>
    <tr>
      <td>libasound</td>
      <td>ALSA sound library</td>
      <td></td>
      <td><tt>--without-alsa</tt></td>
    </tr>
    <tr>
      <td>JACK</td>
      <td>JACK Audio Connection Kit</td>
      <td><a href="https://jackaudio.org/">https://jackaudio.org</a></td>
      <td><tt>--without-jack</tt></td>
    </tr>
    <tr>
      <td>libpulse-simple</td>
      <td>PulseAudio</td>
      <td></td>
      <td><tt>--without-pulse</tt></td>
    </tr>
  </tbody></table>

<p>Note that you can turn off any and all input format support
libraries – naturally, you should not turn them off at once, or
the program will be completely useless. The same applies for output
drivers.</p>

<h2>Debug build</h2>

<p>The configure option <tt>--enable-debug</tt> builds Aqualung with
debugging support enabled (<tt>gcc -rdynamic -ggdb -g -O0</tt>).
Furthermore, a signal handler is installed which runs when the program
receives a SEGV, FPE, ILL, BUS or ABRT signal, and prints useful debug
info (most notably a stack trace). If you get such a report, please
email it to the mailing list along with a short description of what
you were doing when the program crashed. Please also send the output
of `<tt>aqualung -v</tt>'.</p>

<h2>Platform-specific notes</h2>

<h3>FreeBSD, OpenBSD</h3>

<p>Aqualung compiles under FreeBSD and OpenBSD (and perhaps other BSDs too, but
this is untested) without much hassle. However, the LDFLAGS
environment variable has to be set in the shell before running <tt>
./configure</tt>, or else no libraries will be found. For example:</p>

<div class="indent">
  <div class="codebox">
    <code>setenv LDFLAGS -L/usr/local/lib; ./configure</code>
  </div>
</div>

<p>has to be issued instead of plain <tt> ./configure.</tt></p>

<h3>Microsoft Windows</h3>

<p>Aqualung compiles and runs on Microsoft Windows natively or using
the <a href="http://cygwin.com/">Cygwin</a> UNIX emulation layer. If
you use the native version (strongly recommended), all you have to do
is download the installer and proceed through the steps of the setup
wizard.</p>

<p>Aqualung builds out of the box in a standard Cygwin
shell. Naturally, you have to install relevant dependency libs (with
their respective development packages) before compiling, just like on
UNIX. Some libs may not be part of the Cygwin distribution; you'll
have to build and install them from source.</p>

<p>Please note that running Aqualung on Windows (esp. under Cygwin)
may suffer from stability and usability problems. While we are
striving to provide stable and easily usable software on all
platforms, keep in mind that this is still experimental.</p>

<h1>Installing Aqualung</h1>

<p><tt>make install </tt> will put the executable, the manpage, the
icons and the themes in the right places. What the
right places are depends on your system, and can be set manually by
using standard <tt> ./configure </tt> options, most notably <tt>
--prefix</tt>. If you are not familiar with this, <tt> ./configure
--help </tt> is your friend.</p>
