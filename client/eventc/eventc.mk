# Basic CLI client
bin_PROGRAMS += \
	eventc

eventc_SOURCES = \
	client/eventc/src/eventc.c

eventc_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

eventc_LDADD = \
	libeventd-event.la \
	libeventc.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)

eventc client/eventc/src/eventc-eventc.o: $(srcdir)/libeventc_la_vala.stamp
