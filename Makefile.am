
bin_PROGRAMS = \
	eventd

eventd_SOURCES = \
	src/eventd.c

eventd_CFLAGS = \
	$(PULSE_CFLAGS) \
	$(NOTIFY_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	$(PULSE_LIBS) \
	$(NOTIFY_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
