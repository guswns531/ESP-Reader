#pragma once

#include <string>
#include <vector>

#include "esp_err.h"

#include "model.h"

class DisplayManager {
public:
    esp_err_t init();
    esp_err_t showLibrary(const std::vector<DocumentEntry> &documents, std::size_t selected_index, bool partial);
    esp_err_t showReading(const DocumentEntry &document, std::size_t page_index, bool partial, bool force_full);
    esp_err_t showError(const std::string &message);

private:
    esp_err_t drawMultilineText(int x, int y, const std::vector<std::string> &lines, bool emphasized);
};
