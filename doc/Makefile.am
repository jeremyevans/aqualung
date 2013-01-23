man_MANS = aqualung.1

doc_DATA = README split.sh aqualung-doc.css aqualung-doc.dtd aqualung-doc.xml *.xsl *.eps *.png

EXTRA_DIST = $(man_MANS) $(doc_DATA)

aqualung-doc: pdf html bz2 man txt

pdf: aqualung-doc.pdf

html: aqualung-doc.html index.html

bz2: aqualung-doc-html.tar.bz2 aqualung-doc-html-chunk.tar.bz2

man: aqualung.1

txt: aqualung.1.txt

aqualung-doc.tex: aqualung-latex.xsl aqualung-doc.xml
	xsltproc aqualung-latex.xsl aqualung-doc.xml > aqualung-doc.tex.tmp
	sed 's/^[ \t]*@\?//' aqualung-doc.tex.tmp > aqualung-doc.tex
	rm aqualung-doc.tex.tmp

aqualung-doc.pdf: aqualung-doc.tex
	while latex aqualung-doc.tex | grep Rerun; do echo '...rerunning.'; done
	dvipdf -sPAPERSIZE=a4 aqualung-doc.dvi
	rm aqualung-doc.dvi

aqualung-doc.html: aqualung-xhtml.xsl aqualung-doc.xml
	xsltproc --stringparam date "`LANG=C date -u`" \
	    aqualung-xhtml.xsl aqualung-doc.xml > aqualung-doc.html.tmp
	sed 's/^[ \t]*@//' aqualung-doc.html.tmp > aqualung-doc.html
	rm aqualung-doc.html.tmp

index.html: aqualung-xhtml-multipage.xsl aqualung-xhtml.xsl aqualung-doc.xml
	xsltproc --stringparam date "`LANG=C date -u`" \
	    aqualung-xhtml-multipage.xsl aqualung-doc.xml > index.html.tmp
	sed 's/^[ \t]*@//' index.html.tmp | ./split.sh
	rm index.html.tmp

aqualung-doc-html.tar.bz2: aqualung-doc.html
	mkdir aqualung-doc-html
	cp aqualung-doc.html aqualung-doc.css *.png aqualung-doc-html
	tar cvjf aqualung-doc-html.tar.bz2 aqualung-doc-html
	rm -r aqualung-doc-html

aqualung-doc-html-chunk.tar.bz2: index.html
	mkdir aqualung-doc-html-chunk
	cp aqualung-doc-part_*.html index.html aqualung-doc.css *.png aqualung-doc-html-chunk
	tar cvjf aqualung-doc-html-chunk.tar.bz2 aqualung-doc-html-chunk
	rm -r aqualung-doc-html-chunk

aqualung.1: aqualung-man.xsl aqualung-doc.xml
	xsltproc --stringparam date "`LANG=C date -u +'%d %B %Y'`" \
	    aqualung-man.xsl aqualung-doc.xml > aqualung.1.tmp
	sed 's/^[ \t]*//' aqualung.1.tmp > aqualung.1
	rm aqualung.1.tmp

aqualung.1.txt: aqualung.1
	nroff -c -man aqualung.1 > aqualung.1.txt.tmp
	col -bx < aqualung.1.txt.tmp > aqualung.1.txt
	rm aqualung.1.txt.tmp

.PHONY: clean clean-all

clean:
	rm -f *.tex *.aux *.dvi *.log *.out *.toc *.tmp

clean-all:
	rm -f *.html *.tar.bz2 aqualung.1 aqualung.1.txt *.pdf

upload: clean-all aqualung-doc clean
	@echo
	@ls
	@echo
	lftp -c 'set net:limit-rate 0:8000; open aqualung && cd manual && mirror -R -X .svn/ -X Makefile* -X *.sh'
