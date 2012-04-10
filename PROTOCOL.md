This file describes the EVENT protocol



Handshake
---------------

[Client]
HELLO <type>
    Initiate the connection by giving
    the client application type
    this type will be used to search for
    a configuration file

[Server]
HELLO
    Answer to the client HELLO message

[Server]
ERROR bad-handshake
    Inform the client that the event was rejected
    due to an erroneous message


Eventd dispatching
------------------

[Server]
ERROR unknown
    Inform the client that this message is unknown

[Client]
EVENT <type>
    Inform the server that an event happened
    The type must contain only the characters
    A-Za-z0-9- as for keys of the
    Desktop Entry Specification files
    http://freedesktop.org/wiki/Specifications/desktop-entry-spec
    After the event, the client may specify data using
    one or more DATA message

[Client]
CATEGORY <type>
    Specify the real client type of the event
    Mainly targeted for relay clients

[Client]
DATA <name>
    This message is followed by the corresponding data
    Data end with a line containing "."
    If data contain a line with the first character being ".",
    it has to be replaced by ".."
    On server side, ".." at the beginning of a line
    has to be replaced by "."

[Client]
DATAL <name> <data>
    Simple data in one line, up to "\n"

[Client]
.
    Inform the server of the end of the
    EVENT message

[Server]
EVENT <id>
    Acknowledge the event
    The id may be anything represented as a string

[Server]
ERROR bad-event
    Inform the client that the event message was not valid


Closing the connection
----------------------

[Client]
BYE
    Close the connection
    The server must not send any data after this
    message as the connection is closed
