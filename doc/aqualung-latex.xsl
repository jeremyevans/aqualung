<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">

  <xsl:output method="text" encoding="utf-8"/>

  <xsl:key name="toc" match="section|subsection|subsubsection" use="@title"/>
  <xsl:key name="crossref" match="*[@key]" use="@key"/>

  <xsl:template match="text()">
    <xsl:call-template name="replace"/>
  </xsl:template>

  <xsl:template name="replace">
    <xsl:param name="text" select="."/>
    <xsl:choose>
      <xsl:when test="contains($text, '{')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '{')"/>
	</xsl:call-template>
	<xsl:text>\{</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '{')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '}')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '}')"/>
	</xsl:call-template>
	<xsl:text>\}</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '}')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '%')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '%')"/>
	</xsl:call-template>
	<xsl:text>\%</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '%')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '_')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '_')"/>
	</xsl:call-template>
	<xsl:text>\_</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '_')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '$')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '$')"/>
	</xsl:call-template>
	<xsl:text>\$</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '$')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '--')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '--')"/>
	</xsl:call-template>
	<xsl:text>-{}-</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '--')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '\')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '\')"/>
	</xsl:call-template>
	<xsl:text>\textbackslash{}</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '\')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '~')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '~')"/>
	</xsl:call-template>
	<xsl:text>\textasciitilde{}</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '~')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '&amp;')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '&amp;')"/>
	</xsl:call-template>
	<xsl:text>\&amp;</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '&amp;')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="contains($text, '...')">
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-before($text, '...')"/>
	</xsl:call-template>
	<xsl:text>\dots{}</xsl:text>
	<xsl:call-template name="replace">
	  <xsl:with-param name="text" select="substring-after($text, '...')"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="$text"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="aqualung-doc">
    <xsl:text>
      \documentclass[10pt,english]{article}
      \usepackage{ae}
      \usepackage{aecompl}
      \usepackage[T1]{fontenc}
      \usepackage{ucs}
      \usepackage[utf8x]{inputenc}
      \usepackage{babel}
      \usepackage{geometry}
      \geometry{a4paper,tmargin=3cm,bmargin=3cm,lmargin=2.5cm,rmargin=2.5cm,headsep=1cm}
      \usepackage{graphicx}
      \usepackage{color}
      \usepackage{array}
      \usepackage[dvips, bookmarks, bookmarksopen=true, bookmarksopenlevel=2, colorlinks=true, pdfview=FitH, pdfstartview=FitH, urlcolor=blue, linkcolor=black, pdftitle={</xsl:text><xsl:value-of select="titlepage/title"/><xsl:text>}]{hyperref}
      \usepackage{fancyhdr}
      \pagestyle{fancy}
      \sloppy
      \lhead{</xsl:text><xsl:value-of select="titlepage/title"/><xsl:text>}
      \chead{}
      \rhead{\thepage}
      \lfoot{}
      \cfoot{}
      \rfoot{}
      \begin{document}
      \begin{titlepage}
      \begin{center}
      \hspace{0mm}
      \vspace{10mm}\\
      {\Huge\bf Aqualung}
      \vspace{3mm}\\
      {\LARGE Music Player for GNU/Linux}
      \vspace{10mm}\\
      \today
      \vspace{70mm}\\
      {\Huge User's Manual}
      \vspace{60mm}\\
      Copyright \copyright{} </xsl:text><xsl:value-of select="titlepage/copyright"/><xsl:text>
      \vspace{5mm}\\
      </xsl:text><xsl:for-each select="titlepage/license"><xsl:apply-templates/></xsl:for-each><xsl:text>
      \end{center}
      \end{titlepage}
      \tableofcontents{}
      \newpage{}
    </xsl:text>
    <xsl:apply-templates select="document"/>
    <xsl:text>
      \end{document}
    </xsl:text>
  </xsl:template>

  <xsl:template match="document">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="ref">
    <xsl:text>\hyperref[</xsl:text>
    <xsl:value-of select="generate-id(key('crossref', @refkey))"/>
    <xsl:text>]{\color{blue}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="section">
    <xsl:text>\section{</xsl:text>
    <xsl:value-of select="@title"/>
    <xsl:text>\label{</xsl:text>
    <xsl:value-of select="generate-id()"/>
    <xsl:text>}}</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="subsection">
    <xsl:text>\subsection{</xsl:text>
    <xsl:value-of select="@title"/>
    <xsl:text>\label{</xsl:text>
    <xsl:value-of select="generate-id()"/>
    <xsl:text>}}</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="subsubsection">
    <xsl:text>\subsubsection{</xsl:text>
    <xsl:value-of select="@title"/>
    <xsl:text>\label{</xsl:text>
    <xsl:value-of select="generate-id()"/>
    <xsl:text>}}</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="paragraph">
    <xsl:text>\paragraph*{</xsl:text>
    <xsl:value-of select="@title"/>
    <xsl:text>}</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="img">
    <xsl:text>\begin{figure}[h!] \centering \includegraphics[scale=0.7]{</xsl:text>
    <xsl:if test="contains(@src, '.')">
      <xsl:value-of select="substring-before(@src, '.')"/>
      <xsl:text>.eps</xsl:text>
    </xsl:if>
    <xsl:text>}\end{figure}</xsl:text>
  </xsl:template>

  <xsl:template match="a">
    <xsl:text>\href{</xsl:text>
    <xsl:value-of select="@href"/>
    <xsl:text>}{</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\includegraphics[scale=0.5]{external.eps}}</xsl:text>
  </xsl:template>

  <xsl:template match="tabular">
    <xsl:text>\begin{center}\begin{tabular}{</xsl:text>
    <xsl:for-each select="tr[1]">
      <xsl:for-each select="td">
	<xsl:text>l</xsl:text>
      </xsl:for-each>
    </xsl:for-each>
    <xsl:text>}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{tabular}\end{center}</xsl:text>
  </xsl:template>

  <xsl:template match="table">
    <xsl:text>\begin{center}\begin{tabular}{</xsl:text>
    <xsl:for-each select="tr[1]">
      <xsl:for-each select="td">
	<xsl:text>|l</xsl:text>
      </xsl:for-each>
    </xsl:for-each>
    <xsl:text>|}\hline </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{tabular}\end{center}</xsl:text>
  </xsl:template>

  <xsl:template match="matrix">
    <xsl:text>\begin{center}\begin{tabular}{|l</xsl:text>
    <xsl:for-each select="tr[1]">
      <xsl:for-each select="td[position() &gt; 1]">
	<xsl:text>|c</xsl:text>
      </xsl:for-each>
    </xsl:for-each>
    <xsl:text>|}\hline </xsl:text>
    <xsl:for-each select="tr">
      <xsl:choose>
	<xsl:when test="position() = 1">
	  <xsl:for-each select="td">
	    <xsl:text>\rotatebox{90}{\mbox{</xsl:text>
	    <xsl:apply-templates/>
	    <xsl:text>~}}</xsl:text>
	    <xsl:if test="following-sibling::td">
	      <xsl:text>&amp;</xsl:text>
	    </xsl:if>
	  </xsl:for-each>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates/>
	</xsl:otherwise>
      </xsl:choose>
      <xsl:text>\\ \hline </xsl:text>
    </xsl:for-each>
    <xsl:text>\end{tabular}\end{center}</xsl:text>
  </xsl:template>

  <xsl:template match="shortcut">
    <xsl:text>\begin{center}\begin{tabular}{|m{4cm}|m{10cm}|}</xsl:text>
    <xsl:text>\hline \multicolumn{1}{|c|}{\textbf{key}} &amp; \multicolumn{1}{c|}{\textbf{action}} \\ \hline \hline </xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{tabular}\end{center}</xsl:text>
  </xsl:template>

  <xsl:template match="tabular/tr">
    <xsl:apply-templates/>
    <xsl:text>\\</xsl:text>
  </xsl:template>

  <xsl:template match="table/tr|shortcut/tr">
    <xsl:apply-templates/>
    <xsl:text>\\ \hline </xsl:text>
  </xsl:template>

  <xsl:template match="shortcut/hline">
    <xsl:text> \hline </xsl:text>
  </xsl:template>

  <xsl:template match="td">
    <xsl:apply-templates/>
    <xsl:if test="following-sibling::td">
      <xsl:text>&amp;</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="dl">
    <xsl:text>\begin{description}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{description}</xsl:text>
  </xsl:template>

  <xsl:template match="dt">
    <xsl:text>\item [</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>]</xsl:text>
  </xsl:template>

  <xsl:template match="dd"><xsl:apply-templates/></xsl:template>

  <xsl:template match="ul">
    <xsl:text>\begin{itemize}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{itemize}</xsl:text>
  </xsl:template>

  <xsl:template match="li">
    <xsl:text>\item </xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="p">
    <xsl:text disable-output-escaping="yes">&#x0A;&#x0A;</xsl:text>
    <xsl:if test="name(preceding-sibling::*[position()=1])!='p'">
      <xsl:text>\noindent </xsl:text>
    </xsl:if>
    <xsl:apply-templates/>
    <xsl:text disable-output-escaping="yes">&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="q">`<xsl:apply-templates/>'</xsl:template>

  <xsl:template match="gui">
    <xsl:text>\textsl{</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="cmd">
    <xsl:text>\texttt{</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="file">
    <xsl:text>\texttt{</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="emph">
    <xsl:text>\emph{</xsl:text>
    <xsl:apply-templates/>
  <xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="br">
    <xsl:text>\\</xsl:text>
  </xsl:template>

  <xsl:template match="ndash">
    <xsl:text>--</xsl:text>
  </xsl:template>

</xsl:stylesheet>
