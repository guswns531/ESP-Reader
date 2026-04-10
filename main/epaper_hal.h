#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "esp_err.h"

namespace epaper {

constexpr int kWidth = 400;
constexpr int kHeight = 300;

esp_err_t init();
void clear(bool black);
void setUiFont();
void drawTextLine(int x, int baseline_y, const std::string &text, bool black);
void drawTextLineBold(int x, int baseline_y, const std::string &text, bool black);
void drawTitleText(int x, int baseline_y, const std::string &text, bool black);
int titleTextWidth(const std::string &text);
int titleLineHeight();
void drawTriangleUp(int x, int y, int size, bool black);
void drawTriangleDown(int x, int y, int size, bool black);
void drawTriangleLeft(int x, int y, int size, bool black);
void drawTriangleRight(int x, int y, int size, bool black);
void drawFilledRect(int x, int y, int w, int h, bool black);
void drawRect(int x, int y, int w, int h, bool black, int thickness);
void drawHLine(int x, int y, int w, bool black);
int canvasWidth();
int canvasHeight();
int lineHeight();
int textWidth(const std::string &text);
void drawUtf8Line(int x, int baseline_y, const char *text);
esp_err_t updateFull();
esp_err_t updatePartial(int x, int y, int w, int h);

}  // namespace epaper
