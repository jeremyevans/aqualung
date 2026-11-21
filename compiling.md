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
the right way before attempting to compile Aqualung is a prerequisite
for success.</p>

<p>Aqualung uses the GNU autotools (autoconf, automake, autoheader),
make, and relies on <a href="https://wiki.freedesktop.org/Software/pkg-config/">pkg-config</a> to
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
	    <a href="https://www.gtk.org/">www.gtk.org</a>
      </td>
      <td>
    </td></tr>
    <tr>
      <td>libXML2</td>
      <td>XML parser library</td>
      <td>
	<a href="https://download.gnome.org/sources/libxml2/">download.gnome.org/sources/libxml2/</a>
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
      <td><a href="https://sourceforge.net/projects/lrdf">sourceforge.net/projects/lrdf</a></td>
      <td><tt>--without-ladspa</tt></td>
    </tr>
    <tr>
      <td>libcdio</td>
      <td>Library to read Audio CD</td>
      <td><a href="https://libcdio.github.io/">libcdio.github.io/</a></td>
      <td><tt>--without-cdda</tt></td>
    </tr>
    <tr>
      <td>libcddb</td>
      <td>Library to access CDDB/FreeDB server</td>
      <td><a href="https://libcddb.sf.net">libcddb.sf.net</a></td>
      <td><tt>--without-cddb</tt></td>
    </tr>
    <tr>
      <td>libsamplerate</td>
      <td>Library for high quality Sample Rate Conversion</td>
      <td><a href="https://www.mega-nerd.com/SRC/">www.mega-nerd.com/SRC</a></td>
      <td><tt>--without-src</tt></td>
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
      <td><a href="https://www.lua.org/">www.lua.org</a></td>
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
      <td><a href="https://www.mega-nerd.com/libsndfile/">www.mega-nerd.com/libsndfile/</a></td>
      <td><tt>--without-sndfile</tt></td>
    </tr>
    <tr>
      <td>libFLAC</td>
      <td>Library to decode and encode FLAC audio files</td>
      <td><a href="https://xiph.org/flac/">xiph.org/flac</a></td>
      <td><tt>--without-flac</tt></td>
    </tr>
    <tr>
      <td>libvorbis<br>libvorbisfile</td>
      <td>Library to decode Ogg Vorbis audio files</td>
      <td><a href="https://xiph.org/vorbis/">xiph.org/vorbis</a></td>
      <td><tt>--without-ogg</tt></td>
    </tr>
    <tr>
      <td>libvorbis<br>libvorbisenc</td>
      <td>Library to encode Ogg Vorbis audio files</td>
      <td><a href="https://xiph.org/vorbis/">xiph.org/vorbis</a></td>
      <td><code>--without-vorbisenc</code></td>
    </tr>
    <tr>
      <td>liboggz<br>libspeex</td>
      <td>Library to decode Ogg Speex audio files</td>
      <td><a href="https://speex.org/">speex.org</a></td>
      <td><tt>--without-speex</tt></td>
    </tr>
    <tr>
      <td>libmad</td>
      <td>Library to decode MPEG Audio files (MP3 and friends)</td>
      <td><a href="https://www.underbit.com/products/mad">www.underbit.com/products/mad</a></td>
      <td><tt>--without-mpeg</tt></td>
    </tr>
    <tr>
      <td>libmp3lame</td>
      <td>Library to encode MP3 audio files</td>
      <td><a href="https://lame.sourceforge.io/">lame.sourceforge.io/</a></td>
      <td><tt>--without-lame</tt></td>
    </tr>
    <tr>
      <td>libmodplug</td>
      <td>Library to decode MOD files (MOD, S3M, XM, IT, etc.)</td>
      <td><a href="https://modplug-xmms.sourceforge.net/">modplug-xmms.sourceforge.net</a></td>
      <td><tt>--without-mod</tt></td>
    </tr>
    <tr>
      <td>libmpcdec</td>
      <td>Library to decode Musepack files</td>
      <td><a href="https://www.musepack.net/">www.musepack.net</a></td>
      <td><tt>--without-mpc</tt></td>
    </tr>
    <tr>
      <td>libmac</td>
      <td>Library to decode Monkey's Audio files</td>
      <td><a href="https://www.monkeysaudio.com/">www.monkeysaudio.com</a></td>
      <td><tt>--without-MAC</tt></td>
    </tr>
    <tr>
      <td>libwavpack</td>
      <td>Library to decode WavPack files</td>
      <td><a href="https://www.wavpack.com/">www.wavpack.com</a></td>
      <td><tt>--without-wavpack</tt></td>
    </tr>
    <tr>
      <td>FFmpeg</td>
      <td>Numerous audio/video codecs including AC3, AAC, WMA, WavPack 
and movie formats.
      If you don't want to compile this from source, recent distro 
packages of libavcodec, libavformat and libavutil should be OK.</td>
      <td><a href="https://ffmpeg.org/">ffmpeg.org/</a></td>
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
      <td><a href="https://jackaudio.org/">jackaudio.org</a></td>
      <td><tt>--without-jack</tt></td>
    </tr>
    <tr>
      <td>libpipewire-0.3</td>
      <td>PipeWire</td>
      <td></td>
      <td><tt>--without-pipewire</tt></td>
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

<h3>Notes for Debian users</h3>

<p>If you run Debian, or a distribution based on Debian, you can
install most dependencies easily via the package manager:</p>

<div class="indent">
<div class="codebox">
<pre>
sudo apt-get install \
  build-essential autoconf automake autopoint gettext pkgconf \
  libgtk-3-dev libxml2-dev zlib1g-dev libbz2-dev \
  liblrdf0-dev libcdio-dev libcdio-paranoia-dev libcddb2-dev \
  libsamplerate0-dev liblua5.2-dev \
  libsndfile1-dev libflac-dev libvorbis-dev libogg-dev \
  liboggz2-dev libspeex-dev libmad0-dev libmp3lame-dev \
  libmodplug-dev libmpcdec-dev libwavpack-dev \
  libavcodec-dev libavformat-dev libavutil-dev \
  libasound2-dev libjack-jackd2-dev libpulse-dev \
  libpipewire-0.3-dev libsndio-dev
</pre>
</div>
</div>

Note: Monkey's Audio (libmac) isn't packaged in Debian main, so that
decoder can't be installed with apt. Configure will skip it, or you
can build the library from source if you want it.

<h2>Debug build</h2>

<p>The configure option <tt>--enable-debug</tt> builds Aqualung with
debugging support enabled (<tt>gcc -rdynamic -ggdb -g -O0</tt>).
Furthermore, a signal handler is installed which runs when the program
receives a SEGV, FPE, ILL, BUS or ABRT signal, and prints useful debug
info (most notably a stack trace). If you get such a report, please
[open an issue](https://github.com/jeremyevans/aqualung/issues) and
include it along with a short description of what you were doing when
the program crashed. Please also send the output of `<tt>aqualung
-v</tt>'.</p>

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
the <a href="https://cygwin.com/">Cygwin</a> UNIX emulation
layer. (There used to be a native installer, but that is no longer
maintained.)</p>

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
