# notify plugin

XSLTPROC_CONDITIONS += enable_notify


plugins_LTLIBRARIES += \
	notify.la


notify_la_SOURCES = \
	plugins/notify/src/icon.h \
	plugins/notify/src/icon.c \
	plugins/notify/src/libnotify-compat.h \
	plugins/notify/src/notify.c

notify_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-notify\" \
	$(NOTIFY_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

notify_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

notify_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(NOTIFY_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
