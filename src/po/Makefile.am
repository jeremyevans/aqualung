include Makevars

localedir = $(datadir)/locale

MSGFMT = /usr/bin/msgfmt
XGETTEXT = /usr/bin/xgettext
MSGMERGE = /usr/bin/msgmerge

POFILES = hu.po de.po uk.po
MOFILES = hu.mo de.mo uk.mo

EXTRA_DIST = $(DOMAIN).pot $(POFILES) $(MOFILES) Makevars ABOUT-NLS

all: $(DOMAIN).pot $(MOFILES)

hu.mo: hu.po
	iconv -f iso8859-2 -t utf-8 $< > $<.utf8; \
	msgfmt $<.utf8 -o $@; \
	rm $<.utf8

de.mo: de.po
	iconv -f iso8859-1 -t utf-8 $< > $<.utf8; \
	msgfmt $<.utf8 -o $@; \
	rm $<.utf8

uk.mo: uk.po
	iconv -f koi8-u -t utf-8 $< > $<.utf8; \
	msgfmt $<.utf8 -o $@; \
	rm $<.utf8

%.po: $(DOMAIN).pot
	$(MSGMERGE) $@ $< -o $@.in; \
	mv $@.in $@

$(DOMAIN).pot: $(POTFILES)
	$(XGETTEXT) $(XGETTEXT_OPTS) $(POTFILES) -o $(DOMAIN).pot.new;\
	if test ! -e $(DOMAIN).pot; then \
		mv $(DOMAIN).pot.new $(DOMAIN).pot; \
	else \
		cat $(DOMAIN).pot | sed 's/^#.*//g' > pot; \
		cat $(DOMAIN).pot.new | sed 's/^#.*//g' > pot.new; \
		if ! diff -q pot pot.new; then \
			mv $(DOMAIN).pot.new $(DOMAIN).pot; \
		fi; \
		rm -f pot pot.new $(DOMAIN).pot.new; \
	fi

install:
	for i in $(MOFILES); do \
		lang=`echo $$i | sed 's/\.mo$$//'`; \
		$(mkinstalldirs) ${DESTDIR}$(localedir)/$$lang/LC_MESSAGES; \
		$(INSTALL_DATA) $$i ${DESTDIR}$(localedir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
	done

uninstall:
	for i in $(MOFILES); do \
		lang=`echo $$i | sed 's/\.mo$$//'`; \
		rm -f ${DESTDIR}$(localedir)/$$lang/LC_MESSAGES/$(DOMAIN).mo; \
	done

