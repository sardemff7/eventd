# notification plugin

AM_CFLAGS += \
	-I $(srcdir)/plugins/nd/include


plugins_LTLIBRARIES += \
	nd.la

pkginclude_HEADERS += \
	plugins/nd/include/eventd-nd-backend.h

man5_MANS += \
	plugins/nd/man/eventd-nd.conf.5 \
	plugins/nd/man/eventd-nd.event.5


nd_la_SOURCES = \
	plugins/nd/src/icon.h \
	plugins/nd/src/cairo.c \
	plugins/nd/src/cairo.h \
	plugins/nd/src/style.c \
	plugins/nd/src/style.h \
	plugins/nd/src/backends.c \
	plugins/nd/src/backends.h \
	plugins/nd/src/nd.c

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
	libeventd-plugin.la \
	libeventd.la \
	$(GDK_PIXBUF_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)

if ENABLE_GDK_PIXBUF
nd_la_SOURCES += \
	plugins/nd/src/icon.c
endif

ndbackendsdir = $(pluginsdir)/nd

ndbackends_LTLIBRARIES =

if ENABLE_XCB
include plugins/nd/xcb/xcb.mk
endif

if ENABLE_LINUX_FB
include plugins/nd/linux/linux.mk
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
