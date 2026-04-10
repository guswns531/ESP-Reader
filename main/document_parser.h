#pragma once

#include <string>

#include "model.h"

class DocumentParser {
public:
    DocumentEntry parse(const std::string &path, const std::string &source) const;
};
