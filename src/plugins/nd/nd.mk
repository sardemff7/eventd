# notification plugin
plugins_LTLIBRARIES += \
	nd.la

pkginclude_HEADERS += \
	include/eventd-nd-backend.h

nd_la_SOURCES = \
	src/plugins/nd/nd.c

nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-nd\" \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GDK_PIXBUF_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

nd_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	libeventd-nd.la \
	$(GOBJECT_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)

include src/plugins/nd/helper/libeventd-nd.mk
if ENABLE_CAIRO_HELPER
include src/plugins/nd/cairo/libeventd-nd-cairo.mk
endif

ndbackendsdir = $(pluginsdir)/nd

ndbackends_LTLIBRARIES =

if ENABLE_XCB
include src/plugins/nd/xcb/xcb.mk
endif

if ENABLE_LINUX_FB
include src/plugins/nd/linux/linux.mk
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

