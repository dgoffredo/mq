
#include <iostream>
#include <fstream>
#include <string>

#include "repr.h"

int main(int argc, char *argv[])
    // $ splice-readme source.cpp README.md > spliced-source.cpp
{
    const char *const source = argv[1];
    const char *const readme = argv[2];

    std::ifstream sourceIn(source);
    std::string line;
    while (std::getline(sourceIn, line)) {
        if (line != "\"<make, insert README here>\"") {
            std::cout << line << '\n';
            continue;
        }

        // Found the line to replace with the README file.  So now read README.
        std::ifstream readmeIn(readme);
        while (std::getline(readmeIn, line)) {
            line.push_back('\n');
            std::cout << repr(line) << '\n';
        }
        std::cout << repr("\n") << '\n';
    }
}
