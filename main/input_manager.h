#pragma once

#include "event.h"
#include "esp_err.h"

class InputManager {
public:
    esp_err_t init();
    AppEvent pollEvent();

private:
    InputKey last_key_{InputKey::None};
};
