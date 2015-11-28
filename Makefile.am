ACLOCAL_AMFLAGS = -I m4

SUBDIRS = doc po skin . src
EXTRA_DIST = autogen.sh gettext.h ladspa.h src/version.h src/undef_ac_pkg.h

all-local:
	cd src; \
	if test -e version.h; then git1="`cat version.h`"; else git1=""; fi; \
	if test -e ../.git; then git2="#define AQUALUNG_VERSION \"`LC_ALL=C git describe`\""; else git2=""; fi; \
	if test "$$git1" -a "$$git2" -a "$$git1" != "$$git2" -o -z "$$git1" -a "$$git2"; then echo "$$git2" > version.h; fi; \
	if test -z "$$git1" -a -z "$$git2"; then echo "#define AQUALUNG_VERSION \"R-???\"" > version.h; fi;

uninstall-local:
	rm -rf $(pkgdatadir)
