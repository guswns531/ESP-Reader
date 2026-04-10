#pragma once

#include <string>

#include "esp_err.h"

struct ReadingSession {
    std::string document_path;
    uint32_t page_index{0};
    bool valid{false};
};

class SessionManager {
public:
    esp_err_t init();
    ReadingSession load() const;
    esp_err_t save(const ReadingSession &session) const;
    uint32_t loadPageForDocument(const std::string &document_path) const;
    esp_err_t savePageForDocument(const std::string &document_path, uint32_t page_index) const;
};
