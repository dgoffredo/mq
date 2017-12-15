
#include <errno.h>     // error codes
#include <fcntl.h>     // file open constants
#include <limits.h>    // _POSIX_*_MAX
#include <mqueue.h>    // mq_*
#include <string.h>    // strerror
#include <sys/stat.h>  // file mode constants 

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

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
"--read      open the queue receiving messages\n"
"--write     open the queue for sending messages\n"
"--open      open the queue (only if existing, if without --create)\n"
"--create    create the queue (exclusively, if without --open)\n"
"--permissions <octal>    Unix file permissions to use if creating queue \n"
"--maxmsg    maximum number of messages to allow in the queue, if possible\n"
"--msgsize   maximum size of any message in the queue, if possible\n"
"--debug     print to stderr trace useful when debugging\n"
"\n"
"At least one of --create and/or --open must be specified, and at least one\n"
"of --read and/or --write must be specified.  If either of --maxmsg or\n"
"--msgsize is specified, then the other must be specified as well.\n"
"\n"
"Message Queue:\n"
"The name of the POSIX local message queue to open (and possibly create).\n"
"Note that on many systems, message queue names are required to begin with a\n"
"forward slash.  Also note that message queues are not necessarily visible\n"
"on the file system.  On Linux they are, but they are not, for example, on\n"
"AIX and Solaris.\n";
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
        return 0;
    }

    if (!find("--read") && !find("--write")) {
        std::cerr << "One or both of --read and --write must be specified.\n";
        return 2;
    }

    if (!find("--open") && !find("--create")) {
       std::cerr << "One or both of --open and --create must be specified.\n";
       return 3;
    }

    if (bool(find("--msgsize")) != bool(find("--maxmsg"))) {
        std::cerr << "Specify neither of both of --msgsize and --maxmsg.\n";
        return 4;
    }

    if (argv[argc - 1][0] == '-') {
        std::cerr << "Final argument must be a non-option (the queue name).\n";
        return 5;
    }

    return 0;
}

struct Options {
    enum { READ_ONLY, WRITE_ONLY,  READ_WRITE } operation;
    enum { OPEN_ONLY, CREATE_ONLY, OPEN }       open;
    int                                         filePermissions;
    bool                                        maxesSpecified;
    ssize_t                                     maxmsg;
    ssize_t                                     msgsize;                  
    bool                                        debug;
    std::string                                 queueName;
};

Options parseOptions(int argc, const char *const argv[])
{
    const FindArgs find(argv + 1, argv + argc);

    Options options;

    const bool read  = find("--read"),
               write = find("--write");
    options.operation = read && write ? Options::READ_WRITE
                                      : read ? Options::READ_ONLY
                                             : Options::WRITE_ONLY;

    const bool open   = find("--open"),
               create = find("--create");
    options.open = open && create ? Options::OPEN
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

    options.debug = find("--debug");   
 
    options.queueName = argv[argc - 1];

    return options;
}

int main(int argc, char *argv[])
{
    if (const int rc = checkArgs(argc, argv))
        return rc;

    const Options options = parseOptions(argc, argv);

    // TODO The thing!
}
