---
layout: default
title: Aqualung
permalink: /
exclude: true
---

<table style="margin-left: auto; margin-right: auto; width: 100%" cellpadding="0" cellspacing="0">
  <tbody><tr>
    <td>
      <div style="font-size: 85%;">
	<i>"When I die just keep playing the records."</i>
	<br>&nbsp;&nbsp;—&nbsp;Jimi Hendrix
      </div>
    </td>
    <td rowspan="2" align="center">
      <img src="{{ site.baseurl }}/images/logo.png" alt="">
    </td>
  </tr>
  <tr>
    <td align="center">
      <div style="font-size: 175%">Welcome to Aqualung,</div>
      a music player for GNU/Linux
    </td>
  </tr>
</tbody></table>
<br>

<p>Aqualung is an advanced music player originally targeted at the
GNU/Linux operating system, today also running on FreeBSD and OpenBSD,
with native ports to Mac OS X and even Microsoft Windows. It plays
audio CDs, internet radio streams and podcasts as well as soundfiles
in just about any audio format and has the feature of <i><b>inserting
no gaps</b></i> between adjacent tracks.

<div style="border: 1px solid rgb(204, 204, 204); background: rgb(249, 249, 249) none repeat scroll 0% 0%; padding: 10px 10px 0px; margin-bottom: 10px; display: block;">
  <table style="width: 100%" border="0" cellpadding="0" cellspacing="0"><tbody><tr>
    <td><b>Here's how the test looks like.</b></td>
  </tr></tbody></table>

  <p>Pick a song that you know
  really well, something that's in your bones like <i>Siberian
  Khatru</i>. Grab it from CD using <tt>cdparanoia</tt> to have it as
  a WAV file. Now open your favourite wave editor and slice the file
  up into multiple consecutive sections. Be careful not to insert
  silence, delete samples or alter any sample data. Save the slices to
  separate files. Now convert the sample rate of some pieces to random
  values (the example program shipped with the <a href="http://www.mega-nerd.com/SRC/">libsamplerate</a> library will
  let you do this in very good quality). Pick some pieces and convert
  them to <a href="http://www.xiph.org/ogg/vorbis">Ogg Vorbis</a>
  format. Pick some others and encode them to <a href="http://flac.sourceforge.net/">FLAC</a>. Pick a few and convert
  them to MONO. Now open up the Playlist editor of the music player in
  question and add the files in order. <b>Push play, and
  listen.</b></p>

  <p>Aqualung is a music player designed from the ground up to
  provide continuous, absolutely transparent, gap-free playback across
  a variety of input formats and a wide range of sample rates thereby
  allowing for enjoying quality music: concert recordings and
  "non-best-of" albums containing gapless transitions between some
  tracks. (Multiple movements long compositions are often broken into
  separate but gaplessly flowing tracks when mastered to CD.) Obvious
  examples are <i>The Song Remains The Same</i> (Led Zeppelin), <i>The
  Dark Side Of The Moon</i> (Pink Floyd), and <i>Yessongs</i>
  (Yes). Besides the ability to play the music from these records
  without a defect, Aqualung provides high quality sample rate
  conversion, a feature that is essential when building large digital
  music archives containing input sources conforming to various
  standards. Aqualung passed our test – and it will pass yours,
  too.</p>
</div>

<h2>Features at a glance</h2>

<h3>On the input side:</h3>

<ul>

  <li>Audio CDs can be played back and ripped to the Music Store with
  on-the-fly conversion to WAV, FLAC, Ogg Vorbis or CBR/VBR MP3
  (gapless via LAME). Seamless tagging of the created files is offered
  as part of the process.</li>

  <li>Internet radio stations streaming Ogg Vorbis or MP3 are supported.</li>

  <li>Subscribing to RSS and Atom audio podcasts is supported. Aqualung can automatically
  download and add new files to the Music Store. Optional limits for the age, size
  and number of downloaded files can be set.</li>

  <li>Almost all sample-based, uncompressed formats (e.g. WAV, AIFF,
  AU etc.) are supported. For the full list of these formats, visit
  the <a href="http://www.mega-nerd.com/libsndfile/">libsndfile</a>
  homepage.</li>

  <li>Files encoded with <a href="http://flac.sourceforge.net/">FLAC</a> (the Free Lossless Audio
  Codec) are supported.</li>

  <li><a href="http://www.vorbis.com/">Ogg Vorbis</a> and Ogg
  <a href="http://speex.org/">Speex</a> audio files are supported.</li>

  <li>MPEG Audio files are supported. This includes MPEG 1-2-2.5,
  Layer I-II-III encoded audio, including the infamous MPEG-1 Layer
  III format also known as MP3. For tracks containing the appropriate LAME headers, the MPEG
  encoder delay and padding is eliminated by Aqualung, resulting in
  truly gapless playback.  Aqualung also supports VBR (variable
  bitrate) and UBR (unspecified bitrate) MPEG files.</li>

  <li>MOD audio files (MOD, S3M, XM, IT, etc.) are supported via the
  high quality libmodplug library.</li>

  <li>Musepack (a.k.a. MPEG Plus) files are supported.</li>

  <li>Files encoded with Monkey's Audio Codec are supported.</li>

  <li>WavPack files are supported via a native decoder.</li>

  <li>Numerous formats and codecs are supported via the <a href="http://ffmpeg.mplayerhq.hu/">FFmpeg</a> project, including AC3,
  AAC, WMA, WavPack and the soundtrack of many video formats.</li>

  <li>Naturally, any of these files can be mono or stereo.</li>

</ul>

<h3>On the output side:</h3>

<ul>

  <li>OSS driver support</li>

  <li>ALSA driver support</li>

  <li><a href="http://jackaudio.org/">JACK</a> Audio Server
  support</li>

  <li><a href="http://pulseaudio.org/">PulseAudio</a> support</li>

  <li>sndio output (presently available on OpenBSD only)</li>

  <li>Win32 Sound API (available only under native Win32 or <a
  href="http://cygwin.com/">Cygwin</a>)</li>

  <li>Exporting files to external formats from Playlist and Music Store
  is supported.</li>

</ul>

<h3>In between:</h3>

<ul>

  <li>Continuous, gap-free playback of consecutive tracks. Your ears
  get exactly what is in the files – no silence inserted in
  between.</li>

  <li>Ability to convert sample rates between the input file and the
  output device, from downsampling by a factor of 12 to upsampling by
  the same factor. The best converter provides a signal-to-noise ratio
  of 97dB with -3dB passband extending from DC to 96% of the
  theoretical best bandwidth for a given pair of input and output
  sample rates.</li>

  <li><a href="http://www.ladspa.org/">LADSPA</a> plugin support
  – you can use any suitable LADSPA plugin to enhance the music
  you are listening to. There are many different equalizer, spatial
  enhancer, tube preamp simulator etc. plugins out there. If you don't
  have any, <a href="http://tap-plugins.sf.net/">grab these</a>.</li>

</ul>

<h3>Other niceties:</h3>

<ul>

  <li>Playlist tabs allow you to have multiple playlists for your music
  at the same time, very similarly to multiple tabbed browsing in Firefox.</li>

  <li>Internally working volume and balance controls (not touching the
  soundcard mixer).</li>

  <li>Support for multiple skins; changing them is possible at any
  time.</li>

  <li>Support for random seeking during playback.</li>

  <li>Track repeat, List repeat and Shuffle mode (besides normal
  playback). In track repeat mode the looping range is adjustable (A-B
  repeat). It is possible to set the looping boundaries via a single
  keystroke while listening to the track.</li>

  <li>Ability to display and edit Ogg Xiph comments, ID3v1, ID3v2 and
  APE tags found in files that support them. When exporting tracks to
  a different file format, metadata is preserved as much as
  possible.</li>

  <li>All windows are sizable. You can stretch the main window
  horizontally for more accurate seeking.</li>

  <li>You can control any running instance of the program remotely
  from the command line (start, stop, pause etc.). Remote loading or
  enqueueing soundfiles as well as complete playlists is also
  supported.</li>

  <li>State persistence via XML config files. Aqualung will come up in
  the same state as it was when you closed it, including playback
  modes, volume and balance settings, currently processing LADSPA
  plugins, window sizes, positions and visibility, and other
  miscellaneous options.</li>

</ul>

<p>In addition to all this, Aqualung comes with a <i>Music Store</i>
that is an XML-based music database, capable of storing various
metadata about music on your computer (including, but not limited to,
the names of artists, and the titles of records and tracks). You can
(and should) organize your music into a tree of
Artists/Records/Tracks, thereby making life easier than with the
all-in-one Winamp/XMMS playlist. Importing file metadata (ID3v1, ID3v2
tags, Ogg Xiph comments, APE metadata) into the Music Store as well as
getting track names from a CDDB/FreeDB database is supported.</p>

<p>Aqualung is released under the GNU General Public License. For more
info, see the <a href="/misc/">Misc</a> page.</p>
