#pragma once

#include <cstddef>
#include <string>

namespace txtLayout {
// Restrict the fast path to CJK with Latin and common book punctuation. Other
// scripts keep the reader's original contextual shaping and wrapping path.
bool isSimpleCjkLine(const char* text);
using Measure = int (*)(const char* text, void* context);
// Finds a fitting UTF-8 boundary, retaining the original ASCII word boundary
// preference. Temporarily terminates the caller-owned buffer while measuring;
// no substring allocations and no paragraph-sized scratch allocation.
size_t fitCjkLine(std::string& text, int width, Measure measure, void* context);
}  // namespace txtLayout
