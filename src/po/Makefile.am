DOMAIN = aqualung

XGETTEXT_OPTS = --keyword=_ --keyword=N_ --keyword=X_ --no-location --copyright-holder='Tom Szilagyi'

POTFILES = $(wildcard ../*.c ../*.cpp ../decoder/*.c ../decoder/*.cpp)

localedir = $(datadir)/locale

MSGFMT = /usr/bin/msgfmt
XGETTEXT = /usr/bin/xgettext
MSGMERGE = /usr/bin/msgmerge

POFILES = de.po hu.po it.po uk.po
MOFILES = $(POFILES:.po=.mo)

EXTRA_DIST = $(POFILES) $(MOFILES) README

all: $(MOFILES)

%.mo: always
	@po=$(@:.mo=.po); if test $$po -nt $@ ; then $(MSGFMT) $$po -o $@; echo "$@ updated."; fi

%.po: $(DOMAIN).pot
	$(MSGMERGE) $@ $< -o $@.in && mv $@.in $@

$(DOMAIN).pot: $(POTFILES)
	$(XGETTEXT) $(XGETTEXT_OPTS) $(POTFILES) -o $(DOMAIN).pot

install: $(MOFILES)
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

always:
