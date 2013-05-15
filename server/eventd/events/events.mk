
presenceeventdir = $(eventdir)/presence
imeventdir = $(eventdir)/im
chateventdir = $(eventdir)/chat

presenceevent_DATA = \
	server/eventd/events/presence/presence-away.event \
	server/eventd/events/presence/presence-away-message.event \
	server/eventd/events/presence/presence-back.event \
	server/eventd/events/presence/presence-back-message.event \
	server/eventd/events/presence/presence-idle-back.event \
	server/eventd/events/presence/presence-idle.event \
	server/eventd/events/presence/presence-signed-off.event \
	server/eventd/events/presence/presence-signed-on.event \
	server/eventd/events/presence/presence.event

imevent_DATA = \
	server/eventd/events/im/im.event

chatevent_DATA = \
	server/eventd/events/chat/chat.event


EXTRA_DIST += \
	$(chatevent_DATA:.event=.event.in) \
	$(imevent_DATA:.event=.event.in) \
	$(presenceevent_DATA:.event=.event.in)

CLEANFILES += \
	$(chatevent_DATA) \
	$(imevent_DATA) \
	$(presenceevent_DATA)
