#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "esp_err.h"

namespace epaper {

constexpr int kWidth = 400;
constexpr int kHeight = 300;
constexpr int kFontScale = 2;

esp_err_t init();
void clear(bool black);
void drawTextLine(int x, int baseline_y, const std::string &text, bool black);
void drawFilledRect(int x, int y, int w, int h, bool black);
void drawHLine(int x, int y, int w, bool black);
int canvasWidth();
int canvasHeight();
int lineHeight();
esp_err_t updateFull();
esp_err_t updatePartial(int x, int y, int w, int h);

}  // namespace epaper
