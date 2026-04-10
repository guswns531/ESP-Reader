#include "display_manager.h"

#include <algorithm>

#include "epaper_hal.h"

namespace {

constexpr int kMarginX = 16;
constexpr int kHeaderBaseline = 30;
constexpr int kDividerY = 40;
constexpr int kBodyStartY = 68;
constexpr int kFooterBaseline = 286;
constexpr int kSelectionBoxHeight = 26;
constexpr int kListRegionY = 48;
constexpr int kListRegionH = 220;

}  // namespace

esp_err_t DisplayManager::init()
{
    return epaper::init();
}

esp_err_t DisplayManager::showLibrary(const std::vector<DocumentEntry> &documents, std::size_t selected_index, bool partial)
{
    epaper::clear(false);
    epaper::drawTextLine(kMarginX, kHeaderBaseline, "LIBRARY", true);
    epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2), true);

    if (documents.empty()) {
        ESP_ERROR_CHECK(drawMultilineText(kMarginX, kBodyStartY, {"NO DOCUMENTS", "PUT .MD IN SPIFFS"}, false));
        epaper::drawTextLine(kMarginX, kFooterBaseline, "K3: OPEN", true);
        return epaper::updateFull();
    } else {
        int baseline = kBodyStartY;
        for (std::size_t i = 0; i < documents.size(); ++i) {
            const bool selected = i == selected_index;
            if (selected) {
                epaper::drawFilledRect(kMarginX - 6, baseline - 18, epaper::canvasWidth() - (kMarginX * 2) + 12, kSelectionBoxHeight, true);
            }
            epaper::drawTextLine(kMarginX, baseline, documents[i].title, !selected);
            baseline += epaper::lineHeight() + 10;
        }
    }

    epaper::drawTextLine(kMarginX, kFooterBaseline, "K4 UP  K3 DN  K2 OPEN", true);
    if (partial) {
        return epaper::updatePartial(0, kListRegionY, epaper::canvasWidth(), kListRegionH);
    }
    return epaper::updateFull();
}

esp_err_t DisplayManager::showReading(const DocumentEntry &document, std::size_t page_index)
{
    epaper::clear(false);
    epaper::drawTextLine(kMarginX, kHeaderBaseline, document.title, true);
    epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2), true);

    const std::size_t safe_index = std::min(page_index, document.pages.size() - 1);
    ESP_ERROR_CHECK(drawMultilineText(kMarginX, kBodyStartY, document.pages[safe_index].lines, false));

    const std::string footer = "P " + std::to_string(safe_index + 1) + "/" + std::to_string(document.pages.size()) +
                               "  K1 LIB  K4 <  K2 >";
    epaper::drawTextLine(kMarginX, kFooterBaseline, footer, true);
    return epaper::updateFull();
}

esp_err_t DisplayManager::showError(const std::string &message)
{
    epaper::clear(false);
    epaper::drawTextLine(kMarginX, kHeaderBaseline, "ERROR", true);
    epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2), true);
    return drawMultilineText(kMarginX, kBodyStartY, {message}, false) == ESP_OK ? epaper::updateFull() : ESP_FAIL;
}

esp_err_t DisplayManager::drawMultilineText(int x, int y, const std::vector<std::string> &lines, bool emphasized)
{
    (void)emphasized;
    int baseline = y;
    for (const auto &line : lines) {
        epaper::drawTextLine(x, baseline, line, true);
        baseline += epaper::lineHeight() + 4;
    }
    return ESP_OK;
}
