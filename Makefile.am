ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

pkgconfigdir = $(libdir)/pkgconfig
vapidir = $(datadir)/vala/vapi
pluginsdir = $(pkglibdir)/plugins

bin_PROGRAMS =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
plugins_LTLIBRARIES =
pkginclude_HEADERS =
pkgconfig_DATA =
dist_vapi_DATA =
dist_pkgdata_DATA =

EXTRA_DIST =
CLEANFILES =

AM_CFLAGS += \
	-D LOCALEDIR=\"$(localedir)\" \
	--include config.h \
	-I $(top_srcdir)/include/ \
	-I $(top_builddir)/include/

AM_VALAFLAGS += \
	--vapidir $(top_srcdir)/vapi/ \
	--vapidir $(top_builddir)/vapi/ \
	--pkg config

EXTRA_DIST += \
	vapi/glib-local-2.0.vapi \
	vapi/config.vapi

include src/libeventd-event/libeventd-event.mk
include src/libeventd/libeventd.mk
include src/eventd/eventd.mk
include src/eventdctl/eventdctl.mk
include src/plugins/sndfile/sndfile.mk
include src/plugins/espeak/espeak.mk
include src/plugins/notification/notification.mk
include src/libeventc/libeventc.mk
include src/eventc/eventc.mk


#
# Hooks
#

install-data-hook: la-files-install-hook
uninstall-hook: la-files-uninstall-hook

# *.la files cleanup
la-files-install-hook:
	cd $(DESTDIR)/$(pkglibdir) && \
		rm $(pkglib_LTLIBRARIES)
	cd $(DESTDIR)/$(pluginsdir) && \
		rm $(plugins_LTLIBRARIES)

# Remove *.so files at uninstall since
# we remove *.la files at install
la-files-uninstall-hook:
	cd $(DESTDIR)/$(pkglibdir) && \
		rm $(pkglib_LTLIBRARIES:.la=.so)
	cd $(DESTDIR)/$(pluginsdir) && \
		rm $(plugins_LTLIBRARIES:.la=.so)
	rmdir $(DESTDIR)/$(pluginsdir)
	rmdir $(DESTDIR)/$(pkglibdir)
