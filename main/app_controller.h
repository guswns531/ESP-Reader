#pragma once

#include <cstddef>
#include <vector>

#include "app_state.h"
#include "display_manager.h"
#include "event.h"
#include "input_manager.h"
#include "model.h"
#include "storage_manager.h"

class AppController {
public:
    esp_err_t init();
    void tick();

private:
    void buildMockDocuments();
    esp_err_t renderCurrentState();
    void handleKey(InputKey key);
    void enterLibrary();
    void enterReading();

    AppState state_{AppState::Boot};
    bool next_render_partial_{false};
    DisplayManager display_;
    InputManager input_;
    StorageManager storage_;
    std::vector<DocumentEntry> documents_;
    std::size_t selected_document_{0};
    std::size_t selected_page_{0};
};
