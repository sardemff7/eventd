EXTRA_DIST += \
	$(systemduserunit_DATA:=.in)

CLEANFILES += \
	$(systemduserunit_DATA)

$(systemduserunit_DATA): %: %.in src/config.h
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]bindir[@]:$(bindir):g' \
		-e 's:[@]PACKAGE_NAME[@]:$(PACKAGE_NAME):g' \
		-e 's:[@]UNIX_SOCKET[@]:$(UNIX_SOCKET):g' \
		-e 's:[@]DEFAULT_BIND_PORT[@]:$(DEFAULT_BIND_PORT):g' \
		< $< > $@ || rm $@

