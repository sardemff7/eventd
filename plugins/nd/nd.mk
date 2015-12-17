# notification plugin

if ENABLE_NOTIFICATION_DAEMON
plugins_LTLIBRARIES += \
	nd.la \
	$(null)

man1_MANS += \
	%D%/man/eventdctl-nd.1 \
	$(null)

man5_MANS += \
	%D%/man/eventd-nd.conf.5 \
	$(null)

dist_fdonotificationscapabilities_DATA += \
	%D%/fdonotificationscapabilities/nd.capabilities \
	$(null)
endif


nd_la_SOURCES = \
	%D%/src/icon.h \
	%D%/src/icon.c \
	%D%/src/cairo.c \
	%D%/src/cairo.h \
	%D%/src/style.c \
	%D%/src/style.h \
	%D%/src/pixbuf.h \
	%D%/src/pixbuf.c \
	%D%/src/backend.h \
	%D%/src/backends.h \
	%D%/src/backends.c \
	%D%/src/nd.c \
	$(null)

nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(NKUTILS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

nd_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(GDK_PIXBUF_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GMODULE_LIBS) \
	$(NKUTILS_LIBS) \
	$(GLIB_LIBS) \
	$(null)


#
# Backends
#

ndbackendsdir = $(pluginsdir)/nd
ndbackends_LTLIBRARIES =

ND_BACKENDS_CFLAGS = \
	$(AM_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

ND_BACKENDS_LIBS = \
	libeventd.la \
	libeventd-helpers.la \
	$(CAIRO_LIBS) \
	$(GLIB_LIBS) \
	$(null)

if ENABLE_ND_XCB
include src/libgwater/xcb.mk

ndbackends_LTLIBRARIES += \
	xcb.la \
	$(null)

xcb_la_SOURCES = \
	%D%/src/backend.h \
	%D%/src/backend-xcb.c \
	$(null)

xcb_la_CFLAGS = \
	$(ND_BACKENDS_CFLAGS) \
	$(GW_XCB_CFLAGS) \
	$(null)

xcb_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

xcb_la_LIBADD = \
	$(ND_BACKENDS_LIBS) \
	$(GW_XCB_LIBS) \
	$(null)
endif
