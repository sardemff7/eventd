# notify plugin

if ENABLE_NOTIFY
plugins_LTLIBRARIES += \
	notify.la

man5_MANS += \
	plugins/notify/man/eventd-notify.event.5

dist_event_DATA += \
	plugins/notify/events/libnotify.event
endif


notify_la_SOURCES = \
	plugins/notify/src/image.h \
	plugins/notify/src/image.c \
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
	libeventd-plugin.la \
	libeventd.la \
	$(NOTIFY_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
