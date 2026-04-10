#pragma once

#include <vector>

#include "esp_err.h"

#include "document_parser.h"
#include "model.h"

class StorageManager {
public:
    esp_err_t init();
    std::vector<DocumentEntry> loadDocuments() const;

private:
    DocumentParser parser_;
};
