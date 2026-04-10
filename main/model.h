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
    std::string asset_path;
    int asset_width{0};
    int asset_height{0};
};

struct DocumentPage {
    std::string title;
    std::vector<RenderLine> lines;
};

struct DocumentTocEntry {
    std::string title;
    std::size_t page_index{0};
    int level{1};
};

struct DocumentEntry {
    std::string title;
    std::string path;
    std::vector<DocumentPage> pages;
    std::vector<DocumentTocEntry> toc;
    std::size_t saved_page_index{0};
};
