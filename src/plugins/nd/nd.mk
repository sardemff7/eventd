# notification plugin
plugins_LTLIBRARIES += \
	nd.la

nd_la_SOURCES = \
	src/plugins/nd/icon.h \
	src/plugins/nd/icon.c \
	src/plugins/nd/daemon/backends/backend.h \
	src/plugins/nd/daemon/style-internal.h \
	src/plugins/nd/daemon/style.h \
	src/plugins/nd/daemon/style.c \
	src/plugins/nd/daemon/bubble.h \
	src/plugins/nd/daemon/bubble.c \
	src/plugins/nd/daemon/types.h \
	src/plugins/nd/daemon/daemon.h \
	src/plugins/nd/daemon/daemon.c \
	src/plugins/nd/nd.h \
	src/plugins/nd/nd.c

nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-nd\" \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(NOTIFY_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(GLIB_CFLAGS)

nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module -export-symbols-regex eventd_plugin_get_info

nd_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(NOTIFY_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GLIB_LIBS)

ndbackendsdir = $(pluginsdir)/nd

ndbackends_LTLIBRARIES =

if ENABLE_XCB
include src/plugins/nd/daemon/backends/xcb.mk
endif

if ENABLE_LINUX_FB
include src/plugins/nd/daemon/backends/linux.mk
endif

#
# Hooks
#

install-data-hook la-files-install-hook: nd-la-files-install-hook
uninstall-hook la-files-uninstall-hook: nd-la-files-uninstall-hook

# *.la files cleanup
nd-la-files-install-hook:
	cd $(DESTDIR)/$(ndbackendsdir) && \
		rm $(ndbackends_LTLIBRARIES)

# Remove *.so files at uninstall since
# we remove *.la files at install
nd-la-files-uninstall-hook:
	cd $(DESTDIR)/$(ndbackendsdir) && \
		rm $(ndbackends_LTLIBRARIES:.la=.so)
	rmdir $(DESTDIR)/$(ndbackendsdir)

