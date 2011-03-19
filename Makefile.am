ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = \
	$(GLOBAL_CFLAGS)

AM_LDFLAGS = \
	$(GLOBAL_LDFLAGS)

bin_PROGRAMS = \
	eventd

eventd_SOURCES = \
	src/eventd-events.h \
	src/eventd-events.c \
	src/eventd-service.h \
	src/eventd-service.c \
	src/eventd.h \
	src/eventd.c

eventd_CFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
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
