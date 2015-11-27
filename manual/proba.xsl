<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns="http://www.w3.org/1999/xhtml">

  <xsl:output method="xml"
	      omit-xml-declaration="yes"
	      doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"
	      doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN"
	      indent="yes"
	      encoding="utf-8"/>

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
	<p>
	  <xsl:apply-templates/>
	</p>
      </body>
    </html>
  </xsl:template>

</xsl:stylesheet>
