<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:fn="http://www.w3.org/2005/02/xpath-functions">

<xsl:output method="text" encoding="utf-8"/>

<xsl:strip-space elements="*"/>

<xsl:template match="aqualung-doc">
<xml:text>.TH AQUALUNG 1 &#34;</xml:text><xsl:value-of select="$date"/><xml:text>&#34;
.SH NAME
aqualung \- Music player for GNU/Linux
.SH SYNOPSIS
</xml:text>
<xsl:for-each select="//*[@man='SYNOPSYS']"><xsl:apply-templates/></xsl:for-each>
<xml:text>.SH DESCRIPTION
</xml:text>
<xsl:for-each select="//*[@man='DESCRIPTION']"><xsl:apply-templates/></xsl:for-each>
<xml:text>.P
Please refer to the documentation available at the homepage for a
detailed description of features, usage tips and troubleshooting
issues. This manual page is merely an abstract from the User's Manual,
and documents only the command line interface of the program for quick
reference.
.SH OPTIONS
</xml:text>
<xsl:for-each select="//*[@man='OPTIONS']">
<xsl:apply-templates select="."/>
</xsl:for-each>
<xml:text>.SH FILES
</xml:text>
<xsl:for-each select="//*[@man='FILES']">
<xsl:apply-templates select="."/>
</xsl:for-each>
<xml:text>.SH ENVIRONMENT
</xml:text>
<xsl:for-each select="//*[@man='ENVIRONMENT']">
<xsl:apply-templates select="."/>
</xsl:for-each>
<xml:text>
.SH AUTHORS
.br
Tom Szilagyi &lt;tszilagyi@users.sourceforge.net&gt;
.br
Peter Szilagyi &lt;peterszilagyi@users.sourceforge.net&gt;
.br
Tomasz Maka &lt;pasp@users.sourceforge.net&gt;
.br
Jeremy Evans &lt;code@jeremyevans.net&gt;
.SH BUGS
.P
Yes. Report them to our bugtracker at &lt;https://github.com/jeremyevans/aqualung/issues&gt;
.SH HOMEPAGE
.P
Please go to &lt;http://aqualung.jeremyevans.net/&gt; to download the latest version,
and access the Aqualung bugtracker.
.SH USER'S MANUAL
.P
The latest version of the User's Manual is available at the project homepage.
</xml:text>
</xsl:template>

<xsl:template match="dl">
<xsl:apply-templates/>
</xsl:template>

<xsl:template match="dt">
<xsl:text>.TP
</xsl:text>
<xsl:apply-templates/>
<xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="*[@man='no']">
</xsl:template>

<xsl:template match="dd">
<xsl:text>.br
</xsl:text>
<xsl:apply-templates/>
<xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="section|subsection|subsubsection">
<xsl:text>
.TP
.B </xsl:text>
<xsl:value-of select="@title"/>
<xsl:text>
</xsl:text>
<xsl:apply-templates/>
</xsl:template>

<xsl:template match="subsection[@title='Examples']">
<xsl:text>
.TP
.B Examples
.nf
</xsl:text>
<xsl:apply-templates/>
<xsl:text>.fi
</xsl:text>
</xsl:template>

<xsl:template match="p">
<xsl:text>.P
</xsl:text>
<xsl:apply-templates/>
<xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="q">
<xsl:text>`</xsl:text>
<xsl:apply-templates/>
<xsl:text>'</xsl:text>
</xsl:template>

<xsl:template match="gui|emph">
<xsl:text>\fB</xsl:text>
<xsl:apply-templates/>
<xsl:text>\fR</xsl:text>
</xsl:template>

</xsl:stylesheet>
