# HTTP plugin

if ENABLE_HTTP
plugins_LTLIBRARIES += \
	http.la \
	$(null)

man5_MANS += \
	%D%/man/eventd-http.conf.5 \
	$(null)
endif


http_la_SOURCES = \
	%D%/src/http.h \
	%D%/src/http.c \
	%D%/src/websocket.h \
	%D%/src/websocket.c \
	$(null)

http_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(LIBSOUP_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

http_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

http_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(LIBSOUP_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
