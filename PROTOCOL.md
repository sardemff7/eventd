This file describes the EVENT protocol



Data
----

.DATA <name>
    This message is followed by the corresponding data
    Data end with a line containing "."
    If data contain a line with the first character being ".",
    it has to be replaced by ".."
    On server side, ".." at the beginning of a line
    has to be replaced by "."

DATAL <name> <data>
    Simple data in one line, up to "\n"


Eventd dispatching
------------------

[Client]
.EVENT <id> <category> <type>
    Inform the server that an event happened
    The type must contain only the characters
    A-Za-z0-9- as for keys of the
    Desktop Entry Specification files
    http://freedesktop.org/wiki/Specifications/desktop-entry-spec
    The client may specify data using the corresponding messages

[Client]
ANSWER <name>
    Add a possible answer to the event

[Client]
.
    Inform the server of the end of the
    EVENT message

[Client]
END <id>
    Force the end of an event before the timeout
    Will trigger an ENDED message with "client-dismiss" reason



Event timeout or answer
-----------------------

[Server]
ENDED <id> <reason>
    Inform the client of an event ending

[Server]
.ANSWERED <id> <name>
    Inform the client of an answer to an event
    The server may specify data using the corresponding messages

[Server]
.
    Inform the server of the end of the
    ANSWER


Closing the connection
----------------------

[Client]
BYE
    Close the connection
    The server must not send any data after this
    message as the connection is closed
