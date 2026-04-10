#include "app_controller.h"

#include "esp_check.h"
#include "esp_log.h"

namespace {

constexpr const char *kTag = "app_controller";

}  // namespace

esp_err_t AppController::init()
{
    ESP_RETURN_ON_ERROR(display_.init(), kTag, "display init");
    ESP_RETURN_ON_ERROR(input_.init(), kTag, "input init");
    ESP_RETURN_ON_ERROR(storage_.init(), kTag, "storage init");

    documents_ = storage_.loadDocuments();
    selected_document_ = 0;
    selected_page_ = 0;
    enterLibrary();
    return renderCurrentState();
}

void AppController::tick()
{
    const AppEvent event = input_.pollEvent();
    if (event.type == AppEventType::KeyPressed) {
        ESP_LOGI(kTag, "key=%d state=%d", static_cast<int>(event.key), static_cast<int>(state_));
        handleKey(event.key);
        const esp_err_t err = renderCurrentState();
        if (err != ESP_OK) {
            state_ = AppState::Error;
            display_.showError("Render failed");
        }
    }
}

esp_err_t AppController::renderCurrentState()
{
    switch (state_) {
    case AppState::Library:
        return display_.showLibrary(documents_, selected_document_, next_render_partial_);
    case AppState::Reading:
        if (documents_.empty()) {
            return display_.showLibrary(documents_, selected_document_, false);
        }
        return display_.showReading(documents_.at(selected_document_), selected_page_);
    case AppState::Error:
        return display_.showError("Unhandled app state");
    case AppState::Boot:
    default:
        return display_.showError("Boot state");
    }
}

void AppController::handleKey(InputKey key)
{
    next_render_partial_ = false;

    if (state_ == AppState::Library) {
        if (documents_.empty()) {
            return;
        }
        switch (key) {
        case InputKey::Key4:
            if (selected_document_ > 0) {
                --selected_document_;
                next_render_partial_ = true;
            }
            break;
        case InputKey::Key3:
            if (selected_document_ + 1 < documents_.size()) {
                ++selected_document_;
                next_render_partial_ = true;
            }
            break;
        case InputKey::Key1:
        case InputKey::Key2:
            enterReading();
            break;
        default:
            break;
        }
        return;
    }

    if (state_ == AppState::Reading) {
        const auto &pages = documents_.at(selected_document_).pages;
        switch (key) {
        case InputKey::Key1:
            enterLibrary();
            break;
        case InputKey::Key5:
            if (selected_page_ > 0) {
                --selected_page_;
            }
            break;
        case InputKey::Key2:
            if (selected_page_ + 1 < pages.size()) {
                ++selected_page_;
            }
            break;
        case InputKey::Key4:
            if (selected_page_ >= 5) {
                selected_page_ -= 5;
            } else {
                selected_page_ = 0;
            }
            break;
        case InputKey::Key3:
            if (selected_page_ + 5 < pages.size()) {
                selected_page_ += 5;
            } else if (!pages.empty()) {
                selected_page_ = pages.size() - 1;
            }
            break;
        default:
            break;
        }
    }
}

void AppController::enterLibrary()
{
    state_ = AppState::Library;
}

void AppController::enterReading()
{
    if (documents_.empty()) {
        state_ = AppState::Library;
        selected_page_ = 0;
        return;
    }
    state_ = AppState::Reading;
    selected_page_ = 0;
}
