ACLOCAL_AMFLAGS  = -I m4

AM_CFLAGS = \
	$(GLOBAL_CFLAGS)

AM_LDFLAGS = \
	$(GLOBAL_LDFLAGS)

bin_PROGRAMS = \
	eventd

eventd_SOURCES = \
	src/eventd.c

eventd_CFLAGS = \
	$(PULSE_CFLAGS) \
	$(NOTIFY_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	$(PULSE_LIBS) \
	$(NOTIFY_LIBS) \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
