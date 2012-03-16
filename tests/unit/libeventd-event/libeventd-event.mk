TESTS += \
	tests/unit/libeventd-event.test

check_PROGRAMS += \
	tests/unit/libeventd-event.test


tests_unit_libeventd_event_test_SOURCES = \
	tests/unit/libeventd-event/getters.c \
	tests/unit/libeventd-event/getters.h \
	tests/unit/libeventd-event/setters.c \
	tests/unit/libeventd-event/setters.h \
	tests/unit/libeventd-event/common.h \
	tests/unit/libeventd-event/libeventd-event.c

tests_unit_libeventd_event_test_CFLAGS = \
	$(AM_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GOBJECT_CFLAGS) \
	$(GLIB_CFLAGS)

tests_unit_libeventd_event_test_LDADD = \
	libeventd-event.la \
	$(GIO_LIBS) \
	$(GOBJECT_LIBS) \
	$(GLIB_LIBS)
