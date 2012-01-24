ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
vapidir = $(datadir)/vala/vapi
pluginsdir = $(pkglibdir)/plugins
soundpluginsdir = $(pluginsdir)/sound

bin_PROGRAMS =
lib_LTLIBRARIES =
pkglib_LTLIBRARIES =
plugins_LTLIBRARIES =
soundplugins_LTLIBRARIES =
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


include src/libeventd-event/libeventd-event.mk
include src/libeventd/libeventd.mk
include src/eventd/eventd.mk
include src/eventdctl/eventdctl.mk
include src/plugins/sound/sound.mk
include src/plugins/sound/sndfile/sndfile.mk
include src/plugins/sound/espeak/espeak.mk
include src/plugins/notification/notification.mk
include src/libeventc/libeventc.mk
include src/eventc/eventc.mk


#
# Hooks
#

install-data-hook: la-files-removal

# *.la files cleanup
la-files-removal:
	cd $(DESTDIR)/$(libdir) && \
		rm $(lib_LTLIBRARIES)
	cd $(DESTDIR)/$(pkglibdir) && \
		rm $(pkglib_LTLIBRARIES)
	cd $(DESTDIR)/$(pluginsdir) && \
		rm $(plugins_LTLIBRARIES)
	cd $(DESTDIR)/$(soundpluginsdir) && \
		rm $(soundplugins_LTLIBRARIES)
