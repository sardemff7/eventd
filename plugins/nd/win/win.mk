# Windows backend

if ENABLE_WINDOWS
ndbackends_LTLIBRARIES += \
	win.la \
	$(null)

include src/libgwater/win.mk
endif


win_la_SOURCES = \
	%D%/src/win.c \
	$(null)

win_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GW_WIN_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

win_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

win_la_LIBADD = \
	libeventd.la \
	libeventd-helpers.la \
	$(GW_WIN_LIBS) \
	-lgdi32 \
	$(CAIRO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
