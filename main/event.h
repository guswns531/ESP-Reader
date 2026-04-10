#pragma once

enum class InputKey {
    None,
    Key1,
    Key2,
    Key3,
    Key4,
    Key5,
};

enum class AppEventType {
    None,
    KeyPressed,
    IdleTimeout,
};

struct AppEvent {
    AppEventType type{AppEventType::None};
    InputKey key{InputKey::None};
};
