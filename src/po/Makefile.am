include Makevars

localedir = $(datadir)/locale

MSGFMT = /usr/bin/msgfmt
XGETTEXT = /usr/bin/xgettext
MSGMERGE = /usr/bin/msgmerge

POFILES = $(wildcard *.po)
MOFILES = $(wildcard *.mo)

EXTRA_DIST = $(DOMAIN).pot $(POFILES) $(MOFILES) Makevars ABOUT-NLS

all: 
	for i in $(POFILES); do $(MAKE) `echo $$i | sed 's/\.po/\.mo/'`; done

hu.mo: hu.po
	iconv -f iso8859-2 -t utf-8 -o $<.utf8 $<; \
	msgfmt $<.utf8 -o $@; \
	rm $<.utf8

de.mo: de.po
	iconv -f iso8859-1 -t utf-8 -o $<.utf8 $<; \
	msgfmt $<.utf8 -o $@; \
	rm $<.utf8

uk.mo: uk.po
	iconv -f koi8-u -t utf-8 -o $<.utf8 $<; \
	msgfmt $<.utf8 -o $@; \
	rm $<.utf8

%.po: $(DOMAIN).pot
	$(MSGMERGE) $@ $< -o $@.in; \
	mv $@.in $@

pot:
	$(XGETTEXT) $(XGETTEXT_OPTS) --copyright-holder='$(COPYRIGHT_HOLDER)' $(POTFILES) -o $(DOMAIN).pot

install:
	for i in $(MOFILES); do \
		lang=`echo $$i | sed 's/\.mo$$//'`; \
		$(mkinstalldirs) $(localedir)/$$lang/LC_MESSAGES; \
		$(INSTALL_DATA) $$i $(localedir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
	done

uninstall:
	for i in $(MOFILES); do \
		lang=`echo $$i | sed 's/\.mo$$//'`; \
		rm -f $(localedir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
	done

