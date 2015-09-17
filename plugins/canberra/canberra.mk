# libcanberra plugin

if ENABLE_CANBERRA
plugins_LTLIBRARIES += \
	canberra.la \
	$(null)

man5_MANS += \
	plugins/canberra/man/eventd-canberra.event.5 \
	$(null)

fdonotificationscapabilities_DATA += \
	plugins/canberra/fdonotificationscapabilities/canberra.capabilities \
	$(null)
endif


canberra_la_SOURCES = \
	plugins/canberra/src/canberra.c \
	$(null)

canberra_la_CFLAGS = \
	$(AM_CFLAGS) \
	-D G_LOG_DOMAIN=\"eventd-canberra\" \
	$(CANBERRA_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

canberra_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-avoid-version -module \
	$(null)

canberra_la_LIBADD = \
	libeventd-event.la \
	libeventd-plugin.la \
	libeventd-helpers.la \
	$(CANBERRA_LIBS) \
	$(GLIB_LIBS) \
	$(null)


canberra_fdo_notifications_capabilities = \
	sound \
	$(null)

plugins/canberra/fdonotificationscapabilities/canberra.capabilities: src/config.h
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	echo $(canberra_fdo_notifications_capabilities) > $@

CLEANFILES += \
	plugins/canberra/fdonotificationscapabilities/canberra.capabilities \
	$(null)
