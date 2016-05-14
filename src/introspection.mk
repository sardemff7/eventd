-include $(INTROSPECTION_MAKEFILE)

if HAVE_INTROSPECTION
gir_DATA = $(INTROSPECTION_GIRS)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

# .git.files files are generated by the
# MINGW-patched introspection Makefile
CLEANFILES += \
	$(gir_DATA) \
	$(gir_DATA:.gir=.gir.files) \
	$(typelib_DATA) \
	$(null)
endif
