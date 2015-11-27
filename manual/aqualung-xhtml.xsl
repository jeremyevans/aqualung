<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">

  <xsl:output method="xml"
	      omit-xml-declaration="yes"
	      doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
	      doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
	      indent="yes"
	      encoding="utf-8"/>

  <xsl:key name="toc" match="section|subsection|subsubsection" use="@title"/>
  <xsl:key name="crossref" match="*[@key]" use="@key"/>

  <xsl:template match="aqualung-doc">
    <html>
      <head>
	<title><xsl:value-of select="titlepage/title"/></title>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
	<meta name="keywords" content="Aqualung, user's manual, manual, documentation"/>
	<meta name="description" content="Aqualung User's Manual"/>
	<link rel="stylesheet" type="text/css" href="aqualung-doc.css"/>
      </head>
      <body>
	<xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="titlepage">
    <xsl:apply-templates/>
    <p>
      <i>Generated on <xsl:value-of select="$date"/></i>
    </p>
    <hr/>
  </xsl:template>

  <xsl:template match="title">
    <h1>
      <xsl:value-of select="."/>
    </h1>
  </xsl:template>

  <xsl:template match="copyright">
    <p>Copyright <xsl:text disable-output-escaping="yes">&amp;copy; </xsl:text>
    <xsl:value-of select="."/></p>
  </xsl:template>

  <xsl:template match="license">
    <p>
      <xsl:apply-templates/>
    </p>
  </xsl:template>

  <xsl:template name="toc">
    <h1>Table of Contents</h1>
    <xsl:text>
    </xsl:text>
    <xsl:for-each select="../document/section">
      <div class="toc1">
	<xsl:call-template name="num-section"/>
	<a href="#{generate-id()}">
	  <xsl:value-of select="@title"/>
	</a>
      </div>
      <xsl:text>
      </xsl:text>
      <xsl:for-each select="subsection">
	<div class="toc2">
	  <xsl:call-template name="num-subsection"/>
	  <a href="#{generate-id()}">
	    <xsl:value-of select="@title"/>
	  </a>
	</div>
	<xsl:text>
	</xsl:text>
	<xsl:for-each select="subsubsection">
	  <div class="toc3">
	    <xsl:call-template name="num-subsubsection"/>
	    <a href="#{generate-id()}">
	      <xsl:value-of select="@title"/>
	    </a>
	  </div>
	  <xsl:text>
	  </xsl:text>
	</xsl:for-each>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:template>

  <xsl:template name="num-section">
    <xsl:number count="section"/>
    <xsl:text>. </xsl:text>
  </xsl:template>

  <xsl:template name="num-subsection">
    <xsl:number count="section"/>
    <xsl:text>.</xsl:text>
    <xsl:number count="subsection"/>
    <xsl:text>. </xsl:text>
  </xsl:template>

  <xsl:template name="num-subsubsection">
    <xsl:number count="section"/>
    <xsl:text>.</xsl:text>
    <xsl:number count="subsection"/>
    <xsl:text>.</xsl:text>
    <xsl:number count="subsubsection"/>
    <xsl:text>. </xsl:text>
  </xsl:template>

  <xsl:template match="document">
    <xsl:call-template name="toc"/>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="ref">
    <a href="#{generate-id(key('crossref', @refkey))}">
      <xsl:apply-templates/>
    </a>
  </xsl:template>

  <xsl:template match="section">
    <xsl:if test="@img">
      <xsl:call-template name="img-float">
	<xsl:with-param name="src" select="@img"/>
      </xsl:call-template>
    </xsl:if>
    <h1 id="{generate-id()}" style="padding-top: 6px">
      <xsl:call-template name="num-section"/>
      <xsl:value-of select="@title"/>
    </h1>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="subsection">
    <xsl:if test="@img">
      <xsl:call-template name="img-float">
	<xsl:with-param name="src" select="@img"/>
      </xsl:call-template>
    </xsl:if>
    <h2 id="{generate-id()}" style="padding-top: 6px">
      <xsl:call-template name="num-subsection"/>
       <xsl:value-of select="@title"/>
    </h2>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="subsubsection">
    <h3 id="{generate-id()}">
      <xsl:call-template name="num-subsubsection"/>
      <xsl:value-of select="@title"/>
    </h3>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="paragraph">
    <h4>
      <xsl:value-of select="@title"/>
    </h4>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="img">
    <p style="text-align: center"><img>
      <xsl:attribute name="src"><xsl:value-of select="@src"/></xsl:attribute>
      <xsl:attribute name="alt"><xsl:value-of select="@alt"/></xsl:attribute>
    </img></p>
  </xsl:template>

  <xsl:template name="img-float">
    <xsl:param name="src"/>
    <p>
      <img class="screenshot">
	<xsl:attribute name="src"><xsl:value-of select="$src"/></xsl:attribute>
	<xsl:attribute name="alt">[screenshot]</xsl:attribute>
      </img>
    </p>
  </xsl:template>

  <xsl:template match="a">
    <a class="external">
      <xsl:attribute name="href"><xsl:value-of select="@href"/></xsl:attribute>
      <xsl:apply-templates/>
    </a>
  </xsl:template>

  <xsl:template match="tabular">
    <table border="0" cellpadding="3" cellspacing="0">
      <xsl:apply-templates/>
    </table>
  </xsl:template>

  <xsl:template match="table|matrix">
    <table rules="all" border="1" cellpadding="5" cellspacing="0">
      <xsl:apply-templates/>
    </table>
  </xsl:template>

  <xsl:template match="shortcut">
    <table rules="all" border="1" cellpadding="5" cellspacing="0">
      <tr>
	<th>key</th><th>action</th>
      </tr>
      <xsl:call-template name="hline"/>
      <xsl:apply-templates/>
    </table>
  </xsl:template>

  <xsl:template match="tr"><tr><xsl:apply-templates/></tr></xsl:template>

  <xsl:template match="shortcut/tr/td">
    <td>
      <xsl:if test="following-sibling::td">
	<xsl:attribute name="align">center</xsl:attribute>
      </xsl:if>
      <xsl:apply-templates/>
    </td>
  </xsl:template>

  <xsl:template name="hline" match="shortcut/hline">
    <tr>
      <td colspan="2" style="height: 5px; border-left: hidden; border-right: hidden"/>
    </tr>
  </xsl:template>

  <xsl:template match="td">
    <td>
      <xsl:apply-templates/>
    </td>
  </xsl:template>

  <xsl:template match="dl"><dl><xsl:apply-templates/></dl></xsl:template>
  <xsl:template match="dt"><dt><b><xsl:apply-templates/></b></dt></xsl:template>
  <xsl:template match="dd"><dd><xsl:apply-templates/></dd></xsl:template>

  <xsl:template match="ul"><ul><xsl:apply-templates/></ul></xsl:template>
  <xsl:template match="li"><li><xsl:apply-templates/></li></xsl:template>

  <xsl:template match="p"><p><xsl:apply-templates/></p></xsl:template>
  <xsl:template match="q">`<xsl:apply-templates/>'</xsl:template>
  <xsl:template match="gui"><span class="gui"><xsl:apply-templates/></span></xsl:template>
  <xsl:template match="cmd"><code><xsl:apply-templates/></code></xsl:template>
  <xsl:template match="verbatim"><pre><xsl:apply-templates/></pre></xsl:template>
  <xsl:template match="file"><tt><xsl:apply-templates/></tt></xsl:template>
  <xsl:template match="emph"><b><i><xsl:apply-templates/></i></b></xsl:template>

  <xsl:template match="br"><br/></xsl:template>
  <xsl:template match="ndash"><xsl:text  disable-output-escaping="yes">&amp;ndash;</xsl:text></xsl:template>

</xsl:stylesheet>
