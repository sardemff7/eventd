# libcanberra plugin

if ENABLE_CANBERRA
plugins_LTLIBRARIES += \
	canberra.la \
	$(null)

man5_MANS += \
	plugins/canberra/man/eventd-canberra.event.5 \
	$(null)

dist_fdonotificationscapabilities_DATA += \
	plugins/canberra/fdonotificationscapabilities/canberra.capabilities \
	$(null)
endif


canberra_la_SOURCES = \
	plugins/canberra/src/canberra.c \
	$(null)

canberra_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(CANBERRA_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

canberra_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

canberra_la_LIBADD = \
	libeventd.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(CANBERRA_LIBS) \
	$(GLIB_LIBS) \
	$(null)
