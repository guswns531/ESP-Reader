#include "document_parser.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kMaxCharsPerLine = 48;
constexpr std::size_t kMaxLinesPerPage = 14;

std::size_t utf8SequenceLength(unsigned char lead)
{
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

uint32_t decodeUtf8CodePoint(const std::string &text, std::size_t index, std::size_t *advance)
{
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    const std::size_t len = utf8SequenceLength(lead);
    *advance = std::min(len, text.size() - index);

    if (*advance == 1) {
        return lead;
    }

    uint32_t codepoint = 0;
    if (*advance == 2) {
        codepoint = static_cast<uint32_t>(lead & 0x1F) << 6;
        codepoint |= static_cast<uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F);
        return codepoint;
    }
    if (*advance == 3) {
        codepoint = static_cast<uint32_t>(lead & 0x0F) << 12;
        codepoint |= static_cast<uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6;
        codepoint |= static_cast<uint32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3F);
        return codepoint;
    }

    codepoint = static_cast<uint32_t>(lead & 0x07) << 18;
    codepoint |= static_cast<uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 12;
    codepoint |= static_cast<uint32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3F) << 6;
    codepoint |= static_cast<uint32_t>(static_cast<unsigned char>(text[index + 3]) & 0x3F);
    return codepoint;
}

int glyphColumns(uint32_t codepoint)
{
    if (codepoint < 0x80) {
        return 1;
    }
    if ((codepoint >= 0x1100 && codepoint <= 0x11FF) ||
        (codepoint >= 0x3130 && codepoint <= 0x318F) ||
        (codepoint >= 0xAC00 && codepoint <= 0xD7AF) ||
        (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
        (codepoint >= 0x3000 && codepoint <= 0x303F)) {
        return 2;
    }
    return 1;
}

bool parseIntStrict(const std::string &text, int *value)
{
    char *end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

int visualColumns(const std::string &text)
{
    if (text.starts_with("[[MATH_PBM|") && text.ends_with("]]")) {
        const std::size_t sep1 = text.find('|');
        const std::size_t sep2 = text.find('|', sep1 == std::string::npos ? sep1 : sep1 + 1);
        const std::size_t sep3 = text.find('|', sep2 == std::string::npos ? sep2 : sep2 + 1);
        if (sep1 != std::string::npos && sep2 != std::string::npos && sep3 != std::string::npos) {
            int asset_width = 0;
            if (parseIntStrict(text.substr(sep2 + 1, sep3 - sep2 - 1), &asset_width)) {
                return std::max(1, (asset_width + 7) / 8);
            }
        }
    }
    int width = 0;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t advance = 1;
        const uint32_t cp = decodeUtf8CodePoint(text, i, &advance);
        width += glyphColumns(cp);
        i += advance;
    }
    return width;
}

std::string takePrefixByColumns(std::string *text, int max_columns)
{
    std::string prefix;
    int used = 0;
    for (std::size_t i = 0; i < text->size();) {
        std::size_t advance = 1;
        const uint32_t cp = decodeUtf8CodePoint(*text, i, &advance);
        const int cols = glyphColumns(cp);
        if (used + cols > max_columns && !prefix.empty()) {
            break;
        }
        prefix.append(text->substr(i, advance));
        used += cols;
        i += advance;
        if (used >= max_columns) {
            break;
        }
    }
    text->erase(0, prefix.size());
    return prefix;
}

std::string trim(const std::string &input)
{
    std::size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }

    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return input.substr(start, end - start);
}

std::string basenameWithoutExtension(const std::string &path)
{
    const std::size_t slash = path.find_last_of('/');
    const std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
    const std::size_t dot = file.find_last_of('.');
    return dot == std::string::npos ? file : file.substr(0, dot);
}

bool isRuleLine(const std::string &text)
{
    if (text.size() < 3) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](char c) { return c == '-'; });
}

bool parseMathImageMarker(const std::string &text, RenderLine *line)
{
    constexpr const char *kPrefix = "[[MATH_PBM|";
    constexpr const char *kSuffix = "]]";
    if (!text.starts_with(kPrefix) || !text.ends_with(kSuffix)) {
        return false;
    }

    const std::string payload = text.substr(std::strlen(kPrefix), text.size() - std::strlen(kPrefix) - std::strlen(kSuffix));
    const std::size_t sep1 = payload.find('|');
    const std::size_t sep2 = payload.find('|', sep1 == std::string::npos ? sep1 : sep1 + 1);
    if (sep1 == std::string::npos || sep2 == std::string::npos) {
        return false;
    }
    const std::string path = payload.substr(0, sep1);
    int width = 0;
    int height = 0;
    if (!parseIntStrict(payload.substr(sep1 + 1, sep2 - sep1 - 1), &width) ||
        !parseIntStrict(payload.substr(sep2 + 1), &height)) {
        return false;
    }

    line->kind = RenderLineKind::Math;
    line->asset_path = path;
    line->asset_width = width;
    line->asset_height = height;
    line->text.clear();
    return true;
}

std::string normalizeInlineMarkdown(const std::string &text)
{
    std::string out;
    out.reserve(text.size());

    for (std::size_t i = 0; i < text.size();) {
        if (i + 1 < text.size() && text[i] == '*' && text[i + 1] == '*') {
            i += 2;
            continue;
        }
        if (text[i] == '*') {
            ++i;
            continue;
        }
        if (text[i] == '`') {
            out += '[';
            ++i;
            while (i < text.size() && text[i] != '`') {
                out += text[i++];
            }
            out += ']';
            if (i < text.size() && text[i] == '`') {
                ++i;
            }
            continue;
        }
        out += text[i++];
    }

    return out;
}

bool isBoldLine(const std::string &text)
{
    return text.find("**") != std::string::npos;
}

void pushWrapped(std::vector<RenderLine> &lines, const std::string &text, RenderLineKind kind)
{
    if (text.empty()) {
        lines.push_back(RenderLine{.text = "", .kind = RenderLineKind::Spacer, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0});
        return;
    }

    const bool bold = isBoldLine(text);
    const std::string normalized = normalizeInlineMarkdown(text);
    std::istringstream words(normalized);
    std::string word;
    std::string current;
    int current_cols = 0;  // tracks visualColumns(current) without recomputing
    while (words >> word) {
        while (visualColumns(word) > static_cast<int>(kMaxCharsPerLine)) {
            if (!current.empty()) {
                lines.push_back(RenderLine{.text = current, .kind = kind, .bold = bold, .asset_path = "", .asset_width = 0, .asset_height = 0});
                current.clear();
                current_cols = 0;
            }
            std::string segment = takePrefixByColumns(&word, static_cast<int>(kMaxCharsPerLine));
            if (!segment.empty()) {
                lines.push_back(RenderLine{.text = segment, .kind = kind, .bold = bold, .asset_path = "", .asset_width = 0, .asset_height = 0});
            } else {
                break;
            }
        }

        if (word.empty()) {
            continue;
        }

        const int word_cols = visualColumns(word);
        // +1 for the space separator when appending to a non-empty line
        const int needed = current.empty() ? word_cols : (current_cols + 1 + word_cols);
        if (needed > static_cast<int>(kMaxCharsPerLine) && !current.empty()) {
            lines.push_back(RenderLine{.text = current, .kind = kind, .bold = bold, .asset_path = "", .asset_width = 0, .asset_height = 0});
            current = word;
            current_cols = word_cols;
        } else {
            if (!current.empty()) {
                current += ' ';
            }
            current += word;
            current_cols = needed;
        }
    }
    if (!current.empty()) {
        lines.push_back(RenderLine{.text = current, .kind = kind, .bold = bold, .asset_path = "", .asset_width = 0, .asset_height = 0});
    }
}

void pushPage(std::vector<DocumentPage> &pages, const std::string &title, std::vector<RenderLine> &buffer)
{
    if (buffer.empty()) {
        return;
    }

    pages.push_back(DocumentPage{
        .title = title,
        .lines = buffer,
    });
    buffer.clear();
}

void appendPageLine(std::vector<DocumentPage> &pages, const std::string &title, std::vector<RenderLine> &buffer, const RenderLine &line)
{
    if (buffer.size() >= kMaxLinesPerPage) {
        pushPage(pages, title, buffer);
    }
    buffer.push_back(line);
}

struct ParsedDocument {
    std::vector<RenderLine> lines;
    std::vector<DocumentTocEntry> toc;
};

void appendHeading(std::vector<RenderLine> &output, std::vector<DocumentTocEntry> &toc,
                   const std::string &text, int level, const std::string &prefix)
{
    const std::size_t page_index = output.size() / kMaxLinesPerPage;
    const std::size_t start = output.size();
    pushWrapped(output, prefix + text, RenderLineKind::Normal);
    if (!output.empty() && output.size() > start) {
        output.back().bold = true;
    }
    toc.push_back(DocumentTocEntry{
        .title = text,
        .page_index = page_index,
        .level = level,
    });
    output.push_back(RenderLine{.text = "", .kind = RenderLineKind::Spacer, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0});
}

ParsedDocument parseSourceToLines(const std::string &source)
{
    ParsedDocument parsed;
    std::istringstream stream(source);
    std::string line;
    bool in_code_block = false;
    bool in_math_block = false;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.rfind("```", 0) == 0) {
            in_code_block = !in_code_block;
            parsed.lines.push_back(RenderLine{.text = in_code_block ? "[code]" : "", .kind = in_code_block ? RenderLineKind::Code : RenderLineKind::Spacer, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0});
            continue;
        }
        if (trimmed == "$$") {
            in_math_block = !in_math_block;
            continue;
        }

        if (trimmed.empty()) {
            parsed.lines.push_back(RenderLine{.text = "", .kind = RenderLineKind::Spacer, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0});
            continue;
        }

        RenderLine math_image;
        if (parseMathImageMarker(trimmed, &math_image)) {
            parsed.lines.push_back(math_image);
            continue;
        }

        if (in_code_block) {
            while (visualColumns(trimmed) > static_cast<int>(kMaxCharsPerLine)) {
                parsed.lines.push_back(RenderLine{
                    .text = takePrefixByColumns(&trimmed, static_cast<int>(kMaxCharsPerLine)),
                    .kind = RenderLineKind::Code,
                    .bold = false,
                    .asset_path = "",
                    .asset_width = 0,
                    .asset_height = 0,
                });
            }
            parsed.lines.push_back(RenderLine{.text = trimmed, .kind = RenderLineKind::Code, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0});
            continue;
        }

        if (in_math_block) {
            parsed.lines.push_back(RenderLine{.text = trimmed, .kind = RenderLineKind::Math, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0});
            continue;
        }

        if (trimmed.size() >= 2 && trimmed.front() == '$' && trimmed.back() == '$' && trimmed != "$$") {
            pushWrapped(parsed.lines, trimmed, RenderLineKind::Math);
            continue;
        }

        if (trimmed.rfind("#### ", 0) == 0) {
            appendHeading(parsed.lines, parsed.toc, trimmed.substr(5), 4, "#### ");
            continue;
        }
        if (trimmed.rfind("### ", 0) == 0) {
            appendHeading(parsed.lines, parsed.toc, trimmed.substr(4), 3, "### ");
            continue;
        }
        if (trimmed.rfind("## ", 0) == 0) {
            appendHeading(parsed.lines, parsed.toc, trimmed.substr(3), 2, "## ");
            continue;
        }
        if (trimmed.rfind("# ", 0) == 0) {
            appendHeading(parsed.lines, parsed.toc, trimmed.substr(2), 1, "# ");
            continue;
        }
        if (trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0) {
            pushWrapped(parsed.lines, trimmed.substr(2), RenderLineKind::List);
            continue;
        }
        if (trimmed.rfind("> ", 0) == 0) {
            pushWrapped(parsed.lines, trimmed.substr(2), RenderLineKind::Quote);
            continue;
        }
        if (isRuleLine(trimmed)) {
            parsed.lines.push_back(RenderLine{.text = "", .kind = RenderLineKind::Rule, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0});
            continue;
        }
        if (trimmed.size() > 3 && std::isdigit(static_cast<unsigned char>(trimmed[0])) != 0) {
            const std::size_t dot = trimmed.find(". ");
            if (dot != std::string::npos) {
                pushWrapped(parsed.lines, trimmed.substr(dot + 2), RenderLineKind::List);
                continue;
            }
            continue;
        }

        pushWrapped(parsed.lines, trimmed, RenderLineKind::Normal);
    }

    return parsed;
}

}  // namespace

DocumentEntry DocumentParser::parse(const std::string &path, const std::string &source) const
{
    DocumentEntry document;
    document.path = path;
    document.title = basenameWithoutExtension(path);

    const ParsedDocument parsed = parseSourceToLines(source);
    document.toc = parsed.toc;
    std::vector<RenderLine> page_buffer;
    for (const auto &line : parsed.lines) {
        appendPageLine(document.pages, document.title, page_buffer, line);
    }
    pushPage(document.pages, document.title, page_buffer);

    if (document.pages.empty()) {
        document.pages.push_back(DocumentPage{
            .title = document.title,
            .lines = {RenderLine{.text = "(empty document)", .kind = RenderLineKind::Normal, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0}},
        });
    }

    return document;
}
