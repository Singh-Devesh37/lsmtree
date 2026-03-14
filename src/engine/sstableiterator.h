#pragma once

#include <fstream>
#include <sstream>
#include <string>

class SSTableIterator {
public:
    explicit SSTableIterator(const std::string& filename);

    bool isValid() const;
    void next();

    std::string currKey;
    std::string currValue;

private:
    std::ifstream file;
    bool valid = false;
};
