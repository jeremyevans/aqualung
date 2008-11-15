<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">

  <xsl:import href="aqualung-xhtml.xsl"/>

  <xsl:output method="xml"
	      omit-xml-declaration="yes"
	      doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
	      doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
	      indent="yes"
	      encoding="utf-8"/>


  <xsl:template match="aqualung-doc">
    <xsl:apply-templates select="document"/>
  </xsl:template>

  <!-- section level -->

  <xsl:template name="prev-for-section">
    <xsl:for-each select="preceding-sibling::section[1]">
      <xsl:choose>
	<xsl:when test="child::subsection">
	  <xsl:for-each select="subsection[last()]">
	    <xsl:text>Prev: </xsl:text>
	    <xsl:number count="section"/>
	    <xsl:text>.</xsl:text>
	    <xsl:number count="subsection"/>
	    <xsl:text>. </xsl:text>
	    <a>
	      <xsl:attribute name="href">
		<xsl:call-template name="file-of"/>
	      </xsl:attribute>
	      <xsl:value-of select="@title"/>
	    </a>
	  </xsl:for-each>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:text>Prev: </xsl:text>
	  <xsl:number count="section"/>
	  <xsl:text>. </xsl:text>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="next-for-section">
    <xsl:choose>
      <xsl:when test="subsection">
	<xsl:for-each select="child::subsection[1]">
	  <xsl:text>Next: </xsl:text>
	  <xsl:number count="section"/>
	  <xsl:text>.1. </xsl:text>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</xsl:for-each>
      </xsl:when>
      <xsl:otherwise>
	<xsl:for-each select="following-sibling::section[1]">
	  <xsl:text>Next: </xsl:text>
	  <xsl:number count="section"/>
	  <xsl:text>. </xsl:text>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</xsl:for-each>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="toc-for-section">
    <xsl:for-each select="subsection">
      <div class="toc1">
	<xsl:call-template name="num-subsection"/>
	<a>
	  <xsl:attribute name="href">
	    <xsl:call-template name="file-of"/>
	  </xsl:attribute>
	  <xsl:value-of select="@title"/>
	</a>
      </div>
      <xsl:text>
      </xsl:text>
      <xsl:for-each select="subsubsection">
	<div class="toc2">
	  <xsl:call-template name="num-subsubsection"/>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	      <xsl:text>#</xsl:text>
	      <xsl:value-of select="generate-id()"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</div>
	<xsl:text>
	</xsl:text>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="nav-section">
    <table style="width: 100%">
      <tr>
	<td align="left" style="width: 40%">
	  <xsl:call-template name="prev-for-section"/>
	</td>
	<td align="center" style="width: 20%">
	  <xsl:text>[ </xsl:text>
	  <a href="index.html">home</a>
	  <xsl:text> ]</xsl:text>
	</td>
	<td align="right" style="width: 40%">
	  <xsl:call-template name="next-for-section"/>
	</td>
      </tr>
    </table>
  </xsl:template>

  <!-- subsection level -->

  <xsl:template name="prev-for-subsection">
    <xsl:choose>
      <xsl:when test="preceding-sibling::subsection">
	<xsl:text>Prev: </xsl:text>
	<xsl:number count="section"/>
	<xsl:text>.</xsl:text>
	<xsl:for-each select="preceding-sibling::subsection[1]">
	  <xsl:number count="subsection"/>
	  <xsl:text>. </xsl:text>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</xsl:for-each>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>Prev: </xsl:text>
	<xsl:for-each select="ancestor::section">
	  <xsl:number count="section"/>
	  <xsl:text>. </xsl:text>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</xsl:for-each>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="next-for-subsection">
    <xsl:choose>
      <xsl:when test="following-sibling::subsection">
	<xsl:text>Next: </xsl:text>
	<xsl:number count="section"/>
	<xsl:text>.</xsl:text>
	<xsl:for-each select="following-sibling::subsection[1]">
	  <xsl:number count="subsection"/>
	  <xsl:text>. </xsl:text>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</xsl:for-each>
      </xsl:when>
      <xsl:otherwise>
	<xsl:for-each select="following::section[1]">
	  <xsl:text>Next: </xsl:text>
	  <xsl:number count="section"/>
	  <xsl:text>. </xsl:text>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</xsl:for-each>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="toc-for-subsection">
    <xsl:for-each select="subsubsection">
      <div class="toc1">
	<xsl:call-template name="num-subsubsection"/>
	<a href="#{generate-id()}">
	  <xsl:value-of select="@title"/>
	</a>
      </div>
      <xsl:text>
      </xsl:text>
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="nav-subsection">
    <table style="width: 100%">
      <tr>
	<td align="left" style="width: 40%">
	  <xsl:call-template name="prev-for-subsection"/>
	</td>
	<td align="center" style="width: 20%">
	  <xsl:text>[ </xsl:text>
	  <a href="index.html">home</a>
	  <xsl:text> ]</xsl:text>
	</td>
	<td align="right" style="width: 40%">
	  <xsl:call-template name="next-for-subsection"/>
	</td>
      </tr>
    </table>
  </xsl:template>

  <!-- toplevel -->

  <xsl:template name="toc">
    <xsl:for-each select="section">
      <div class="toc1">
	<xsl:call-template name="num-section"/>
	<a>
	  <xsl:attribute name="href">
	    <xsl:call-template name="file-of"/>
	  </xsl:attribute>
	  <xsl:value-of select="@title"/>
	</a>
      </div>
      <xsl:text>
      </xsl:text>
      <xsl:for-each select="subsection">
	<div class="toc2">
	  <xsl:call-template name="num-subsection"/>
	  <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	  </a>
	</div>
	<xsl:text>
	</xsl:text>
	<xsl:for-each select="subsubsection">
	  <div class="toc3">
	    <xsl:call-template name="num-subsubsection"/>
	    <a>
	    <xsl:attribute name="href">
	      <xsl:call-template name="file-of"/>
	      <xsl:text>#</xsl:text>
	      <xsl:value-of select="generate-id()"/>
	    </xsl:attribute>
	    <xsl:value-of select="@title"/>
	    </a>
	  </div>
	  <xsl:text>
	  </xsl:text>
	</xsl:for-each>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="nav-toc">
    <table style="width: 100%">
      <tr>
	<td align="right">
	  <xsl:for-each select="child::section[1]">
	    <xsl:text>Next: </xsl:text>
	    <xsl:number count="section"/>
	    <xsl:text>. </xsl:text>
	    <a>
	      <xsl:attribute name="href">
		<xsl:call-template name="file-of"/>
	      </xsl:attribute>
	      <xsl:value-of select="@title"/>
	    </a>
	  </xsl:for-each>
	</td>
      </tr>
    </table>
  </xsl:template>

  <xsl:template name="file-of">
    <xsl:choose>
      <xsl:when test="self::section">
	<xsl:text>aqualung-doc-part_</xsl:text>
	<xsl:number count="section"/>
	<xsl:text>.html</xsl:text>
      </xsl:when>
      <xsl:when test="self::subsection">
	<xsl:text>aqualung-doc-part_</xsl:text>
	<xsl:number count="section"/>
	<xsl:text>_</xsl:text>
	<xsl:number count="subsection"/>
	<xsl:text>.html</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:for-each select="ancestor::*[1]">
	  <xsl:call-template name="file-of"/>
	</xsl:for-each>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="ref">
    <a>
      <xsl:attribute name="href">
	<xsl:variable name="tmp" select="generate-id(key('crossref', @refkey))"/>
	<xsl:for-each select="/aqualung-doc//*[generate-id()=$tmp]">
	  <xsl:call-template name="file-of"/>
	</xsl:for-each>
	<xsl:text>#</xsl:text>
	<xsl:value-of select="generate-id(key('crossref', @refkey))"/>
      </xsl:attribute>
      <xsl:apply-templates/>
    </a>
  </xsl:template>

  <xsl:template match="document">

    <xsl:text disable-output-escaping="yes">&#x0A;&amp;&amp;&#x0A;index.html&#x0A;</xsl:text>

    <html>
      <head>
	<title><xsl:value-of select="/aqualung-doc/titlepage/title"/></title>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
	<link rel="stylesheet" type="text/css" href="aqualung-doc.css"/>
      </head>
      <body>

	<xsl:call-template name="nav-toc"/>
	<hr/>

	<xsl:for-each select="/aqualung-doc">
	  <xsl:apply-templates select="titlepage"/>
	</xsl:for-each>
	<h1>Table of Contents</h1>
	<xsl:call-template name="toc"/>

	<hr/>
	<xsl:call-template name="nav-toc"/>

      </body>
    </html>

    <xsl:for-each select="section">

      <xsl:text>
      </xsl:text>
      <xsl:text disable-output-escaping="yes">&#x0A;&amp;&amp;&#x0A;</xsl:text>
      <xsl:call-template name="file-of"/>
      <xsl:text disable-output-escaping="yes">&#x0A;</xsl:text>

      <html>
	<head>
	  <title><xsl:value-of select="/aqualung-doc/titlepage/title"/></title>
	  <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
	  <link rel="stylesheet" type="text/css" href="aqualung-doc.css"/>
	</head>
	<body>

	  <xsl:call-template name="nav-section"/>
	  <hr/>

	  <h1>
	    <xsl:call-template name="num-section"/>
	    <xsl:value-of select="@title"/>
	  </h1>
	  
	  <xsl:call-template name="toc-for-section"/>
	  
	  <xsl:apply-templates select="child::*[local-name()!='subsection']"/>

	  <hr/>	  
	  <xsl:call-template name="nav-section"/>
	</body>
      </html>

      <xsl:for-each select="subsection">

	<xsl:text>
	</xsl:text>
	<xsl:text disable-output-escaping="yes">&#x0A;&amp;&amp;&#x0A;</xsl:text>
	<xsl:call-template name="file-of"/>
	<xsl:text disable-output-escaping="yes">&#x0A;</xsl:text>

	<html>
	  <head>
	    <title><xsl:value-of select="/aqualung-doc/titlepage/title"/></title>
	    <meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
	    <link rel="stylesheet" type="text/css" href="aqualung-doc.css"/>
	  </head>
	  <body>

	    <xsl:call-template name="nav-subsection"/>
	    <hr/>

	    <xsl:if test="@img">
	      <xsl:call-template name="img-float">
		<xsl:with-param name="src" select="@img"/>
	      </xsl:call-template>
	    </xsl:if>

	    <h2>
	      <xsl:call-template name="num-subsection"/>
	      <xsl:value-of select="@title"/>
	    </h2>
	  
	    <xsl:call-template name="toc-for-subsection"/>
	  
	    <xsl:apply-templates/>

	    <hr/>	  
	    <xsl:call-template name="nav-subsection"/>
	  </body>
	</html>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:template>

</xsl:stylesheet>
