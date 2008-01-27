DOMAIN = aqualung

XGETTEXT_OPTS = --keyword=_ --keyword=N_ --keyword=X_ --no-location --copyright-holder='Tom Szilagyi'

POTFILES = ../*.c ../decoder/*.c ../decoder/*.cpp

localedir = $(datadir)/locale

MSGFMT = msgfmt
XGETTEXT = xgettext
MSGMERGE = msgmerge

POFILES = de.po hu.po it.po uk.po ru.po
MOFILES = $(POFILES:.po=.mo)

EXTRA_DIST = $(POFILES) $(MOFILES) README

all: $(MOFILES)

$(MOFILES): always
	@if test $(@:.mo=.po) -nt $@; then $(MSGFMT) $(@:.mo=.po) -o $@; echo "$@ updated."; fi

$(POFILES): $(DOMAIN).pot
	$(MSGMERGE) $@ $(DOMAIN).pot -o $@.in && mv $@.in $@

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
