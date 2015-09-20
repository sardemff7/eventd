# notify plugin

if ENABLE_NOTIFY
plugins_LTLIBRARIES += \
	notify.la \
	$(null)

man5_MANS += \
	%D%/man/eventd-notify.event.5 \
	$(null)
endif


notify_la_SOURCES = \
	%D%/src/image.h \
	%D%/src/image.c \
	%D%/src/notify.c \
	$(null)

notify_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(NOTIFY_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

notify_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

notify_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(NOTIFY_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
