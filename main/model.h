#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct DocumentPage {
    std::string title;
    std::vector<std::string> lines;
};

struct DocumentEntry {
    std::string title;
    std::string path;
    std::vector<DocumentPage> pages;
};
