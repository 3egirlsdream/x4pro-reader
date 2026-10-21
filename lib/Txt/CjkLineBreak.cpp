#include "CjkLineBreak.h"

#include <Utf8.h>

#include <algorithm>

namespace txtLayout {
bool isSimpleCjkLine(const char* text) {
  bool cjk = false;
  const auto* cursor = reinterpret_cast<const unsigned char*>(text);
  while (const uint32_t cp = utf8NextCodepoint(&cursor)) {
    // Han, Latin and common book punctuation. Contextual scripts, combining
    // sequences and direction controls keep the shaping-aware fallback.
    if ((cp >= 0x3400 && cp <= 0x9fff) || (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0x20000 && cp <= 0x2ebef) ||
        (cp >= 0x30000 && cp <= 0x323af) || (cp >= 0x3000 && cp <= 0x3029) || (cp >= 0xff01 && cp <= 0xff60)) {
      cjk = true;
    } else if ((cp >= 0x20 && cp <= 0x7e) || (cp >= 0xa0 && cp <= 0x24f) || cp == 0x261 ||
               (cp >= 0x2010 && cp <= 0x2027) || cp == 0x2030 || cp == 0x2032 || cp == 0x2033) {
      continue;
    } else {
      return false;
    }
  }
  return cjk;
}
namespace {
size_t boundary(const std::string& text, size_t offset) {
  while (offset && offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80) --offset;
  return offset;
}
size_t nextBoundary(const std::string& text, size_t offset) {
  if (offset < text.size()) ++offset;
  while (offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xc0) == 0x80) ++offset;
  return offset;
}
int prefixWidth(std::string& text, size_t length, Measure measure, void* context) {
  const char saved = text[length];
  text[length] = '\0';
  const int width = measure(text.c_str(), context);
  text[length] = saved;
  return width;
}
}  // namespace
size_t fitCjkLine(std::string& text, int width, Measure measure, void* context) {
  if (text.empty()) return 0;
  size_t low = 0;
  size_t high = nextBoundary(text, 0);
  // First bracket the first screen line, not the entire remaining paragraph.
  // For a long Chinese paragraph this bounds expensive glyph measurement by
  // the line's visible width rather than thousands of trailing characters.
  while (prefixWidth(text, high, measure, context) <= width) {
    low = high;
    if (high == text.size()) return high;
    high = boundary(text, high + std::min(high, text.size() - high));
    if (high <= low) high = nextBoundary(text, low);
  }
  while (nextBoundary(text, low) < high) {
    size_t mid = boundary(text, low + (high - low) / 2);
    if (mid <= low) mid = nextBoundary(text, low);
    if (prefixWidth(text, mid, measure, context) <= width)
      low = mid;
    else
      high = mid;
  }
  // A glyph wider than the viewport must still advance by one whole codepoint.
  if (!low) return nextBoundary(text, 0);
  const size_t space = text.rfind(' ', low);
  return space != std::string::npos && space > 0 ? space : low;
}
}  // namespace txtLayout
