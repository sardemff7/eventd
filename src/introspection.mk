-include $(INTROSPECTION_MAKEFILE)

if HAVE_INTROSPECTION
gir_DATA = $(INTROSPECTION_GIRS)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += \
	$(gir_DATA) \
	$(typelib_DATA) \
	$(null)
endif
