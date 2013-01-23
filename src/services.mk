EXTRA_DIST += \
	$(dbussessionservice_DATA:=.in)

CLEANFILES += \
	$(dbussessionservice_DATA)

$(dbussessionservice_DATA): %: %.in src/config.h
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]bindir[@]:$(bindir):g' \
		< $< > $@ || rm $@
