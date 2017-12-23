
#include "repr.h"

// Standard C
#include <stdio.h>  // snprintf

// Standard C++
#include <cassert>
#include <cctype>   // std::isgraph

std::string repr(const std::string& input)
    // Return a representation of the specified 'input' as it might appear in
    // C++ source code, in double quotes with the appropriate characters
    // escaped.
{
    std::string result;
    result.push_back('\"');
    for (std::string::const_iterator it = input.begin();
         it != input.end();
         ++it)
    {
        const char ch = *it;

        // Cast to unsigned char to avoid undefined behavior in 'isgraph',
        // according to <cppreference.com>.
        if (std::isgraph(static_cast<unsigned char>(ch)))
        {
            if (ch == '\"')
                result.push_back('\\');

            result.push_back(ch);
            continue;
        }
        
        switch (ch) {
          case ' ': result.push_back(ch);   break;
          case '\a': result.append("\\a");  break;
          case '\b': result.append("\\b");  break;
          case '\f': result.append("\\f");  break;
          case '\n': result.append("\\n");  break;
          case '\r': result.append("\\r");  break;
          case '\t': result.append("\\t");  break;
          case '\v': result.append("\\v");  break;
          default: {
              // Anything else is escaped as hexadecimal.
              result.append("\\x");
              char buffer[3];  // two hex digits and the null terminator
              const int rc =
                  snprintf(buffer, sizeof buffer, "%.2x", (unsigned int)ch);
              assert(rc == 2);
              result.append(buffer);
          }
        }
    } 

    result.push_back('\"');
    return result;
}

