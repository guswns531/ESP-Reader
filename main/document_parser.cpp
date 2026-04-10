#include "document_parser.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kMaxCharsPerLine = 34;
constexpr std::size_t kMaxLinesPerPage = 14;

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

void pushWrapped(std::vector<std::string> &lines, const std::string &text)
{
    if (text.empty()) {
        lines.emplace_back("");
        return;
    }

    std::istringstream words(text);
    std::string word;
    std::string current;
    while (words >> word) {
        const std::size_t candidate_size = current.empty() ? word.size() : current.size() + 1 + word.size();
        if (candidate_size > kMaxCharsPerLine && !current.empty()) {
            lines.push_back(current);
            current = word;
        } else {
            if (!current.empty()) {
                current += ' ';
            }
            current += word;
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
}

void pushPage(std::vector<DocumentPage> &pages, const std::string &title, std::vector<std::string> &buffer)
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

void appendPageLine(std::vector<DocumentPage> &pages, const std::string &title, std::vector<std::string> &buffer, const std::string &line)
{
    if (buffer.size() >= kMaxLinesPerPage) {
        pushPage(pages, title, buffer);
    }
    buffer.push_back(line);
}

std::vector<std::string> parseSourceToLines(const std::string &source)
{
    std::vector<std::string> output;
    std::istringstream stream(source);
    std::string line;
    bool in_code_block = false;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.rfind("```", 0) == 0) {
            in_code_block = !in_code_block;
            output.emplace_back(in_code_block ? "[code]" : "");
            continue;
        }

        if (trimmed.empty()) {
            output.emplace_back("");
            continue;
        }

        if (in_code_block) {
            while (trimmed.size() > kMaxCharsPerLine) {
                output.push_back(trimmed.substr(0, kMaxCharsPerLine));
                trimmed.erase(0, kMaxCharsPerLine);
            }
            output.push_back(trimmed);
            continue;
        }

        if (trimmed.rfind("### ", 0) == 0) {
            pushWrapped(output, std::string("### ") + trimmed.substr(4));
            output.emplace_back("");
            continue;
        }
        if (trimmed.rfind("## ", 0) == 0) {
            pushWrapped(output, std::string("## ") + trimmed.substr(3));
            output.emplace_back("");
            continue;
        }
        if (trimmed.rfind("# ", 0) == 0) {
            pushWrapped(output, trimmed.substr(2));
            output.emplace_back("");
            continue;
        }
        if (trimmed.rfind("- ", 0) == 0 || trimmed.rfind("* ", 0) == 0) {
            pushWrapped(output, std::string("* ") + trimmed.substr(2));
            continue;
        }

        pushWrapped(output, trimmed);
    }

    return output;
}

}  // namespace

DocumentEntry DocumentParser::parse(const std::string &path, const std::string &source) const
{
    DocumentEntry document;
    document.path = path;
    document.title = basenameWithoutExtension(path);

    std::vector<std::string> lines = parseSourceToLines(source);
    std::vector<std::string> page_buffer;
    for (const auto &line : lines) {
        appendPageLine(document.pages, document.title, page_buffer, line);
    }
    pushPage(document.pages, document.title, page_buffer);

    if (document.pages.empty()) {
        document.pages.push_back(DocumentPage{
            .title = document.title,
            .lines = {"(empty document)"},
        });
    }

    return document;
}
