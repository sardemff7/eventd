EXTRA_DIST += \
	$(dbussessionservice_DATA:=.in) \
	$(null)

CLEANFILES += \
	$(dbussessionservice_DATA) \
	$(null)

$(dbussessionservice_DATA): %: %.in $(CONFIG_HEADER) %D%/services.mk
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
		$(SED) \
		-e 's:[@]bindir[@]:$(bindir):g' \
		< $< > $@ || rm $@
