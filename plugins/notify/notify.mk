# notify plugin

if ENABLE_NOTIFY
plugins_LTLIBRARIES += \
	notify.la \
	$(null)

man5_MANS += \
	%D%/man/eventd-notify.conf.5 \
	$(null)
endif


notify_la_SOURCES = \
	%D%/src/notify.c \
	%D%/../nd/src/pixbuf.c \
	$(null)

notify_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(NKUTILS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

notify_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(PLUGIN_LDFLAGS) \
	$(null)

notify_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(GDK_PIXBUF_LIBS) \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(NKUTILS_LIBS) \
	$(GLIB_LIBS) \
	$(null)
