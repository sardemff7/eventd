# notification plugin
plugins_LTLIBRARIES += \
	notification.la

notification_la_SOURCES = \
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
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GLIB_LIBS)

if ENABLE_NOTIFICATION_DAEMON
notification_la_SOURCES += \
	src/plugins/notification/daemon/backends/backend.h \
	src/plugins/notification/daemon/style-internal.h \
	src/plugins/notification/daemon/style.h \
	src/plugins/notification/daemon/style.c \
	src/plugins/notification/daemon/bubble.h \
	src/plugins/notification/daemon/bubble.c \
	src/plugins/notification/daemon/types.h \
	src/plugins/notification/daemon/daemon.c

notificationbackendsdir = $(pluginsdir)/notification

notificationbackends_LTLIBRARIES =

if ENABLE_XCB
include src/plugins/notification/daemon/backends/xcb.mk
endif

if ENABLE_LINUX_FB
include src/plugins/notification/daemon/backends/linux.mk
endif

#
# Hooks
#

install-data-hook la-files-install-hook: notification-la-files-install-hook
uninstall-hook la-files-uninstall-hook: notification-la-files-uninstall-hook

# *.la files cleanup
notification-la-files-install-hook:
	cd $(DESTDIR)/$(notificationbackendsdir) && \
		rm $(notificationbackends_LTLIBRARIES)

# Remove *.so files at uninstall since
# we remove *.la files at install
notification-la-files-uninstall-hook:
	cd $(DESTDIR)/$(notificationbackendsdir) && \
		rm $(notificationbackends_LTLIBRARIES:.la=.so)
	rmdir $(DESTDIR)/$(notificationbackendsdir)

else
notification_la_SOURCES += \
	src/plugins/notification/daemon/dummy.c
endif
