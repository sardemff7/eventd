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
	$(GTK_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GLIB_CFLAGS)

eventd_LDADD = \
	$(GTK_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)


if ENABLE_SOUND
eventd_SOURCES += \
	src/eventd-pulse.h \
	src/eventd-pulse.c

eventd_CFLAGS += \
	$(SOUND_CFLAGS)

eventd_LDADD += \
	$(SOUND_LIBS)
endif

if ENABLE_NOTIFY
eventd_SOURCES += \
	src/eventd-notify.h \
	src/eventd-notify.c

eventd_CFLAGS += \
	$(NOTIFY_CFLAGS)

eventd_LDADD += \
	$(NOTIFY_LIBS)
endif
