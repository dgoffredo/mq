
// POSIX
#include <errno.h>     // error codes
#include <fcntl.h>     // file open constants
#include <mqueue.h>    // mq_*
#include <pthread.h>   // pthread_*
#include <signal.h>    // SIGUSR1
#include <string.h>    // strerror
#include <sys/stat.h>  // file mode constants 
#include <unistd.h>    // write

// Standard C
#include <stdio.h>     // snprintf, fileno

// Standard C++
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>     // std::exit
#include <iostream>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

#include "repr.h"

// --------------------
// command line parsing
// --------------------

void usage(const char *argv0, std::ostream& out)
{
    out << "usage: " << argv0 << "  <options ...>  <message queue>\n"
        << "       " << argv0 << " --help\n";
}

void help(const char *argv0, std::ostream& out)
{
    out << argv0 << 
"  <Options ...>  <Message Queue>\n"
"\n"
"Options:\n"
"--help      print this prompt to standard output\n"
"--readme    print the README file for this utility\n"
"--read      open the queue receiving messages\n"
"--write     open the queue for sending messages\n"
"--open      open the queue (if without --create, then only if existing)\n"
"--create    create the queue (if without --open, then exclusively)\n"
"--permissions <octal>    Unix file permissions to use if creating queue \n"
"--maxmsg    maximum number of messages to allow in the queue, if possible\n"
"--msgsize   maximum size of any message in the queue, if possible\n"
"--unlink    unlink the specified message queue (see MQ_UNLINK(3))\n"
"--debug     print to stderr trace useful when debugging\n"
"\n"
"If --unlink is specified, the only other option that may be specified is\n"
"--debug.\n"
"\n"
"Otherwise, at least one of --create and/or --open must be specified, and at\n"
"least one of --read and/or --write must be specified.  If either of\n"
"--maxmsg or --msgsize is specified, then the other must be specified as\n"
"well.\n"
"\n"
"Message Queue:\n"
"The name of the POSIX local message queue to open (and possibly create).\n"
"Note that on many systems, message queue names are required to begin with a\n"
"forward slash.  Also note that message queues are not necessarily visible\n"
"on the file system.\n";
}

const char README[] = 
"<make, insert README here>"
;

void readme() {
    std::cout << README;
}

struct FindArgs {
    const char *const *const begin;
    const char *const *const end;
    FindArgs(const char *const *begin, const char *const *end)
    : begin(begin)
    , end(end)
    {}

    const char *const *operator()(const std::string& argument) const {
        const char *const *const result = std::find(begin, end, argument);
        return result == end ? 0 : result;
    }
};

int checkArgs(int argc, const char *const argv[])
{
    if (argc < 2) {
        usage(argv[0], std::cerr);
        return 1;
    }

    const FindArgs find(argv + 1, argv + argc);

    if (find("--help") || find("-h")) {
        help(argv[0], std::cout);
        std::exit(0);
    }

    if (find("--readme")) {
        readme();
        std::exit(0);
    }

    if (argv[argc - 1][0] == '-') {
        std::cerr << "Final argument must be a non-option (the queue name).\n";
        return 6;
    }

    const bool unlink = find("--unlink");

    if (unlink && ((find("--debug") && argc != 4) || argc != 3)) {
        std::cerr << "--unlink must be alone or with --debug.\n";
        return 2;
    }

    if (unlink)
        return 0;  // don't need to enforce other requirements

    if (!find("--read") && !find("--write")) {
        std::cerr << "One or both of --read and --write must be specified.\n";
        return 3;
    }

    if (!find("--open") && !find("--create")) {
       std::cerr << "One or both of --open and --create must be specified.\n";
       return 4;
    }

    if (bool(find("--msgsize")) != bool(find("--maxmsg"))) {
        std::cerr << "Specify neither of both of --msgsize and --maxmsg.\n";
        return 5;
    }

    // Make sure there aren't any unsupported flags specified, like
    // --chicken-dinner
    const char *const flags[] = { 
        "read", "write", "open", "create", "msgsize", "maxmsg", "readme",
        "debug"
    };
    const char *const *const endFlags =
        flags + sizeof(flags) / sizeof(flags[0]);

    // For each argument passed (except the last one), if it looks like a flag
    // (begins with "--"), then make sure the rest of it is in 'flags'.
    for (const char *const *it = argv + 1; it != argv + argc - 1; ++it)
    {
        const std::string arg(*it);
        if (   arg.size() > 2
            && arg.substr(0, 2) == "--"
            && std::find(flags, endFlags, arg.substr(2)) == endFlags)
        {
            std::cerr << "Unrecognized option " << repr(arg) << '\n';
            return 6;
        }
    }

    return 0;
}

struct Options {
    enum { READ_ONLY, WRITE_ONLY,  READ_WRITE }  operation;
    enum { OPEN_ONLY, CREATE_ONLY, OPEN_CREATE } open;
    int                                          filePermissions;
    bool                                         maxesSpecified;
    ssize_t                                      maxmsg;
    ssize_t                                      msgsize;                  
    bool                                         unlink;
    bool                                         debug;
    std::string                                  queueName;

    Options()
    : operation(READ_WRITE)
    , open(OPEN_CREATE)
    , filePermissions(0600)
    , maxesSpecified(false)
    , maxmsg(-1)   // arbitrarily chosen
    , msgsize(-1)  // arbitrarily chosen
    , unlink(false)
    , debug(false)
    {}
};

Options parseOptions(int argc, const char *const argv[])
{
    const FindArgs find(argv + 1, argv + argc);

    Options options;

    options.debug = find("--debug");   
 
    options.queueName = argv[argc - 1];

    if (find("--unlink")) {
        options.unlink = true;
        return options;  // the rest can be ignored.
    }

    const bool read  = find("--read"),
               write = find("--write");
    options.operation = read && write ? Options::READ_WRITE
                                      : read ? Options::READ_ONLY
                                             : Options::WRITE_ONLY;

    const bool open   = find("--open"),
               create = find("--create");
    options.open = open && create ? Options::OPEN_CREATE
                                  : open ? Options::OPEN_ONLY
                                         : Options::CREATE_ONLY;

    struct ToSsize {
        ssize_t operator()(const char *string) const {
            std::stringstream ss;
            ss << string;
            ssize_t result;
            ss >> result;
            return result;
        }
    } const toSsize;

    const char *const *const maxmsgOption = find("--maxmsg");
    if (maxmsgOption) {
        options.maxmsg = toSsize(*(maxmsgOption + 1));
    }

    const char *const *const msgsizeOption = find("--msgsize");
    if (msgsizeOption) {
        options.msgsize = toSsize(*(msgsizeOption + 1));
    }

    options.maxesSpecified = maxmsgOption || msgsizeOption;

    return options;
}

// -------------------------
// message queue open/create
// -------------------------

mqd_t openQueue(const Options& options)
{
    int openFlags = 0;
    switch (options.operation) {
      case Options::READ_ONLY:  openFlags |= O_RDONLY; break;
      case Options::WRITE_ONLY: openFlags |= O_WRONLY; break;
      default:
        assert(options.operation == Options::READ_WRITE);
        openFlags |= O_RDWR;
    }

    switch (options.open) {
      case Options::OPEN_ONLY: break;
      case Options::CREATE_ONLY: openFlags |= O_EXCL | O_CREAT; break;
      default: 
        assert(options.open == Options::OPEN_CREATE);
        openFlags |= O_CREAT;
    }

    mq_attr  attributes;
    mq_attr *attributesPtr = 0;
    if (options.maxesSpecified) {
        attributes.mq_maxmsg  = options.maxmsg;
        attributes.mq_msgsize = options.msgsize;
        attributesPtr         = &attributes;
    }

    if (options.debug) {
        std::cerr << "Attempting to open a message queue named "
                  << repr(options.queueName) << std::endl;
    }

    return mq_open(options.queueName.c_str(),
                   openFlags, 
                   options.filePermissions, 
                   attributesPtr);
}

struct Shared {
    // Data shared between threads: mutexes, the message queue descriptor, and
    // some other misc.

    pthread_mutex_t stoppedMutex;
    bool            stopped;
    pthread_mutex_t stdoutMutex;
    const mqd_t     queue;
    pthread_mutex_t stderrMutex;
    const ssize_t   msgsize;
    bool            consumerThreadExists;
    pthread_t       consumerThread;
    const Options&  options;

    explicit Shared(mqd_t          messageQueue,
                    ssize_t        messageSize,
                    const Options& commandLineOptions)
    : stopped(false)
    , queue(messageQueue)
    , msgsize(messageSize)
    , consumerThreadExists(false)
    , options(commandLineOptions)
    {
        const pthread_mutexattr_t *const defaultAttributes = 0;

        pthread_mutex_init(&stoppedMutex, defaultAttributes);
        pthread_mutex_init(&stdoutMutex,  defaultAttributes);
        pthread_mutex_init(&stderrMutex,  defaultAttributes);
    }

    ~Shared()
    {
        pthread_mutex_destroy(&stoppedMutex);
        pthread_mutex_destroy(&stdoutMutex);
        pthread_mutex_destroy(&stderrMutex);
    }
};

class Lock {
    pthread_mutex_t& mutex;
    bool             holding;

  public:
    explicit Lock(pthread_mutex_t& mutex, bool lockOnInit = true)
    : mutex(mutex)
    , holding(false)
    {
        if (lockOnInit)
            acquire();
    }

    void acquire() {
        if (holding)
            return;

        pthread_mutex_lock(&mutex);
        holding = true;
    }

    void release() {
        if (!holding)
            return;

        pthread_mutex_unlock(&mutex);
        holding = false;
    }

    ~Lock()
    {
        if (holding)
            release();
    }
};


// -----------------
// handling commands
// -----------------

int sendHandler(std::string& chunk, Shared& shared)
{
    unsigned priority;
    std::cin >> priority;
    if (!std::cin) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Unable to read message priority from \"send\" command."
                  << std::endl;
        return 1;
    }

    ssize_t size;
    std::cin >> size;
    if (!std::cin) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Unable to read message size from \"send\" command." 
                  << std::endl;
        return 2;
    }
    if (size < 0) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Messages must have a non-negative size. Size " << size
                  << " is not permitted." << std::endl;
        return 4;
    } 

    std::cin.ignore();  // Discard space character between size and payload.

    if (size) {
        chunk.resize(size);
        std::cin.read(&chunk[0], size);
        if (!std::cin || std::cin.gcount() != size) {
            Lock lock(shared.stderrMutex, shared.consumerThreadExists);
            std::cerr << "Unable to read from input all of the supposed "
                      << size << " byte message. " << std::cin.gcount()
                      << " were read instead." << std::endl;
            return 5;
        }
    }

    // looping for retry on signal interruption
    for (;;) {
        const ssize_t rc = mq_send(shared.queue, 
                                   chunk.data(), 
                                   size, 
                                   priority);
        if (rc == -1) {
            // failed to send
            const int error = errno;
            if (error != EINTR) {
                Lock lock(shared.stderrMutex, shared.consumerThreadExists);
                std::cerr << "Unable to send message for \"send\" command: "
                          << strerror(error) << std::endl;
                return 3;
            }
        }
        else {
            // send succeeded
            break;
        }
    }

    Lock lock(shared.stdoutMutex, shared.consumerThreadExists);
    std::cout << "ack " << size << std::endl;

    return 0;
}

const int FAIL_RECEIVE               = 1,
          FAIL_WRITE                 = 2,
          FAIL_ALLOC                 = 3,
          FAIL_INTERRUPTED_OR_CLOSED = 4;

int doReceive(std::string& buffer, Shared& shared)
    // Receive a message from a POSIX message queue and print the message to
    // standard output, prefixed by its priority and length.  'doReceive' is
    // used by both 'receiveHandler' and 'consume'.
{
    // Note on the implementation: This code goes out of its way to arrange the
    // output contiguously in memory before calling 'write'.  In part this
    // makes the 'write' call simpler, but also it's a (perhaps unnecessary)
    // optimization that minimizes the number of syscalls ('write' is called
    // only once per message in the common case).
    //
    // The message, once received, will be printed to stdout prefixed with its
    // priority and size, e.g. a priority-2 message containing "hello" would
    // be written to stdout as "2 5 hello\n" (without the null terminator).
    // 'numbersMaxSize' is the maximum possible number of characters that could
    // be necessary for the "2 5 " prefix.
    //
    const int numbersMaxSize =
        std::numeric_limits<unsigned>::digits10 +  // priority
        1 +  // separating whitespace
        1 +  // minus sign for the ssize_t, even though it won't happen
        std::numeric_limits<ssize_t>::digits10  +  // message size
        1;   // null terminator, which will be converted into a space

    try {
        buffer.resize(numbersMaxSize +  // <priority> <space> <size> <space>
                      shared.msgsize +  // the payload received from the queue
                      1);               // a trailing newline
    }
    catch (const std::bad_alloc&) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Failed to allocate memory for consuming messages."
                  << std::endl;
        return FAIL_ALLOC;
    }

    assert(!buffer.empty());
    char *const bufferBegin = &buffer[0];
    char *const msgBegin    = bufferBegin + numbersMaxSize;

    assert(buffer.size() >= numbersMaxSize);
    // The room available for the message itself is the full size of the buffer
    // minus the space reserved for the prefix and for the trailing newline.
    const size_t msgBufferSize = size_t(buffer.size() - numbersMaxSize - 1);

    if (shared.options.debug) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "About to receive with"
                     " buffer.size()=" << buffer.size()
                  << " numbersMaxSize=" << numbersMaxSize
                  << " msgBufferSize=" << msgBufferSize
                  << " shared.msgsize=" << shared.msgsize << std::endl;     
    }

    unsigned priority;
    const ssize_t msgSize = mq_receive(shared.queue, 
                                       msgBegin, 
                                       msgBufferSize, 
                                       &priority);
    // Handle mq_receive failure.
    if (msgSize == -1) {
        const int error = errno;
        if (error == EINTR || error == EBADF) {
            return FAIL_INTERRUPTED_OR_CLOSED;
        }

        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Failed to receive message: " << strerror(error)
                  << std::endl;
        return FAIL_RECEIVE;
    }

    if (shared.options.debug) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "received a priority " << priority << " message of size "
                  << msgSize << std::endl;
    } 

    assert(msgSize >= 0);

    // Put a newline character after the retrieved message.
    msgBegin[msgSize] = '\n';

    // Now calculate how much space is needed before the message to write
    // <priority> <size> 
    struct SizeBase10 {
        int operator()(double number) const {
            if (number == 0)
                return 1;

            return std::floor(1 + std::log10(number)) + // number of digits
                   int(number < 0);  // minus sign
       }
    } const sizeBase10;

    const int numbersExpectedSize =
        sizeBase10(priority) +
        1 +  // separating whitespace
        sizeBase10(msgSize) +
        1;   // null terminator, which will be converted into a space

    if (shared.options.debug) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "calculated numbersExpectedSize=" << numbersExpectedSize
                  << std::endl;
    } 

    // Format the numeric prefixes to the message payload starting at a
    // position in the buffer such that the end of the prefixes will be
    // just before the payload.
    char *const numbersBegin =
        bufferBegin + (numbersMaxSize - numbersExpectedSize);                 

    const std::ptrdiff_t spaceRemaining =
        bufferBegin + buffer.size() - numbersBegin;

    // The "+ 1" is to account for the null terminator written (snprinf
    // does not account for the null terminator in its return value).
    const int numbersSize = snprintf(numbersBegin,
                                     spaceRemaining, 
                                     "%u %lld",
                                     priority,
                                     static_cast<long long>(msgSize)) + 1;

    if (shared.options.debug) {
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "measured numbersSize=" << numbersSize << std::endl;
    } 

    assert(numbersSize == numbersExpectedSize);

    // Overwrite the prefix's trailing null character with a space.
    numbersBegin[numbersSize - 1] = ' ';

    const int stdoutFd = fileno(stdout);  // always equal to one

    const char *const outputBegin = numbersBegin;
    const size_t      outputSize  = numbersSize +  // the prefix
                                    msgSize +  // the payload
                                    1;  // newline character

    // Write the message priority, size, and contents to stdout.
    Lock stdoutLock(shared.stdoutMutex, shared.consumerThreadExists);

    for (ssize_t written = 0; written != ssize_t(outputSize);) {
        const ssize_t rc = write(stdoutFd, outputBegin, outputSize);
        if (rc == -1) {
            const int error = errno;
            if (error == EINTR) {
                continue;  // interrupted by signal before writing. Retry.
            }

            stdoutLock.release();  // no need to hold stdout during error
            Lock lock(shared.stderrMutex, shared.consumerThreadExists);
            std::cerr << "Failed to return message: " << strerror(error)
                      << std::endl;
            return FAIL_WRITE;
        }

        written += rc;
    }

    return 0;  
}

int receiveHandler(std::string& buffer, Shared& shared)
{
    // Assume that if receive fails, it won't be due to the queue having been
    // closed, because only a 'close' handler would close the queue.
    // That means that if we get 'FAIL_INTERRUPTED_OR_CLOSED', then it was due
    // to a signal interruption, and so we should retry.
    for (;;) {
        const int rc = doReceive(buffer, shared);

        if (rc != FAIL_INTERRUPTED_OR_CLOSED)
            return rc;
    }
}

// Signal handler for 'SIGUSR1', installed in 'consumeHandler'.
extern "C" void noOpSignalHandler(int) {
    // TODO: Would it be sufficient to set the 'sigaction' to ignore rather
    //       than call this no-op function?
}

extern "C" void *consume(void *data);  // defined further below

int consumeHandler(std::string&, Shared& shared)
{
    // Don't do anything when a 'SIGUSR1' signal is received.  'SIGUSR1' is the
    // signal used to wake up the consumer thread (if we ever create a consumer
    // thread) from 'mq_receive', on systems where closing the queue is not
    // sufficient.  We can't ignore the signal (since we want it to interrupt
    // 'mq_receive'), but we also don't want it to do anything.  So, let the
    // handler be a no-op function ('noOpSignalHandler'). TODO: is that true?
    struct sigaction doNothing = {};
    doNothing.sa_handler = &noOpSignalHandler;
    sigaction(SIGUSR1,     // signal
              &doNothing,  // action
              0);          // don't need old action

    shared.consumerThreadExists = true;

    const int rc = pthread_create(&shared.consumerThread,
                                  0,         // pthread_attr_t (0 -> defaults)
                                  &consume,  // start routine
                                  &shared);  // argument to start routine
    if (rc) {
        const int error = errno;
        shared.consumerThreadExists = false;
        std::cerr << "Unable to create consumer thread: " << strerror(error)
                  << std::endl;
    }
 
    return rc;
}

int countHandler(std::string&, Shared& shared)
{
    mq_attr attributes;
    if (const int rc = mq_getattr(shared.queue, &attributes)) {
        const int error = errno;
        std::cerr << "Unable to get queue attributes to query message count: "
                  << strerror(error) << std::endl;
        return rc;
    }

    Lock lock(shared.stdoutMutex, shared.consumerThreadExists);
    std::cout << "count " << attributes.mq_curmsgs << std::endl;
 
    return 0;
}

int msgsizeHandler(std::string&, Shared& shared)
{
    mq_attr attributes;
    if (const int rc = mq_getattr(shared.queue, &attributes)) {
        const int error = errno;
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Unable to get queue attributes to report msgsize: "
                  << strerror(error) << std::endl;
        return rc;
    }

    Lock lock(shared.stdoutMutex, shared.consumerThreadExists);
    std::cout << "msgsize " << attributes.mq_msgsize << std::endl;
 
    return 0;
}

int maxmsgHandler(std::string&, Shared& shared)
{
    mq_attr attributes;
    if (const int rc = mq_getattr(shared.queue, &attributes)) {
        const int error = errno;
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Unable to get queue attributes to report maxmsg: "
                  << strerror(error) << std::endl;
        return rc;
    }

    Lock lock(shared.stdoutMutex, shared.consumerThreadExists);
    std::cout << "maxmsg " << attributes.mq_maxmsg << std::endl;
 
    return 0;
}

int closeHandler(std::string&, Shared& shared)
{
    Lock stoppedLock(shared.stoppedMutex, shared.consumerThreadExists);

    shared.stopped = true;

    const int rc = mq_close(shared.queue);

    if (rc) {
        const int error = errno;
        Lock lock(shared.stderrMutex, shared.consumerThreadExists);
        std::cerr << "Unable to close the message queue: "
                  << strerror(error) << std::endl;
    }

    if (shared.consumerThreadExists) {
        pthread_kill(shared.consumerThread, SIGUSR1);  // and ignore rcode
    }

    return rc;
}

// --------------
// thread drivers
// --------------

void *consume(void *data)
    // Receive messages from a POSIX message queue and print the messages to
    // standard output, each prefixed by its priority and length.  'data' must
    // be a pointer to a 'Shared' object.
{
    Shared&     shared = *static_cast<Shared*>(data);
    std::string buffer;

    for (;;) {
        const int rc = doReceive(buffer, shared);

        if (rc == FAIL_INTERRUPTED_OR_CLOSED) {
            Lock lock(shared.stoppedMutex);
            if (shared.stopped) {
                if (shared.options.debug) {
                    Lock lock(shared.stderrMutex);
                    std::cerr << "Consumer thread is finishing." << std::endl;
                }
                return 0;  // we're done
            }

            // otherwise, go around again
        }
        else if (rc) {
            return data;  // an error occurred (reported in 'doReceive').
                          // Return 'data' just because it's non-zero.
        }
    }
}

int serve(const mqd_t& mq, const Options& options)
{
    mq_attr attributes;
    if (const int rc = mq_getattr(mq, &attributes)) {
        const int error = errno;
        std::cerr << "Unable to get queue attributes initially: "
                  << strerror(error) << std::endl;
        return rc;
    }

    if (options.debug) {
        std::cerr << "Got the following attributes for message queue "
                  << repr(options.queueName) << ": "
                     " mq_maxmsg="  << attributes.mq_maxmsg
                  << " mq_msgsize=" << attributes.mq_msgsize
                  << " mq_curmsgs=" << attributes.mq_curmsgs << std::endl;
    }

    Shared shared(mq, attributes.mq_msgsize, options);

    class ThreadJoinGuard {
        const pthread_t& thread;
        const bool&      shouldJoin;

      public:
        ThreadJoinGuard(const pthread_t& thread, const bool& shouldJoin)
        : thread(thread)
        , shouldJoin(shouldJoin)
        {}

        ~ThreadJoinGuard() {
            if (shouldJoin)
                pthread_join(thread, 0);
        }
    };

    // If a "consumer" thread ends up getting created, make sure we join() it
    // before leaving this function. 
    ThreadJoinGuard threadJoinGuard(shared.consumerThread,
                                    shared.consumerThreadExists);

    // Buffer used for reading from standard input, and as a temporary place to
    // put messages received on demand.
    std::string chunk;
    int         commandResult = 0;

    while (std::cin >> chunk) {
        #define HANDLE_COMMAND(MSG_NAME)                                 \
            if (chunk == #MSG_NAME) {                                    \
                if (const int rc = MSG_NAME ## Handler(chunk, shared)) { \
                    commandResult = rc;                                  \
                    break;                                               \
                }                                                        \
            }

        HANDLE_COMMAND(send)
        else HANDLE_COMMAND(receive)
        else HANDLE_COMMAND(consume)
        else HANDLE_COMMAND(count)
        else HANDLE_COMMAND(msgsize)
        else HANDLE_COMMAND(maxmsg)
        else if (chunk == "close") {
            break;  // "close" is handled at the end.
        }
        else {
            Lock lock(shared.stderrMutex, shared.consumerThreadExists);
            std::cerr << "Unknown command \"" << chunk << '\"' << std::endl;
            commandResult = 1;
            break;
        }

        #undef HANDLE_COMMAND
    }

    const int closeResult = closeHandler(chunk, shared);
    return commandResult ? commandResult : closeResult;
}

// ----
// main
// ----

int main(int argc, char *argv[])
{
    if (const int rc = checkArgs(argc, argv))
        return rc;

    const Options options = parseOptions(argc, argv);

    if (options.unlink) {
        if (mq_unlink(options.queueName.c_str()) == -1) {
            std::cerr << "Unable to unlink queue " << repr(options.queueName)
                      << ": " << strerror(errno) << '\n';
            return errno;
        }
        return 0;
    }

    const mqd_t mq = openQueue(options);
    if (mq == mqd_t(-1)) {
        std::cerr << "Unable to open queue named " << repr(options.queueName)
                  << ": " << strerror(errno) << '\n';
        return errno;
    }

    return serve(mq, options);
}
