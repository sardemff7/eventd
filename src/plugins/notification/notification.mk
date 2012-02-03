# notification plugin
plugins_LTLIBRARIES += \
	notification.la

notification_la_SOURCES = \
	src/plugins/notification/libnotify-compat.h \
	src/plugins/notification/notify.h \
	src/plugins/notification/icon.h \
	src/plugins/notification/icon.c \
	src/plugins/notification/daemon/daemon.h \
	src/plugins/notification/notification.h \
	src/plugins/notification/notification.c

notification_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"notification\" \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(NOTIFY_CFLAGS) \
	$(XCB_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GLIB_CFLAGS)

notification_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

notification_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(NOTIFY_LIBS) \
	$(XCB_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GLIB_LIBS)

if ENABLE_NOTIFICATION_DAEMON
notification_la_SOURCES += \
	src/plugins/notification/daemon/backends/fb.h \
	src/plugins/notification/daemon/backends/graphical.h \
	src/plugins/notification/daemon/style-internal.h \
	src/plugins/notification/daemon/style.h \
	src/plugins/notification/daemon/style.c \
	src/plugins/notification/daemon/bubble.h \
	src/plugins/notification/daemon/bubble.c \
	src/plugins/notification/daemon/types.h \
	src/plugins/notification/daemon/daemon.c

if ENABLE_XCB
notification_la_SOURCES += \
	src/plugins/notification/daemon/backends/xcb.c
endif

if ENABLE_LINUX_FB
notification_la_SOURCES += \
	src/plugins/notification/daemon/backends/linux.c
endif

else
notification_la_SOURCES += \
	src/plugins/notification/daemon/dummy.c
endif


if ENABLE_NOTIFY
notification_la_SOURCES += \
	src/plugins/notification/notify.c
else
notification_la_SOURCES += \
	src/plugins/notification/notify-dummy.c
endif
