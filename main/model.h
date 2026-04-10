#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class RenderLineKind {
    Normal,
    List,
    Code,
    Math,
    Quote,
    Rule,
    Spacer,
};

struct RenderLine {
    std::string text;
    RenderLineKind kind{RenderLineKind::Normal};
    bool bold{false};
};

struct DocumentPage {
    std::string title;
    std::vector<RenderLine> lines;
};

struct DocumentEntry {
    std::string title;
    std::string path;
    std::vector<DocumentPage> pages;
    std::size_t saved_page_index{0};
};
