# Basic CLI client
bin_PROGRAMS += \
	eventc

eventc_SOURCES = \
	src/eventc/eventc.vala

eventc_VALAFLAGS = \
	$(AM_VALAFLAGS) \
	--pkg libeventd-event \
	--pkg libeventc \
	$(GIO_VALAFLAGS) \
	$(GOBJECT_VALAFLAGS) \
	$(GLIB_VALAFLAGS)

eventc_CFLAGS = \
	$(AM_CFLAGS) \
	$(AM_VALA_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

eventc_LDADD = \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
