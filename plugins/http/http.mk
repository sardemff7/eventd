# HTTP plugin

if ENABLE_HTTP
plugins_LTLIBRARIES += \
	http.la \
	$(null)
endif


http_la_SOURCES = \
	%D%/src/http.c \
	%D%/src/websocket.h \
	%D%/src/websocket.c \
	$(null)

http_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(LIBSOUP_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

http_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

http_la_LIBADD = \
	libeventd.la \
	libeventd-protocol-json.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(AVAHI_LIBS) \
	$(LIBSOUP_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
