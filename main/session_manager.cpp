#include "session_manager.h"

#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace {

constexpr const char *kNamespace = "reader";
constexpr const char *kKeyPath = "last_path";
constexpr const char *kKeyPage = "last_page";

}  // namespace

esp_err_t SessionManager::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

ReadingSession SessionManager::load() const
{
    ReadingSession session;
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return session;
    }

    size_t length = 0;
    if (nvs_get_str(handle, kKeyPath, nullptr, &length) == ESP_OK && length > 1) {
        session.document_path.resize(length - 1);
        if (nvs_get_str(handle, kKeyPath, session.document_path.data(), &length) == ESP_OK) {
            session.document_path.resize(length - 1);
            session.valid = true;
        }
    }

    uint32_t page_index = 0;
    if (nvs_get_u32(handle, kKeyPage, &page_index) == ESP_OK) {
        session.page_index = page_index;
    }

    nvs_close(handle);
    return session;
}

esp_err_t SessionManager::save(const ReadingSession &session) const
{
    nvs_handle_t handle = 0;
    ESP_RETURN_ON_ERROR(nvs_open(kNamespace, NVS_READWRITE, &handle), "session", "open nvs");
    ESP_RETURN_ON_ERROR(nvs_set_str(handle, kKeyPath, session.document_path.c_str()), "session", "set path");
    ESP_RETURN_ON_ERROR(nvs_set_u32(handle, kKeyPage, session.page_index), "session", "set page");
    ESP_RETURN_ON_ERROR(nvs_commit(handle), "session", "commit");
    nvs_close(handle);
    return ESP_OK;
}
