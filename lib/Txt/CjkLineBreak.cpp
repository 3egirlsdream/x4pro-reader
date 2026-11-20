#include "CjkLineBreak.h"

#include <algorithm>

namespace txtLayout {
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
size_t fitLine(std::string& text, int width, Measure measure, void* context) {
  if (text.empty()) return 0;
  if (text.size() <= 64 && measure(text.c_str(), context) <= width) return text.size();
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
