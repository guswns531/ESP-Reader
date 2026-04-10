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
constexpr int kFooterBaseline = 294;


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
        kMarginX, kBodyStartY,
        {RenderLine{.text = "NO DOCUMENTS", .kind = RenderLineKind::Normal},
         RenderLine{.text = "PUT .MD IN SPIFFS", .kind = RenderLineKind::Normal}},
        false));
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
    return epaper::updatePartial(0, 0, epaper::canvasWidth(),
                                 epaper::canvasHeight());
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

  const std::string page_status =
      std::to_string(safe_index + 1) + " / " + std::to_string(document.pages.size());
  const int page_status_x = epaper::canvasWidth() - 8 - epaper::textWidth(page_status);
  epaper::drawTextLine(page_status_x, kFooterBaseline, page_status, true);

  if (force_full || !partial) {
    return epaper::updateFull();
  }
  return epaper::updatePartial(0, 0, epaper::canvasWidth(),
                               epaper::canvasHeight());
}

esp_err_t DisplayManager::showError(const std::string &message) {
  epaper::clear(false);
  epaper::setUiFont();
  epaper::drawTitleText(kMarginX, kHeaderBaseline, "ERROR", true);
  epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2),
                    true);
  return drawMultilineText(kMarginX, kBodyStartY,
                           {RenderLine{.text = message, .kind = RenderLineKind::Normal}}, false) == ESP_OK
             ? epaper::updateFull()
             : ESP_FAIL;
}

esp_err_t DisplayManager::drawMultilineText(
    int x, int y, const std::vector<RenderLine> &lines, bool emphasized) {
  (void)emphasized;
  const int lh = epaper::lineHeight();  // cached once — font never changes mid-render
  int baseline = y;
  for (const auto &line : lines) {
    switch (line.kind) {
    case RenderLineKind::Spacer:
      baseline += 6;
      break;
    case RenderLineKind::List:
      epaper::drawFilledRect(x, baseline - 5, 4, 4, true);
      if (line.bold) {
        epaper::drawTextLineBold(x + 12, baseline, line.text, true);
      } else {
        epaper::drawTextLine(x + 12, baseline, line.text, true);
      }
      baseline += lh + 6;
      break;
    case RenderLineKind::Code:
      epaper::drawFilledRect(x, baseline - lh + 2, 2, lh, true);
      epaper::drawTextLine(x + 10, baseline, line.text, true);
      baseline += lh + 4;
      break;
    case RenderLineKind::Math:
      epaper::drawRect(x, baseline - lh + 1, epaper::textWidth(line.text) + 10, lh + 4, true, 1);
      epaper::drawTextLine(x + 5, baseline, line.text, true);
      baseline += lh + 6;
      break;
    case RenderLineKind::Quote:
      epaper::drawFilledRect(x, baseline - lh + 2, 2, lh, true);
      epaper::drawTextLine(x + 10, baseline, line.text, true);
      epaper::drawHLine(x + 10, baseline + 2, epaper::textWidth(line.text), true);
      baseline += lh + 6;
      break;
    case RenderLineKind::Rule:
      epaper::drawHLine(x, baseline - 4, epaper::canvasWidth() - (x * 2), true);
      baseline += 8;
      break;
    case RenderLineKind::Normal:
    default:
      if (line.bold) {
        epaper::drawTextLineBold(x, baseline, line.text, true);
      } else {
        epaper::drawTextLine(x, baseline, line.text, true);
      }
      baseline += lh + 6;
      break;
    }
  }
  return ESP_OK;
}
