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
    ESP_RETURN_ON_ERROR(session_.init(), kTag, "session init");
    ESP_RETURN_ON_ERROR(storage_.init(), kTag, "storage init");

    documents_ = storage_.loadDocuments();
    for (auto &document : documents_) {
        document.saved_page_index = session_.loadPageForDocument(document.path);
    }
    selected_document_ = 0;
    selected_page_ = 0;
    restoreSession();
    state_ = AppState::Library;
    return renderCurrentState();
}

void AppController::tick()
{
    const AppEvent event = input_.pollEvent();
    if (event.type == AppEventType::KeyPressed) {
        ESP_LOGI(kTag, "key=%d state=%d", static_cast<int>(event.key), static_cast<int>(state_));
        handleKey(event.key);
        if (needs_render_) {
            const esp_err_t err = renderCurrentState();
            if (err != ESP_OK) {
                state_ = AppState::Error;
                display_.showError("Render failed");
            }
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
        return display_.showReading(documents_.at(selected_document_), selected_page_, next_render_partial_, force_full_refresh_);
    case AppState::Error:
        return display_.showError("Unhandled app state");
    case AppState::Boot:
    default:
        return display_.showError("Boot state");
    }
}

void AppController::handleKey(InputKey key)
{
    needs_render_ = false;
    next_render_partial_ = false;
    force_full_refresh_ = false;

    if (state_ == AppState::Library) {
        if (documents_.empty()) {
            return;
        }
        switch (key) {
        case InputKey::Key4:
            if (selected_document_ > 0) {
                --selected_document_;
                needs_render_ = true;
                next_render_partial_ = true;
            }
            break;
        case InputKey::Key3:
            if (selected_document_ + 1 < documents_.size()) {
                ++selected_document_;
                needs_render_ = true;
                next_render_partial_ = true;
            }
            break;
        case InputKey::Key2:
            enterReading();
            needs_render_ = true;
            next_render_partial_ = true;
            break;
        case InputKey::Key1:
            needs_render_ = true;
            force_full_refresh_ = true;
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
            needs_render_ = true;
            force_full_refresh_ = true;
            break;
        case InputKey::Key5:
            enterLibrary();
            persistSession();
            needs_render_ = true;
            next_render_partial_ = true;
            break;
        case InputKey::Key4:
            if (selected_page_ > 0) {
                --selected_page_;
                persistSession();
                needs_render_ = true;
                next_render_partial_ = true;
            }
            break;
        case InputKey::Key3:
            if (selected_page_ + 1 < pages.size()) {
                ++selected_page_;
                persistSession();
                needs_render_ = true;
                next_render_partial_ = true;
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
    if (!documents_[selected_document_].pages.empty()) {
        selected_page_ = std::min<std::size_t>(
            documents_[selected_document_].saved_page_index,
            documents_[selected_document_].pages.size() - 1);
    } else {
        selected_page_ = 0;
    }
    persistSession();
}

void AppController::restoreSession()
{
    if (documents_.empty()) {
        return;
    }

    const ReadingSession session = session_.load();
    if (!session.valid) {
        return;
    }

        for (std::size_t i = 0; i < documents_.size(); ++i) {
            if (documents_[i].path == session.document_path) {
                selected_document_ = i;
                if (!documents_[i].pages.empty()) {
                    selected_page_ = std::min<std::size_t>(session.page_index, documents_[i].pages.size() - 1);
                    documents_[i].saved_page_index = selected_page_;
                }
                return;
            }
        }
}

void AppController::persistSession()
{
    if (documents_.empty()) {
        return;
    }

    ReadingSession session;
    session.document_path = documents_[selected_document_].path;
    session.page_index = static_cast<uint32_t>(selected_page_);
    session.valid = true;
    documents_[selected_document_].saved_page_index = selected_page_;
    session_.savePageForDocument(documents_[selected_document_].path, static_cast<uint32_t>(selected_page_));
    session_.save(session);
}
