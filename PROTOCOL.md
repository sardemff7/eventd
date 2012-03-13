This file describes the EVENT protocol



Handshake
---------------

[Client]
HELLO <type>
    Initiate the connection by giving
    the client application type
    this type will be used to search for
    a configuration file
    The client may specify a name what
    will be used to display events

[Server]
HELLO
    Answer to the client HELLO message

[Client]
MODE <mode>
    Inform the server of the mode used
    by the client. You can omit this message
    for now (compatibility with the old
    protocol)

[Server]
MODE
    Answer to the client MODE message


Eventd dispatching
------------------

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
OK
    Acknowledge the event


Closing the connection
----------------------

[Client]
BYE
    Close the connection

[Server]
BYE
    Answer to the BYE message
