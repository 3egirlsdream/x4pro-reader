#pragma once

#include <cstddef>
#include <string>

namespace txtLayout {
using Measure = int (*)(const char* text, void* context);
// Finds a fitting UTF-8 boundary, retaining the original ASCII word boundary
// preference. Temporarily terminates the caller-owned buffer while measuring;
// no substring allocations and no paragraph-sized scratch allocation.
size_t fitLine(std::string& text, int width, Measure measure, void* context);
}  // namespace txtLayout
