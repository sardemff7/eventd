# IM plugin

if ENABLE_IM
plugins_LTLIBRARIES += \
	im.la

man5_MANS += \
	plugins/im/man/eventd-im.conf.5 \
	plugins/im/man/eventd-im.event.5
endif


im_la_SOURCES = \
	plugins/im/src/im.c

im_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-im\" \
	$(PURPLE_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS)

im_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module

im_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	libeventd.la \
	$(PURPLE_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS)
