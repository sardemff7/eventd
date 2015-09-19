
presenceeventdir = $(eventdir)/presence
imeventdir = $(eventdir)/im
chateventdir = $(eventdir)/chat

presenceevent_DATA = \
	%D%/presence/presence-away.event \
	%D%/presence/presence-away-message.event \
	%D%/presence/presence-back.event \
	%D%/presence/presence-back-message.event \
	%D%/presence/presence-idle-back.event \
	%D%/presence/presence-idle.event \
	%D%/presence/presence-signed-off.event \
	%D%/presence/presence-signed-on.event \
	%D%/presence/presence.event \
	$(null)

imevent_DATA = \
	%D%/im/im.event \
	$(null)

chatevent_DATA = \
	%D%/chat/chat.event \
	$(null)


EXTRA_DIST += \
	$(chatevent_DATA:.event=.event.in) \
	$(imevent_DATA:.event=.event.in) \
	$(presenceevent_DATA:.event=.event.in) \
	$(null)

CLEANFILES += \
	$(chatevent_DATA) \
	$(imevent_DATA) \
	$(presenceevent_DATA) \
	$(null)
