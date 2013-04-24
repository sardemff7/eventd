This file describes the EVENT protocol


Messages are UTF-8 text which ends at the "\n" character.
Messages starting with a dot (".") are multiple-lines messages and ends with a
single dot on its own line (".\n"). They are referred as "dot messages".

An implementation should ignore unknown messages for backward compatibility.
Existing messages will not be nested in future messages, thus ignoring lines
without a known message should be enough.


Data
----

.DATA <name>
    This message is followed by the corresponding data.
    Data end with a line containing a single dot (".\n").
    If data contain a line with the first character being dot ("."),
    it has to be replaced by two dots ("..").
    On the other side, two dots ("..") at the beginning of a line
    has to be replaced by a single dot (".").

DATA <name> <data>
    Simple data in one line, up to "\n" (non included).


Eventd dispatching
------------------

.EVENT <id> <category> <type>
    Inform the server that an event happened
    The id is an arbritary string representing the event on the client side.
    The type must contain only the characters
    A-Za-z0-9- as for keys of the
    [Desktop Entry Specification files
    http://freedesktop.org/wiki/Specifications/desktop-entry-spec].
    The message may contain data using the corresponding messages.
    The message may contain possible answers using the corresponding message.

ANSWER <name>
    Add a possible answer to the event

END <id>
    Force the end of an event before the timeout.
    Will trigger an ENDED message with "client-dismiss" reason.



Event timeout or answer
-----------------------

ENDED <id> <reason>
    Inform the client of an event ending.

.ANSWERED <id> <name>
    Inform the client of an answer to an event.
    The server may specify data using the corresponding messages.


Closing the connection
----------------------

BYE
    Close the connection.
    The connection is closed immediately and must not be used any more.
