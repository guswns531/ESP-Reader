#include "storage_manager.h"

#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sstream>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"

namespace {

constexpr const char *kTag = "storage_manager";
constexpr const char *kMountPoint = "/spiffs";

bool hasMarkdownExtension(const char *name)
{
    const std::string filename(name);
    return filename.size() >= 3 && filename.ends_with(".md");
}

std::string readFile(const std::string &path)
{
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}  // namespace

esp_err_t StorageManager::init()
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = kMountPoint;
    conf.partition_label = "storage";
    conf.max_files = 8;
    conf.format_if_mount_failed = true;

    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&conf), kTag, "mount spiffs");

    size_t total = 0;
    size_t used = 0;
    ESP_RETURN_ON_ERROR(esp_spiffs_info("storage", &total, &used), kTag, "spiffs info");
    ESP_LOGI(kTag, "SPIFFS mounted total=%u used=%u", static_cast<unsigned>(total), static_cast<unsigned>(used));
    return ESP_OK;
}

std::vector<DocumentEntry> StorageManager::loadDocuments() const
{
    std::vector<DocumentEntry> documents;

    DIR *dir = opendir(kMountPoint);
    if (dir == nullptr) {
        ESP_LOGE(kTag, "failed to open %s", kMountPoint);
        return documents;
    }

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (!hasMarkdownExtension(entry->d_name)) {
            continue;
        }

        const std::string path = std::string(kMountPoint) + "/" + entry->d_name;
        const std::string content = readFile(path);
        documents.push_back(parser_.parse(path, content));
    }
    closedir(dir);

    std::sort(documents.begin(), documents.end(), [](const DocumentEntry &lhs, const DocumentEntry &rhs) {
        return lhs.title < rhs.title;
    });

    ESP_LOGI(kTag, "loaded %u markdown documents", static_cast<unsigned>(documents.size()));
    return documents;
}
