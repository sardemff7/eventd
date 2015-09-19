# IM plugin

if ENABLE_IM
plugins_LTLIBRARIES += \
	im.la \
	$(null)

man5_MANS += \
	plugins/im/man/eventd-im.conf.5 \
	plugins/im/man/eventd-im.event.5 \
	$(null)
endif


im_la_SOURCES = \
	plugins/im/src/im.c \
	$(null)

im_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(PURPLE_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

im_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

im_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(PURPLE_LIBS) \
	$(GMODULE_LIBS) \
	$(GLIB_LIBS) \
	$(null)
