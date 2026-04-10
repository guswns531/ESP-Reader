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

void drawTopRightHint(const std::string &line1, const std::string &line2)
{
    const int right_margin = 12;
    epaper::setUiFont();
    const int x1 = epaper::canvasWidth() - right_margin - epaper::textWidth(line1);
    const int x2 = epaper::canvasWidth() - right_margin - epaper::textWidth(line2);
    epaper::drawTextLine(x1, 16, line1, true);
    epaper::drawTextLine(x2, 30, line2, true);
}

}  // namespace

esp_err_t DisplayManager::init()
{
    return epaper::init();
}

esp_err_t DisplayManager::showLibrary(const std::vector<DocumentEntry> &documents, std::size_t selected_index, bool partial)
{
    epaper::clear(false);
    epaper::setUiFont();
    epaper::drawTitleText(kMarginX, kHeaderBaseline, "LIBRARY", true);
    drawTopRightHint("UP/DN: MOVE", "RIGHT/ENTER");
    epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2), true);

    if (documents.empty()) {
        ESP_ERROR_CHECK(drawMultilineText(kMarginX, kBodyStartY, {"NO DOCUMENTS", "PUT .MD IN SPIFFS"}, false));
        return epaper::updateFull();
    } else {
        int baseline = kBodyStartY;
        for (std::size_t i = 0; i < documents.size(); ++i) {
            const bool selected = i == selected_index;
            const int label_width = epaper::textWidth(documents[i].title);
            if (selected) {
                epaper::drawRect(
                    kMarginX - kSelectionBoxPadX + 1,
                    baseline - epaper::lineHeight() - 3,
                    label_width + (kSelectionBoxPadX * 2),
                    epaper::lineHeight() + (kSelectionBoxPadY * 2) + 1,
                    true,
                    2);
            }
            epaper::drawTextLine(kMarginX, baseline, documents[i].title, true);
            baseline += epaper::lineHeight() + 8;
        }
    }
    if (partial) {
        return epaper::updatePartial(0, kListRegionY, epaper::canvasWidth(), kListRegionH);
    }
    return epaper::updateFull();
}

esp_err_t DisplayManager::showReading(const DocumentEntry &document, std::size_t page_index)
{
    epaper::clear(false);
    epaper::setUiFont();
    epaper::drawTitleText(kMarginX, kHeaderBaseline, document.title, true);
    drawTopRightHint("LEFT/RIGHT", "ENTER: LIB");
    epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2), true);

    const std::size_t safe_index = std::min(page_index, document.pages.size() - 1);
    ESP_ERROR_CHECK(drawMultilineText(kMarginX, kBodyStartY, document.pages[safe_index].lines, false));

    return epaper::updateFull();
}

esp_err_t DisplayManager::showError(const std::string &message)
{
    epaper::clear(false);
    epaper::setUiFont();
    epaper::drawTitleText(kMarginX, kHeaderBaseline, "ERROR", true);
    epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2), true);
    return drawMultilineText(kMarginX, kBodyStartY, {message}, false) == ESP_OK ? epaper::updateFull() : ESP_FAIL;
}

esp_err_t DisplayManager::drawMultilineText(int x, int y, const std::vector<std::string> &lines, bool emphasized)
{
    (void)emphasized;
    int baseline = y;
    for (const auto &line : lines) {
        epaper::drawTextLine(x, baseline, line, true);
        baseline += epaper::lineHeight() + 6;
    }
    return ESP_OK;
}
