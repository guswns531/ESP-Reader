#include "display_manager.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "epaper_hal.h"
#include "math_renderer.h"

namespace {

constexpr int kMarginX = 8;
constexpr int kHeaderBaseline = 26;
constexpr int kReadingTitleBaseline = 20;
constexpr int kReadingSubtitleBaseline = 39;
constexpr int kDividerY = 48;
constexpr int kBodyStartY = 60;
constexpr int kSelectionBoxPadX = 6;
constexpr int kSelectionBoxPadY = 3;
constexpr int kFooterBaseline = 294;
constexpr std::size_t kTocVisibleRows = 8;


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
  epaper::drawTriangleRight(352, 22, 4, true);
  epaper::drawTextLine(362, 30, "TOC", true);
}

void drawTopRightHintToc() {
  epaper::drawTriangleUp(304, 11, 4, true);
  epaper::drawTriangleDown(314, 11, 4, true);
  epaper::drawTextLine(324, 18, "SELECT", true);

  epaper::drawTriangleLeft(305, 22, 4, true);
  epaper::drawTextLine(314, 30, "BACK", true);
  epaper::drawTriangleRight(352, 22, 4, true);
  epaper::drawTextLine(362, 30, "GO", true);
}

std::string tocTreePrefix(int level) {
  if (level <= 1) {
    return "";
  }
  return std::string(static_cast<std::size_t>(level - 1), '>') + " ";
}

std::string currentHeadingLabel(const DocumentEntry &document,
                                std::size_t page_index) {
  std::string current = document.title;
  for (const auto &entry : document.toc) {
    if (entry.page_index > page_index) {
      break;
    }
    current = entry.title;
  }
  return current;
}

void drawReadingHeader(const DocumentEntry &document, std::size_t page_index,
                       const std::string &subtitle) {
  const std::string title = document.title;
  const int available_width = 292 - kMarginX;
  std::string clipped_title = title;
  while (!clipped_title.empty() &&
         epaper::headerTextWidth(clipped_title) > available_width) {
    clipped_title.pop_back();
  }
  epaper::drawHeaderText(kMarginX, kReadingTitleBaseline, clipped_title, true);
  epaper::drawTextLine(kMarginX, kReadingSubtitleBaseline,
                       subtitle.empty() ? currentHeadingLabel(document, page_index)
                                        : subtitle,
                       true);
}

bool readPbmImage(const std::string &asset_path, int *width, int *height,
                  std::vector<uint8_t> *data) {
  std::ifstream file("/spiffs/" + asset_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  std::string magic;
  file >> magic;
  if (magic != "P4") {
    return false;
  }

  file >> *width >> *height;
  file.get();

  const std::size_t row_bytes = static_cast<std::size_t>((*width + 7) / 8);
  data->assign(row_bytes * static_cast<std::size_t>(*height), 0x00);
  file.read(reinterpret_cast<char *>(data->data()),
            static_cast<std::streamsize>(data->size()));
  return file.good() || file.eof();
}

void drawPbmImage(int x, int y, const RenderLine &line) {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> data;
  if (!readPbmImage(line.asset_path, &width, &height, &data)) {
    epaper::drawTextLine(x, y, "[math image missing]", true);
    return;
  }

  const std::size_t row_bytes = static_cast<std::size_t>((width + 7) / 8);
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      const uint8_t byte = data[static_cast<std::size_t>(yy) * row_bytes + static_cast<std::size_t>(xx / 8)];
      const uint8_t mask = static_cast<uint8_t>(0x80u >> (xx & 7));
      if ((byte & mask) != 0) {
        epaper::drawFilledRect(x + xx, y + yy, 1, 1, true);
      }
    }
  }
}

bool parseIntStrict(const std::string &text, int *value) {
  char *end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0') {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

bool parseInlineMathMarker(const std::string &text, std::size_t start,
                           std::size_t *end, RenderLine *line) {
  constexpr const char *kPrefix = "[[MATH_PBM|";
  constexpr const char *kSuffix = "]]";
  if (text.compare(start, std::strlen(kPrefix), kPrefix) != 0) {
    return false;
  }
  const std::size_t close = text.find(kSuffix, start + std::strlen(kPrefix));
  if (close == std::string::npos) {
    return false;
  }
  const std::string marker = text.substr(start, close + std::strlen(kSuffix) - start);
  std::size_t sep1 = marker.find('|');
  std::size_t sep2 = marker.find('|', sep1 + 1);
  std::size_t sep3 = marker.find('|', sep2 + 1);
  if (sep1 == std::string::npos || sep2 == std::string::npos || sep3 == std::string::npos) {
    return false;
  }
  line->asset_path = marker.substr(sep1 + 1, sep2 - sep1 - 1);
  if (!parseIntStrict(marker.substr(sep2 + 1, sep3 - sep2 - 1), &line->asset_width) ||
      !parseIntStrict(marker.substr(sep3 + 1, marker.size() - sep3 - 1 - std::strlen(kSuffix)), &line->asset_height)) {
    return false;
  }
  *end = close + std::strlen(kSuffix);
  return true;
}

int drawInlineRichText(int x, int baseline, const std::string &text, bool bold) {
  int cursor = x;
  std::size_t i = 0;
  while (i < text.size()) {
    RenderLine marker;
    std::size_t marker_end = i;
    if (parseInlineMathMarker(text, i, &marker_end, &marker)) {
      drawPbmImage(cursor, baseline - marker.asset_height + 2, marker);
      cursor += marker.asset_width + 2;
      i = marker_end;
      continue;
    }

    const std::size_t next = text.find("[[MATH_PBM|", i);
    const std::string chunk = text.substr(i, next == std::string::npos ? std::string::npos : next - i);
    if (!chunk.empty()) {
      if (bold) {
        epaper::drawTextLineBold(cursor, baseline, chunk, true);
      } else {
        epaper::drawTextLine(cursor, baseline, chunk, true);
      }
      cursor += epaper::textWidth(chunk);
    }
    if (next == std::string::npos) {
      break;
    }
    i = next;
  }
  return cursor - x;
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
        {RenderLine{.text = "NO DOCUMENTS", .kind = RenderLineKind::Normal, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0},
         RenderLine{.text = "PUT .MD IN SPIFFS", .kind = RenderLineKind::Normal, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0}},
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
  const std::size_t safe_index =
      std::min(page_index, document.pages.size() - 1);
  drawReadingHeader(document, safe_index, "");
  drawTopRightHintReading();
  epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2),
                    true);

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

esp_err_t DisplayManager::showToc(const DocumentEntry &document,
                                  std::size_t selected_toc_index,
                                  std::size_t current_page_index, bool partial,
                                  bool force_full) {
  epaper::clear(false);
  epaper::setUiFont();
  drawReadingHeader(document, current_page_index, "목차");
  drawTopRightHintToc();
  epaper::drawHLine(kMarginX, kDividerY, epaper::canvasWidth() - (kMarginX * 2),
                    true);

  if (document.toc.empty()) {
    ESP_ERROR_CHECK(drawMultilineText(
        kMarginX, kBodyStartY,
        {RenderLine{.text = "NO HEADINGS", .kind = RenderLineKind::Normal, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0},
         RenderLine{.text = "PRESS LEFT TO RETURN", .kind = RenderLineKind::Normal, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0}},
        false));
  } else {
    const std::size_t safe_index =
        std::min(selected_toc_index, document.toc.size() - 1);
    const std::size_t window_start =
        safe_index < kTocVisibleRows ? 0 : (safe_index - kTocVisibleRows + 1);
    const std::size_t window_end =
        std::min(window_start + kTocVisibleRows, document.toc.size());

    int baseline = kBodyStartY;
    for (std::size_t i = window_start; i < window_end; ++i) {
      const auto &entry = document.toc[i];
      const bool selected = i == safe_index;
      const bool current = entry.page_index == current_page_index;
      const std::string tree_prefix = tocTreePrefix(entry.level);
      std::string label =
          tree_prefix + entry.title + "  p." +
          std::to_string(entry.page_index + 1);
      if (current) {
        label += " *";
      }
      const int x = kMarginX;
      const int label_width = epaper::textWidth(label);
      if (selected) {
        epaper::drawRect(x - kSelectionBoxPadX + 1, baseline - epaper::lineHeight() - 3,
                         label_width + (kSelectionBoxPadX * 2),
                         epaper::lineHeight() + (kSelectionBoxPadY * 2) + 1,
                         true, 2);
      }
      epaper::drawTextLine(x, baseline, label, true);
      baseline += epaper::lineHeight() + 8;
    }
  }

  const std::string footer =
      document.toc.empty()
          ? "0 / 0"
          : std::to_string(std::min(selected_toc_index, document.toc.size() - 1) + 1) +
                " / " + std::to_string(document.toc.size());
  const int footer_x = epaper::canvasWidth() - 8 - epaper::textWidth(footer);
  epaper::drawTextLine(footer_x, kFooterBaseline, footer, true);

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
                           {RenderLine{.text = message, .kind = RenderLineKind::Normal, .bold = false, .asset_path = "", .asset_width = 0, .asset_height = 0}}, false) == ESP_OK
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
      drawInlineRichText(x + 12, baseline, line.text, line.bold);
      baseline += lh + 6;
      break;
    case RenderLineKind::Code:
      epaper::drawFilledRect(x, baseline - lh + 2, 2, lh, true);
      epaper::drawTextLine(x + 10, baseline, line.text, true);
      baseline += lh + 4;
      break;
    case RenderLineKind::Math: {
      if (!line.asset_path.empty()) {
        drawPbmImage(x, baseline - lh + 2, line);
        baseline += std::max(line.asset_height, lh) + 6;
      } else {
        const int math_baseline = baseline + math::kAscentExtra;
        math::render(x, math_baseline, line.text, true);
        baseline += lh + math::kAscentExtra + math::kDescentExtra + 4;
      }
      break;
    }
    case RenderLineKind::Quote:
      epaper::drawFilledRect(x, baseline - lh + 2, 2, lh, true);
      epaper::drawHLine(x + 10, baseline + 2, drawInlineRichText(x + 10, baseline, line.text, false), true);
      baseline += lh + 6;
      break;
    case RenderLineKind::Rule:
      epaper::drawHLine(x, baseline - 4, epaper::canvasWidth() - (x * 2), true);
      baseline += 8;
      break;
    case RenderLineKind::Normal:
    default:
      drawInlineRichText(x, baseline, line.text, line.bold);
      baseline += lh + 6;
      break;
    }
  }
  return ESP_OK;
}
