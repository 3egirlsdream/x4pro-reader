#include "TxtReaderActivity.h"

#include <BidiUtils.h>
#include <CjkLineBreak.h>
#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>
#include <Performance.h>
#include <Serialization.h>
#include <Utf8.h>

#include "CrossPointSettings.h"
#include "ProgressFile.h"
#include "ReaderActivity.h"
#include "ReaderUtils.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t CHUNK_SIZE = 8 * 1024;  // 8KB chunk for reading
// Cache file magic and version
constexpr uint32_t CACHE_MAGIC = 0x54585449;  // "TXTI"
constexpr uint8_t CACHE_VERSION = 4;          // Increment when cache format changes
}  // namespace

bool TxtReaderActivity::loadBook() {
  txt = makeUniqueNoThrow<Txt>(bookPath, "/.crosspoint");
  if (!txt) {
    LOG_ERR("TRS", "Failed to allocate TXT object");
    return false;
  }
  if (!txt->load()) {
    LOG_ERR("TRS", "Failed to load TXT");
    return false;
  }
  txt->setupCacheDir();
  return true;
}

void TxtReaderActivity::initializeReader(GfxRenderer& renderer) {
  if (initialized) {
    return;
  }

  // Store current settings for cache validation
  cachedFontId = SETTINGS.getReaderFontId();
  cachedScreenMargin = SETTINGS.screenMargin;
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;

  // Calculate viewport dimensions
  renderer.getOrientedViewableTRBL(&cachedOrientedMarginTop, &cachedOrientedMarginRight, &cachedOrientedMarginBottom,
                                   &cachedOrientedMarginLeft);
  cachedOrientedMarginTop += cachedScreenMargin;
  cachedOrientedMarginLeft += cachedScreenMargin;
  cachedOrientedMarginRight += cachedScreenMargin;
  cachedOrientedMarginBottom +=
      std::max(cachedScreenMargin, static_cast<uint8_t>(UITheme::getInstance().getStatusBarHeight()));

  viewportWidth = renderer.getScreenWidth() - cachedOrientedMarginLeft - cachedOrientedMarginRight;
  const int viewportHeight = renderer.getScreenHeight() - cachedOrientedMarginTop - cachedOrientedMarginBottom;
  const int lineHeight = renderer.getLineHeight(cachedFontId);

  linesPerPage = viewportHeight / lineHeight;
  if (linesPerPage < 1) linesPerPage = 1;

  LOG_DBG("TRS", "Viewport: %dx%d, lines per page: %d", viewportWidth, viewportHeight, linesPerPage);

  // Opening never paginates the unread remainder of the book.
  if (!loadPageIndexCache()) {
    indexHeaderWritten = false;
    savedIndexEntries = 0;
    pageOffsets.clear();
    pageOffsets.reserve(128);
    pageOffsets.push_back(0);
    indexComplete = txt->getFileSize() == 0;
  }
  totalPages = static_cast<int>(pageOffsets.size());
  loadProgress();
  initialized = true;
}

void TxtReaderActivity::observePage(size_t page, size_t nextOffset) {
  if (nextOffset <= pageOffsets[page]) return;
  if (page + 1 == pageOffsets.size()) {
    if (nextOffset < txt->getFileSize()) {
      if (pageOffsets.size() == pageOffsets.capacity()) pageOffsets.reserve(pageOffsets.size() + 128);
      pageOffsets.push_back(nextOffset);
    } else {
      indexComplete = true;
    }
  }
  totalPages = static_cast<int>(pageOffsets.size());
}
float TxtReaderActivity::byteProgress() const {
  if (!txt || !txt->getFileSize() || currentPage < 0) return 0;
  if (restoreOffset != UINT32_MAX) return static_cast<float>(restoreOffset) * 100.0f / txt->getFileSize();
  if (currentPage >= static_cast<int>(pageOffsets.size())) return 100;
  return static_cast<float>(pageOffsets[currentPage]) * 100.0f / txt->getFileSize();
}

bool TxtReaderActivity::loadPageAtOffset(const GfxRenderer& renderer, size_t offset, std::vector<std::string>& outLines,
                                         size_t& nextOffset) {
  outLines.clear();
  const size_t fileSize = txt->getFileSize();

  if (offset >= fileSize) {
    return false;
  }

  // Read a chunk from file
  size_t chunkSize = std::min(CHUNK_SIZE, fileSize - offset);
  // One bounded scratch buffer, independent of book length.
  auto ownedBuffer = makeUniqueNoThrow<uint8_t[]>(chunkSize + 1);
  auto* buffer = ownedBuffer.get();
  if (!buffer) {
    LOG_ERR("TRS", "Failed to allocate %zu bytes", chunkSize);
    return false;
  }

  if (!txt->readContent(buffer, offset, chunkSize)) {
    return false;
  }
  if (offset + chunkSize < fileSize) {
    while (chunkSize && (buffer[chunkSize - 1] & 0xc0) == 0x80) --chunkSize;
    if (chunkSize && buffer[chunkSize - 1] >= 0xc0) --chunkSize;
  }
  buffer[chunkSize] = '\0';

  if (renderer.isSdCardFont(cachedFontId)) {
    renderer.ensureSdCardFontReady(cachedFontId, reinterpret_cast<const char*>(buffer), /*styleMask=*/0x01);
  }

  // Parse lines from buffer
  size_t pos = 0;

  while (pos < chunkSize && static_cast<int>(outLines.size()) < linesPerPage) {
    // Find end of line
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') {
      lineEnd++;
    }

    // Check if we have a complete line
    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= fileSize);

    if (!lineComplete && static_cast<int>(outLines.size()) > 0) {
      // Incomplete line and we already have some lines, stop here
      break;
    }

    size_t lineContentLen = lineEnd - pos;
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;

    std::string line(reinterpret_cast<char*>(buffer + pos), displayLen);
    size_t lineBytePos = 0;
    struct MeasureContext {
      const GfxRenderer& renderer;
      int fontId;
    } measureContext{renderer, cachedFontId};

    do {
      if (line.empty()) {
        outLines.emplace_back();
        break;
      }

      const size_t breakPos = txtLayout::fitLine(
          line, viewportWidth,
          [](const char* text, void* context) {
            const auto& m = *static_cast<MeasureContext*>(context);
            return m.renderer.getTextAdvanceX(m.fontId, text, EpdFontFamily::REGULAR);
          },
          &measureContext);

      if (breakPos == line.length()) {
        outLines.push_back(line);
        lineBytePos = displayLen;
        line.clear();
        break;
      }

      outLines.push_back(line.substr(0, breakPos));

      size_t skipChars = breakPos;
      if (breakPos < line.length() && line[breakPos] == ' ') {
        skipChars++;
      }
      lineBytePos += skipChars;
      line.erase(0, skipChars);
    } while (!line.empty() && static_cast<int>(outLines.size()) < linesPerPage);

    if (line.empty()) {
      pos = lineEnd + (lineEnd < chunkSize ? 1 : 0);
    } else {
      pos = pos + lineBytePos;
      break;
    }
  }

  if (pos == 0 && !outLines.empty()) {
    pos = 1;
  }

  nextOffset = offset + pos;
  if (nextOffset > fileSize) {
    nextOffset = fileSize;
  }

  return !outLines.empty();
}

void TxtReaderActivity::renderBook() {
  if (!txt) {
    return;
  }

  if (!initialized) {
    initializeReader(renderer);
  }

  if (txt->getFileSize() == 0 || pageOffsets.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  if (restoreOffset != UINT32_MAX || restorePage != 0) {
    // Release the render mutex between batches so Back, sleep and other system actions remain available.
    for (int slice = 0;
         slice < 2 && !indexComplete &&
         (restoreOffset != UINT32_MAX ? pageOffsets.back() < restoreOffset : pageOffsets.size() <= restorePage);
         ++slice) {
      const auto frontier = pageOffsets.size() - 1;
      size_t next = pageOffsets.back();
      if (!loadPageAtOffset(renderer, next, currentPageLines, next) || next <= pageOffsets.back()) {
        renderer.clearScreen();
        renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_PAGE_LOAD_ERROR), true, EpdFontFamily::BOLD);
        renderer.displayBuffer();
        return;
      }
      observePage(frontier, next);
    }
    if (!indexComplete &&
        (restoreOffset != UINT32_MAX ? pageOffsets.back() < restoreOffset : pageOffsets.size() <= restorePage)) {
      if (!restorePreview && restoreOffset != UINT32_MAX) {
        size_t next = restoreOffset;
        if (loadPageAtOffset(renderer, restoreOffset, currentPageLines, next)) {
          renderer.clearScreen();
          renderPage(renderer);
          restorePreview = true;
        }
      }
      requestUpdate();
      return;
    }
    currentPage =
        restoreOffset != UINT32_MAX
            ? int(std::upper_bound(pageOffsets.begin(), pageOffsets.end(), restoreOffset) - pageOffsets.begin()) - 1
            : int(std::min<size_t>(restorePage, pageOffsets.size() - 1));
    restoreOffset = UINT32_MAX;
    restorePage = 0;
  }

  // A long-press may request up to ten unseen pages; measure only that bounded prefix.
  while (!indexComplete && currentPage >= static_cast<int>(pageOffsets.size())) {
    const size_t frontier = pageOffsets.size() - 1;
    size_t next = pageOffsets[frontier];
    loadedPage = -1;
    if (!loadPageAtOffset(renderer, next, currentPageLines, next) || next <= pageOffsets[frontier]) break;
    observePage(frontier, next);
  }
  // Bounds check
  if (currentPage < 0) currentPage = 0;
  if (currentPage >= totalPages) currentPage = totalPages - 1;

  // Load current page content
  size_t offset = pageOffsets[currentPage];
  size_t nextOffset = offset;
  const auto layoutStarted = millis();
  const bool reusedLayout = loadedPage == currentPage;
  if (reusedLayout) nextOffset = loadedNextOffset;
  if (!reusedLayout && !loadPageAtOffset(renderer, offset, currentPageLines, nextOffset)) {
    loadedPage = -1;
    LOG_ERR("TRS", "Failed to load page at offset %zu", offset);
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, tr(STR_PAGE_LOAD_ERROR), true, EpdFontFamily::BOLD);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  loadedPage = currentPage;
  loadedNextOffset = nextOffset;
  observePage(currentPage, nextOffset);
  layoutMillis = millis() - layoutStarted;
  performance::record(performance::Stage::TxtLayout, layoutMillis);
  LOG_INF("TRS", "page layout %lu ms; known=%u complete=%d", static_cast<unsigned long>(layoutMillis),
          static_cast<unsigned>(pageOffsets.size()), indexComplete.load());
  renderer.clearScreen();
  renderPage(renderer);
  markPageRendered();

  // Append discovered offsets before committing the matching reading position.
  performance::Scope persistTiming(performance::Stage::TxtPersist);
  if (savePageIndexCache()) saveProgress();
}

void TxtReaderActivity::renderPage(GfxRenderer& renderer) {
  const int lineHeight = renderer.getLineHeight(cachedFontId);
  const int contentWidth = viewportWidth;

  // Render text lines with alignment
  auto renderLines = [&]() {
    int y = cachedOrientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = cachedOrientedMarginLeft;
        const bool lineIsRtl = BidiUtils::startsWithRtl(line.c_str(), BidiUtils::RTL_PARAGRAPH_PROBE_DEPTH);
        uint8_t effectiveAlignment = cachedParagraphAlignment;
        if (lineIsRtl && (effectiveAlignment == CrossPointSettings::LEFT_ALIGN ||
                          effectiveAlignment == CrossPointSettings::JUSTIFIED)) {
          effectiveAlignment = CrossPointSettings::RIGHT_ALIGN;
        }
        const int textWidth = effectiveAlignment == CrossPointSettings::CENTER_ALIGN ||
                                      effectiveAlignment == CrossPointSettings::RIGHT_ALIGN
                                  ? renderer.getTextAdvanceX(cachedFontId, line.c_str(), EpdFontFamily::REGULAR)
                                  : 0;

        // Apply text alignment
        switch (effectiveAlignment) {
          case CrossPointSettings::LEFT_ALIGN:
          default:
            break;
          case CrossPointSettings::CENTER_ALIGN: {
            x = cachedOrientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            x = cachedOrientedMarginLeft + contentWidth - textWidth;
            break;
          }
          case CrossPointSettings::JUSTIFIED:
            break;
        }

        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
  };

  // Font prewarm: scan pass accumulates text, then prewarm, then real render
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  renderLines();      // scan pass
  renderStatusBar();  // scan: a CJK title joins the batch prewarm
  scope.endScanAndPrewarm();

  // BW rendering
  renderLines();
  renderStatusBar();

  if (SETTINGS.textAntiAliasing) {
    ReaderUtils::displayBaseWithRefreshCycle(renderer, pagesUntilFullRefresh);
    ReaderUtils::renderAntiAliased(renderer, [&renderLines]() { renderLines(); });
  } else {
    ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
  }
}

void TxtReaderActivity::renderStatusBar() const {
  const float progress = byteProgress();
  std::string title;
  if (SETTINGS.statusBarSpec().showsTitle()) {
    title = txt->getTitle();
  }
  const size_t consumed =
      currentPage + 1 < static_cast<int>(pageOffsets.size()) ? pageOffsets[currentPage + 1] : txt->getFileSize();
  const int estimate =
      indexComplete || !consumed
          ? totalPages
          : std::max(totalPages + 1,
                     static_cast<int>((uint64_t(txt->getFileSize()) * (currentPage + 1) + consumed - 1) / consumed));
  GUI.drawStatusBar(renderer, progress, currentPage + 1, estimate, title, 0, 0, true, false, !indexComplete);
}

bool TxtReaderActivity::pageTurn(bool isForward) {
  RenderLock lock(*this);
  // Ignore paging until the first visible page is ready
  if (!initialized || restorePage || restoreOffset != UINT32_MAX) {
    return false;
  }
  if (isForward) {
    if (currentPage + 1 < static_cast<int>(pageOffsets.size()) ||
        (indexComplete && currentPage < static_cast<int>(pageOffsets.size()))) {
      currentPage++;
      return true;
    }
  } else {
    if (currentPage > 0) {
      currentPage--;
      return true;
    }
  }
  return false;
}

bool TxtReaderActivity::skipPages(int amount) {
  RenderLock lock(*this);
  if (!initialized || restorePage || restoreOffset != UINT32_MAX) {
    return false;
  }
  int newPage = currentPage + amount;
  if (newPage < 0) newPage = 0;
  // Clamp to totalPages, not totalPages - 1: pageTurn() lets currentPage reach
  // totalPages and isAtEndOfBook() treats that as the end-of-book sentinel, so
  // a forward skip must be able to reach it too.
  const int limit = indexComplete ? totalPages : currentPage.load() + std::max(0, std::min(amount, 10));
  if (newPage > limit) newPage = limit;
  if (newPage != currentPage) {
    currentPage = newPage;
    return true;
  }
  return false;
}

bool TxtReaderActivity::isAtEndOfBook() const {
  return initialized && indexComplete && currentPage >= static_cast<int>(pageOffsets.size());
}

void TxtReaderActivity::onReturnFromEndOfBook() { currentPage = totalPages > 0 ? totalPages - 1 : 0; }

void TxtReaderActivity::saveProgress() {
  if (savedProgressPage == currentPage) return;
  const uint32_t data[] = {static_cast<uint32_t>(currentPage.load()), static_cast<uint32_t>(pageOffsets[currentPage]),
                           static_cast<uint32_t>(txt->getFileSize())};
  if (!ProgressFile::writeAtomic(txt->getCachePath(), reinterpret_cast<const uint8_t*>(data), sizeof(data))) {
    LOG_ERR("TRS", "Failed to save progress: page %d", currentPage.load());
  } else {
    savedProgressPage = currentPage;
  }
}

void TxtReaderActivity::loadProgress() {
  HalFile f;
  if (ProgressFile::open(txt->getCachePath(), ProgressFile::Format::Txt, f)) {
    uint32_t data[3]{};
    const int size = f.read(data, sizeof(data));
    if (size != 4 && size != sizeof(data)) return;
    if (size == sizeof(data) && (data[2] != txt->getFileSize() || data[1] >= data[2])) return;
    const uint32_t page = size == 4 ? data[0] & 0xffff : data[0];
    if (size == sizeof(data)) {
      restoreOffset = data[1];
    } else if (page < pageOffsets.size()) {
      currentPage = static_cast<int>(page);
      return;
    } else {
      // The old layout can still translate a legacy page number into a byte position.
      HalFile old;
      uint8_t header[30]{};
      uint32_t magic = 0, length = 0, offset = 0;
      if (Storage.openFileForRead("TRS", txt->getCachePath() + "/index.bin", old) &&
          old.read(header, sizeof(header)) == sizeof(header)) {
        memcpy(&magic, header, 4);
        memcpy(&length, header + 5, 4);
        if (magic == CACHE_MAGIC && (header[4] == 3 || header[4] == CACHE_VERSION) && length == txt->getFileSize() &&
            old.seek(sizeof(header) + page * 4) && old.read(&offset, 4) == 4 && offset < length && offset > 0)
          restoreOffset = offset;
      }
      if (restoreOffset == UINT32_MAX) restorePage = page;
    }
  }
}

bool TxtReaderActivity::loadPageIndexCache() {
  std::string cachePath = txt->getCachePath() + "/index.bin";
  HalFile f;
  if (!Storage.openFileForRead("TRS", cachePath, f)) {
    LOG_DBG("TRS", "No page index cache found");
    return false;
  }

  if (f.size() < 30) return false;
  uint32_t magic = 0;
  serialization::readPod(f, magic);
  if (magic != CACHE_MAGIC) {
    LOG_DBG("TRS", "Cache magic mismatch, rebuilding");
    return false;
  }

  uint8_t version;
  serialization::readPod(f, version);
  if (version != CACHE_VERSION && version != 3) {
    LOG_DBG("TRS", "Cache version mismatch (%d != %d), rebuilding", version, CACHE_VERSION);
    return false;
  }

  uint32_t fileSize;
  serialization::readPod(f, fileSize);
  if (fileSize != txt->getFileSize()) {
    LOG_DBG("TRS", "Cache file size mismatch, rebuilding");
    return false;
  }

  int32_t cachedWidth;
  serialization::readPod(f, cachedWidth);
  if (cachedWidth != viewportWidth) {
    LOG_DBG("TRS", "Cache viewport width mismatch, rebuilding");
    return false;
  }

  int32_t cachedLines;
  serialization::readPod(f, cachedLines);
  if (cachedLines != linesPerPage) {
    LOG_DBG("TRS", "Cache lines per page mismatch, rebuilding");
    return false;
  }

  int32_t fontId;
  serialization::readPod(f, fontId);
  if (fontId != cachedFontId) {
    LOG_DBG("TRS", "Cache font ID mismatch (%d != %d), rebuilding", fontId, cachedFontId);
    return false;
  }

  int32_t margin;
  serialization::readPod(f, margin);
  if (margin != cachedScreenMargin) {
    LOG_DBG("TRS", "Cache screen margin mismatch, rebuilding");
    return false;
  }

  uint8_t alignment;
  serialization::readPod(f, alignment);
  if (alignment != cachedParagraphAlignment) {
    LOG_DBG("TRS", "Cache paragraph alignment mismatch, rebuilding");
    return false;
  }

  uint32_t numPages = 0;
  serialization::readPod(f, numPages);
  const size_t headerSize = f.position();
  if (version == CACHE_VERSION) {
    indexComplete = (numPages & 0x80000000U) != 0;
    savedIndexComplete = indexComplete;
    numPages = (f.size() - headerSize) / sizeof(uint32_t);
    savedIndexEntries = numPages;
    indexHeaderWritten = true;
  } else {
    indexComplete = true;
  }
  if (!numPages || numPages > 100000 || headerSize + size_t(numPages) * sizeof(uint32_t) > f.size()) return false;

  pageOffsets.clear();
  pageOffsets.reserve(numPages);

  uint32_t offsets[32];
  for (uint32_t first = 0; first < numPages;) {
    const size_t count = std::min<size_t>(32, numPages - first);
    if (f.read(offsets, count * sizeof(uint32_t)) != int(count * sizeof(uint32_t))) return false;
    for (size_t i = 0; i < count; ++i) {
      const auto offset = offsets[i];
      if ((first + i == 0 && offset != 0) || (!pageOffsets.empty() && offset <= pageOffsets.back()) ||
          (offset >= txt->getFileSize() && offset != 0)) {
        pageOffsets.clear();
        indexHeaderWritten = false;
        savedIndexEntries = 0;
        return false;
      }
      pageOffsets.push_back(offset);
    }
    first += count;
  }

  totalPages = pageOffsets.size();
  LOG_DBG("TRS", "Loaded page index cache: %d pages", totalPages);
  return true;
}

bool TxtReaderActivity::savePageIndexCache() {
  if (indexHeaderWritten && savedIndexEntries == pageOffsets.size() && savedIndexComplete == indexComplete) return true;
  const std::string cachePath = txt->getCachePath() + "/index.bin";
  auto f = Storage.open(cachePath.c_str(), O_RDWR | O_CREAT);
  if (!f) return false;
  constexpr size_t countPosition = 26;
  constexpr size_t headerSize = countPosition + sizeof(uint32_t);
  if (!indexHeaderWritten) {
    f.close();
    if (!Storage.openFileForWrite("TRS", cachePath, f)) return false;
    // Match the v4 little-endian header; publish state only after a complete write.
    uint8_t header[headerSize]{};
    size_t at = 0;
    const auto append = [&](uint32_t value, size_t bytes) {
      for (size_t i = 0; i < bytes; ++i) header[at++] = static_cast<uint8_t>(value >> (i * 8));
    };
    append(CACHE_MAGIC, 4);
    append(CACHE_VERSION, 1);
    append(txt->getFileSize(), 4);
    append(viewportWidth, 4);
    append(linesPerPage, 4);
    append(cachedFontId, 4);
    append(cachedScreenMargin, 4);
    append(cachedParagraphAlignment, 1);
    append(0, 4);
    if (f.write(header, sizeof(header)) != sizeof(header)) return false;
    savedIndexEntries = 0;
    savedIndexComplete = false;
    indexHeaderWritten = true;
  }
  if (!f.seek(headerSize + savedIndexEntries * sizeof(uint32_t))) return false;
  uint32_t offsets[32];
  while (savedIndexEntries < pageOffsets.size()) {
    const size_t count = std::min<size_t>(32, pageOffsets.size() - savedIndexEntries);
    for (size_t i = 0; i < count; ++i) offsets[i] = pageOffsets[savedIndexEntries + i];
    const size_t bytes = count * sizeof(offsets[0]);
    if (f.write(offsets, bytes) != bytes) return false;
    savedIndexEntries += count;
  }
  // Complete offsets must reach the card before publishing EOF or progress.
  f.flush();
  if (savedIndexComplete != indexComplete) {
    if (!f.seek(countPosition)) return false;
    const uint32_t flags = indexComplete ? 0x80000000U : 0;
    if (f.write(&flags, sizeof(flags)) != sizeof(flags)) return false;
    f.flush();
    savedIndexComplete = indexComplete;
  }
  return true;
}

ScreenshotInfo TxtReaderActivity::getScreenshotInfo() const {
  ScreenshotInfo info;
  info.readerType = ScreenshotInfo::ReaderType::Txt;
  if (txt) {
    const std::string t = txt->getTitle();
    snprintf(info.title, sizeof(info.title), "%s", t.c_str());
  }
  info.currentPage = currentPage + 1;
  info.totalPages = totalPages;
  info.progressPercent = static_cast<int>(byteProgress() + 0.5f);
  if (info.progressPercent > 100) info.progressPercent = 100;
  return info;
}
