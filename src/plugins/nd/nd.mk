# notification plugin
plugins_LTLIBRARIES += \
	nd.la

pkginclude_HEADERS += \
	include/eventd-nd-backend.h

nd_la_SOURCES = \
	src/plugins/nd/cairo.c \
	src/plugins/nd/cairo.h \
	src/plugins/nd/style.c \
	src/plugins/nd/style.h \
	src/plugins/nd/backends.c \
	src/plugins/nd/backends.h \
	src/plugins/nd/nd.c

nd_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-nd\" \
	-D SYSCONFDIR=\"$(sysconfdir)\" \
	-D LIBDIR=\"$(libdir)\" \
	-D DATADIR=\"$(datadir)\" \
	$(GDK_PIXBUF_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

nd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

nd_la_LIBADD = \
	libeventd-event.la \
	libeventd.la \
	$(GDK_PIXBUF_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)


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

 ifneq (,$(ndbackends_LTLIBRARIES))
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
 endif
