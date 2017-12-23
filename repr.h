#ifndef INCLUDED_REPR
#define INCLUDED_REPR

#include <string>

std::string repr(const std::string& input);
    // Return a representation of the specified 'input' as it might appear in
    // C++ source code, in double quotes with the appropriate characters
    // escaped.

#endif

