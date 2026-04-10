#pragma once

#include <string>

namespace math {

// Strip surrounding $...$ or $$...$$ delimiters if present.
std::string stripDelimiters(const std::string &expr);

// Render a LaTeX math expression into the framebuffer.
// baseline_y : text baseline for the expression (same convention as drawTextLine)
// Returns the x coordinate immediately after the last rendered glyph.
int render(int x, int baseline_y, const std::string &expr, bool black);

// Measure the pixel width of a math expression without drawing anything.
int measure(const std::string &expr);

// Extra vertical space (pixels above baseline) needed for superscripts / frac numerators.
constexpr int kAscentExtra  = 18;
// Extra vertical space (pixels below baseline) needed for subscripts / frac denominators.
constexpr int kDescentExtra =  6;

}  // namespace math
