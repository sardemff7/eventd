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
	%D%/src/nd.c \
	$(null)

nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GMODULE_CFLAGS) \
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
	$(GLIB_LIBS) \
	$(null)


#
# Backends
#

if ENABLE_ND_XCB
include src/libgwater/xcb.mk
nd_la_SOURCES += \
	%D%/src/backend-xcb.h \
	%D%/src/backend-xcb.c \
	$(null)
nd_la_CFLAGS += $(GW_XCB_CFLAGS)
nd_la_LIBADD += $(GW_XCB_LIBS)
endif

if ENABLE_ND_LINUX_FB
nd_la_SOURCES += \
	%D%/src/backend-fbdev.h \
	%D%/src/backend-linux.c \
	$(null)
endif
