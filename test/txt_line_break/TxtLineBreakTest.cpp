#include <CjkLineBreak.h>
#include <Utf8.h>
#include <gtest/gtest.h>

#include <random>
#include <string>

namespace {
struct Metrics {
  size_t calls = 0;
  size_t codepoints = 0;
};
int measure(const char* text, void* context) {
  auto& metrics = *static_cast<Metrics*>(context);
  ++metrics.calls;
  int width = 0;
  auto* cursor = reinterpret_cast<const unsigned char*>(text);
  while (const uint32_t cp = utf8NextCodepoint(&cursor)) {
    ++metrics.codepoints;
    width += cp == ' ' ? 4 : cp < 128 ? 8 : cp == 0x6c49 ? 14 : 16;
  }
  return width;
}
size_t originalBreak(const std::string& line, int width, Metrics& metrics) {
  if (measure(line.c_str(), &metrics) <= width) return line.size();
  size_t pos = line.size();
  while (pos && measure(line.substr(0, pos).c_str(), &metrics) > width) {
    const auto space = line.rfind(' ', pos - 1);
    if (space != std::string::npos && space > 0)
      pos = space;
    else {
      --pos;
      while (pos && (line[pos] & 0xc0) == 0x80) --pos;
    }
  }
  return pos ? pos : 1;
}
}  // namespace
TEST(TxtLineBreak, MixedParagraphsKeepMeasurementsBounded) {
  for (const char* unit : {"中\t文", "العربية", "한가", "かな", "中é", "123"}) {
    std::string line;
    while (line.size() < 8192) line += unit;
    Metrics metrics;
    auto count = txtLayout::fitLine(line, 420, measure, &metrics);
    EXPECT_GT(count, 0u);
    EXPECT_LT(metrics.calls, 20u);
    EXPECT_LT(metrics.codepoints, 500u);
    EXPECT_NE(static_cast<unsigned char>(line[count]) & 0xc0, 0x80);
  }
}
TEST(TxtLineBreak, MatchesOriginalWordPreferenceAndUnicodeBoundaries) {
  std::mt19937 random(0x123456);
  const char* tokens[] = {"汉", "字", "，", "。", " ", "  ", "123", "！",
                          "𠀀", "“",  "”",  "——", "‘", "…",  "ABC", "pīnyīn"};
  for (int trial = 0; trial < 240; ++trial) {
    std::string text = "汉";
    for (size_t i = 0, n = random() % 600; i < n; ++i) text += tokens[random() % std::size(tokens)];
    const auto saved = text;
    const int width = 32 + random() % 500;
    Metrics oldMetrics, newMetrics;
    EXPECT_EQ(txtLayout::fitLine(text, width, measure, &newMetrics), originalBreak(text, width, oldMetrics))
        << "trial=" << trial << " width=" << width << " prefix=" << text.substr(0, 150);
    EXPECT_EQ(text, saved);
  }
}
TEST(TxtLineBreak, WaterMarginDialogueKeepsWorkBounded) {
  std::string text;
  for (int i = 0; i < 300; ++i) text += "宋江问道：“武松——你可认得？”他说：‘正是。’ABC…";
  Metrics metrics;
  const auto pos = txtLayout::fitLine(text, 420, measure, &metrics);
  EXPECT_GT(pos, 0u);
  EXPECT_LT(metrics.calls, 20u);
  EXPECT_LT(metrics.codepoints, 400u);
}
TEST(TxtLineBreak, LongChineseParagraphDoesNotMeasureEveryTrailingCharacter) {
  std::string text;
  for (int i = 0; i < 1756; ++i) text += "汉";
  const auto saved = text;
  Metrics oldMetrics, newMetrics;
  EXPECT_EQ(txtLayout::fitLine(text, 420, measure, &newMetrics), originalBreak(text, 420, oldMetrics));
  EXPECT_LT(newMetrics.calls, 20u);
  EXPECT_GT(oldMetrics.calls, 1700u);
  EXPECT_LT(newMetrics.codepoints, 300u);
  EXPECT_EQ(text, saved);
}
TEST(TxtLineBreak, EmptyExactFitAndOversizeGlyphAlwaysMakeValidProgress) {
  Metrics metrics;
  std::string empty, exact = "汉字", tooWide = "𠀀字";
  EXPECT_EQ(txtLayout::fitLine(empty, 30, measure, &metrics), 0u);
  EXPECT_EQ(txtLayout::fitLine(exact, 30, measure, &metrics), exact.size());
  EXPECT_EQ(txtLayout::fitLine(tooWide, 1, measure, &metrics), 4u);
  EXPECT_EQ(tooWide, "𠀀字");
}
