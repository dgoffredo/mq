mq
=======
a POSIX message queue wrapper

Why
---
I don't want to have to write bindings to POSIX's `mq_*` functions for all of
the languages under the sun.  Instead, I want to be able to interact with a
message queue using an intermediate subprocess that I communicate with using
pipes.

What
----
`mq` is a C++ program that opens or creates a POSIX message queue based on its
command line arguments, and then provides send and/or recv access to the
message queue via a protocol read from stdin and written to stdout.  Errors go
to stderr.

How
---
Here's an example.  Standard input is here prefixed with a prompt "`> `" for
readability, though there would be no prompt in reality.

The example opens (or first creates with Unix permissions `600`) the message
queue named `/my-queue` (on many systems, queue names must be prefixed with a
`/`) for both reading and writing, and then sends and receives various messages
before closeping the queue:

    $ mq --open --create --read --write --permissions 600 /my-queue
    > send 47 hello, here is my message of length forty-seven
    ack 47
    > send 32 here is one of length thirty-two
    ack 32
    > recv
    47 hello, here is my message of length forty-seven
    > recv
    32 here is one of length thirty-two
    > send 3 one
    > send 3 two
    > send 8 goodbye!
    > consume
    3 one
    3 two
    8 goodbye!
    > close
    $

Here's an example that demonstrates the persistence of the queue.  It is
destroyed when the system goes down, or when it is `unlink`ed and no open file
descriptors remain for it:

    $ mq --open --create --read --write --permissions 600 /todo
    > send 19 send a few messages
    ack 19
    > send 10 like these
    ack 10
    > send 54 and read from another process after this one goes away
    ack 54
    > close
    $ echo Notice how now we can omit the "create" and "permissions" options.
    Notice how now we can omit the "create" and "permissions" options.
    $ mq --open --read /todo
    > consume
    19 send a few messages
    10 like these
    54 and read from another process after this one goes away
    > close
    $

More
----
What follows are descriptions of the wire protocol, how errors and handled and
reported, and build instructions.

### Wire Protocol
The wire protocol used to communicate with `mq` is a hybrid between a
whitespace delimited text protocol and a length-prefixed binary protocol.
Message payloads are prefixed by their length, in decimal as text, and so the
payloads themselves may contain aribitrary bytes without conflicting with
delimiter characters.  Otherwise, though, components in the wire protocol are
whitespace separated.

#### mq stdin
The standard input of `mq` is the pipe to which the user sends commands, such
as to send or receive a message from the queue.

##### Grammar

    stdin            ::=  (command (ws command)* ws?)?

    ws               ::=  /[\t\r\n ]+/

    command          ::=  send-command
                       |  recv-command
                       |  consume-command
                       |  close-command

    send-command     ::=  "send" ws length ws data

    length           ::=  "0"
                       |  /[1-9][0-9]*/

    data             ::=  /.*/

    recv-command     ::=  "recv"

    consume-command  ::=  "consume"

    close-command    ::=  "close"

##### Semantics
Any `data` prefixed by a `length` must have that length.  The `close` command
exists so that the user can receive any already-popped messages from the queue,
or `ack` responses from `mq`, before closing the pipe, thus preventing messages
from being lost on shutdown.  Lengths are base ten non-negative integers in
text.

#### mq stdout
`mq` responds to the user's commands through its standard output pipe:  popped
messages and acknowledgements of messages sent.

##### Grammar

    stdout    ::=  (response (ws response)* ws?)?

    ws        ::=  /[\t\r\n ]+/
    
    response  ::=  msg
                |  ack

    msg       ::=  length ws data

    ack       ::=  "ack" length

    length    ::=  "0"
                |  /[1-9][0-9]*/

    data      ::=  /.*/

##### Semantics
The `length` in a `msg` response is the length of the `data`.  The `length` in
an `ack` message is the length of the `data` in the sent messages that is being
acknowledged.  Lengths are base ten non-negative integers in text.

#### mq stderr

##### Grammar

    stderr      ::=  (diagnostic newline)*

    newline     ::=  /[\n]/

    diagnostic  ::=  /[^\n]*/

##### Semantics
Each `diagnostic` is a human-readable English language description of the error
that occurred.  The text encoding is UTF-8 (but without the newline character).

### Error Handling
Any error reported to the user through `mq`'s standard error pipe is grounds
for terminating the `mq` process.  It will not terminate itself on purpose,
and many errors are recoverable, but for simplicity's sake it's best to start
again when an error occurs.

### Build
The `mq` binary is built in place using the `Makefile`:

    $ make
    $ ls mq