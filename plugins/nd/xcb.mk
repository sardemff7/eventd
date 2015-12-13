# XCB backend

if ENABLE_ND_XCB
ndbackends_LTLIBRARIES += \
	xcb.la \
	$(null)

include src/libgwater/xcb.mk
endif


xcb_la_SOURCES = \
	%D%/src/backend-xcb.c \
	$(null)

xcb_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GW_XCB_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

xcb_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

xcb_la_LIBADD = \
	libeventd.la \
	libeventd-helpers.la \
	$(GW_XCB_LIBS) \
	$(CAIRO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
