#include "display_manager.h"

#include <algorithm>

#include "epaper_hal.h"

namespace {

constexpr int kMarginX = 8;
constexpr int kHeaderBaseline = 26;
constexpr int kDividerY = 40;
constexpr int kBodyStartY = 64;
constexpr int kSelectionBoxPadX = 6;
constexpr int kSelectionBoxPadY = 3;
constexpr int kListRegionY = 48;
constexpr int kListRegionH = 220;
constexpr int kReadingRegionY = 0;
constexpr int kReadingRegionH = 300;

void drawTopRightHint(const std::string &line1, const std::string &line2) {
  const int right_margin = 12;
  epaper::setUiFont();
  const int x1 =
      epaper::canvasWidth() - right_margin - epaper::textWidth(line1);
  const int x2 =
      epaper::canvasWidth() - right_margin - epaper::textWidth(line2);
  epaper::drawTextLine(x1, 16, line1, true);
  epaper::drawTextLine(x2, 30, line2, true);
}

void drawTopRightHintLibrary() {
  epaper::drawTriangleUp(304, 11, 4, true);
  epaper::drawTriangleDown(314, 11, 4, true);
  epaper::drawTextLine(324, 18, "SELECT", true);

  epaper::drawTriangleLeft(305, 22, 4, true);
  epaper::drawTextLine(314, 30, "BACK", true);
  epaper::drawTriangleRight(352, 22, 4, true);
  epaper::drawTextLine(362, 30, "OPEN", true);
}

void drawTopRightHintReading() {
  epaper::drawTriangleUp(304, 11, 4, true);
  epaper::drawTriangleDown(314, 11, 4, true);
  epaper::drawTextLine(324, 18, "PAGE", true);

  epaper::drawTriangleLeft(305, 22, 4, true);
  epaper::drawTextLine(314, 30, "LIB", true);
}

} // namespace

esp_err_t DisplayManager::init() { return epaper::init(); }

esp_err_t
DisplayManager::showLibrary(const std::vector<DocumentEntry> &documents,
                            std::size_t selected_index, bool partial) {
  epaper::clear(false);
  epaper::setUiFont();
  epaper::drawTitleText(kMarginX, kHeaderBaseline, "LIBRARY", true);
  drawTopRightHintLibrary();
  epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2),
                    true);

  if (documents.empty()) {
    ESP_ERROR_CHECK(drawMultilineText(
        kMarginX, kBodyStartY, {"NO DOCUMENTS", "PUT .MD IN SPIFFS"}, false));
    return epaper::updateFull();
  } else {
    int baseline = kBodyStartY;
    for (std::size_t i = 0; i < documents.size(); ++i) {
      const bool selected = i == selected_index;
      const std::string status =
          " " + std::to_string(documents[i].saved_page_index + 1) + "/" +
          std::to_string(std::max<std::size_t>(documents[i].pages.size(), 1));
      const std::string label = documents[i].title + status;
      const int label_width = epaper::textWidth(label);
      if (selected) {
        epaper::drawRect(kMarginX - kSelectionBoxPadX + 1,
                         baseline - epaper::lineHeight() - 3,
                         label_width + (kSelectionBoxPadX * 2),
                         epaper::lineHeight() + (kSelectionBoxPadY * 2) + 1,
                         true, 2);
      }
      epaper::drawTextLine(kMarginX, baseline, label, true);
      baseline += epaper::lineHeight() + 8;
    }
  }
  if (partial) {
    return epaper::updatePartial(0, kListRegionY, epaper::canvasWidth(),
                                 kListRegionH);
  }
  return epaper::updateFull();
}

esp_err_t DisplayManager::showReading(const DocumentEntry &document,
                                      std::size_t page_index, bool partial,
                                      bool force_full) {
  epaper::clear(false);
  epaper::setUiFont();
  epaper::drawTitleText(kMarginX, kHeaderBaseline, document.title, true);
  drawTopRightHintReading();
  epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2),
                    true);

  const std::size_t safe_index =
      std::min(page_index, document.pages.size() - 1);
  ESP_ERROR_CHECK(drawMultilineText(kMarginX, kBodyStartY,
                                    document.pages[safe_index].lines, false));

  if (force_full || !partial) {
    return epaper::updateFull();
  }
  return epaper::updatePartial(0, kReadingRegionY, epaper::canvasWidth(),
                               kReadingRegionH);
}

esp_err_t DisplayManager::showError(const std::string &message) {
  epaper::clear(false);
  epaper::setUiFont();
  epaper::drawTitleText(kMarginX, kHeaderBaseline, "ERROR", true);
  epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2),
                    true);
  return drawMultilineText(kMarginX, kBodyStartY, {message}, false) == ESP_OK
             ? epaper::updateFull()
             : ESP_FAIL;
}

esp_err_t DisplayManager::drawMultilineText(
    int x, int y, const std::vector<std::string> &lines, bool emphasized) {
  (void)emphasized;
  int baseline = y;
  for (const auto &line : lines) {
    epaper::drawTextLine(x, baseline, line, true);
    baseline += epaper::lineHeight() + 6;
  }
  return ESP_OK;
}
