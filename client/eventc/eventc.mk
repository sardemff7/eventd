# Basic CLI client
bin_PROGRAMS += \
	eventc \
	$(null)

eventc_SOURCES = \
	client/eventc/src/eventc.c \
	$(null)

eventc_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(null)

eventc_LDADD = \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS) \
	$(null)
