#include "DesktopShelf.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <Memory.h>
#include <Performance.h>
#include <Txt.h>
#include <Xtc.h>

#include "ReadingHistory.h"
#include "util/BookProgress.h"

namespace desktop {
namespace {
uint32_t orderChecksum(const Rank* ranks, size_t count) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(ranks);
  uint32_t value = 2166136261u;
  for (size_t i = 0; i < count * sizeof(Rank); ++i) value = (value ^ bytes[i]) * 16777619u;
  return value;
}
}  // namespace
Shelf::~Shelf() { stop(); }
void Shelf::stop() {
  performance::Scope timing(performance::Stage::ShelfStop);
  cancel = true;
  // The worker owns no Activity state after building becomes false.
  while (building.load() || coverBusy.load()) delay(1);
  index.close();
}
void Shelf::begin() {
  openIndex();
  needsBuild = !index.isOpen() || library::isLibraryIndexDirty();
  if (!needsBuild && !error) {
    if (loadOrder()) {
      scanning = false;
      loadPage(0);
    } else
      total = 0;
  }
}
void Shelf::openIndex() {
  index.close();
  ranksMemory.reset();
  ranks = nullptr;
  capacity = 0;
  error = false;
  total = cursor = shown = 0;
  scanning = false;
  if (!index.open(library::libraryIndexPath())) return;
  capacity = index.bookCount();
  // Compact ranks scale with the bounded CLIX format, in PSRAM; book strings stay page-sized.
  ranksMemory = HalMemory::allocatePsram(std::max<size_t>(1, capacity) * sizeof(Rank));
  ranks = reinterpret_cast<Rank*>(ranksMemory.get());
  if (!ranks) {
    error = true;
    LOG_ERR("DESKTOP", "OOM: shelf ranks");
    return;
  }
  scanning = true;
}
void Shelf::buildTask(void* context) {
  auto* self = static_cast<Shelf*>(context);
  const library::BuildControl control{self, [](void* c) { return !static_cast<Shelf*>(c)->cancel.load(); }};
  library::BuildStats stats;
  self->buildOk = library::buildLibraryIndex("/", stats, false, &control);
  self->buildDone = true;
  self->building = false;
  vTaskDelete(nullptr);
}
void Shelf::coverTask(void* context) {
  auto* self = static_cast<Shelf*>(context);
  const auto started = millis();
  const auto& path = self->coverJobPath;
  std::string result;
  if (!self->cancel.load() && FsHelpers::hasEpubExtension(path)) {
    auto epub = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
    if (epub && epub->generateThumbBmpFromSource(self->jobHeight)) result = epub->getThumbBmpPath(self->jobHeight);
  } else if (!self->cancel.load() && FsHelpers::hasXtcExtension(path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(path, "/.crosspoint");
    if (xtc && xtc->load() && xtc->generateThumbBmp(self->jobHeight)) result = xtc->getThumbBmpPath(self->jobHeight);
  } else if (!self->cancel.load()) {
    auto txt = makeUniqueNoThrow<Txt>(path, "/.crosspoint");
    if (txt) {
      txt->setupCacheDir();
      if (txt->generateCoverBmp()) result = txt->getCoverBmpPath();
    }
  }
  LOG_INF("DESKTOP", "visible_cover slot=%d height=%d software_ms=%lu", self->coverSlot, self->jobHeight,
          static_cast<unsigned long>(millis() - started));
  performance::record(performance::Stage::ShelfCover, millis() - started);
  self->coverResult = std::move(result);
  self->coverDone = true;
  self->coverBusy = false;
  vTaskDelete(nullptr);
}
bool Shelf::step(int coverHeight) {
  if (coverDone.load() && !coverBusy.load()) {
    coverDone = false;
    if (!coverResult.empty() && books[coverSlot].path == coverJobPath) {
      books[coverSlot].cover = std::move(coverResult);
      return true;
    }
  }
  if (buildDone.exchange(false)) {
    error = !buildOk.load();
    if (!error) openIndex();
    return true;
  }
  if (needsBuild && !scanning && !building.load()) {
    needsBuild = false;
    index.close();
    cancel = false;
    building = true;
    // No book parsers: the builder's bounded batches yield and observe cancellation.
    if (xTaskCreate(&Shelf::buildTask, "DesktopIndex", 8192, this, 1, nullptr) != pdPASS) {
      building = false;
      error = true;
    }
    return true;
  }
  if (!scanning) {
    if (!building.load() && !coverBusy.load() && coverHeight > 0 && !error) {
      for (int i = 0; i < shown; ++i) {
        if (coverAttempted[i]) continue;
        coverAttempted[i] = true;
        if (!books[i].cover.empty() && books[i].cover.find("[HEIGHT]") == std::string::npos) continue;
        coverSlot = i;
        jobHeight = coverHeight;
        coverJobPath = books[i].path;
        cancel = false;
        coverBusy = true;
        if (xTaskCreate(&Shelf::coverTask, "DesktopCover", 8192, this, 1, nullptr) != pdPASS) coverBusy = false;
        break;
      }
    }
    return false;
  }
  // At most four indexed paths per tick; no directory walk or book parsing here.
  for (int work = 0; work < 4 && cursor < capacity; ++work, ++cursor) {
    std::string path;
    if (!index.readRecord(cursor, record) || !index.readPath(record, path)) {
      error = true;
      scanning = false;
      return true;
    }
    if (!Storage.exists(path.c_str())) continue;
    const auto history = readStamp(path);
    uint64_t stamp = history.sequence ? (uint64_t(1) << 63) | history.sequence : 0;
    if (!stamp) {
      const auto& recent = RECENT_BOOKS.getBooks();
      for (size_t i = 0; i < recent.size(); ++i) {
        if (recent[i].path == path) {
          stamp = (uint64_t(1) << 62) | (recent.size() - i);
          break;
        }
      }
      if (!stamp) {
        HalFile progress;
        if (Storage.openFileForRead("DESKTOP", cachePathFor(path) + "/progress.bin", progress))
          stamp = uint64_t(progress.modificationTime()) + 1;
      }
    }
    ranks[total++] = {stamp, cursor};
  }
  if (cursor < capacity) return false;
  scanning = false;
  std::sort(ranks, ranks + total, recentFirst);
  saveOrder();
  loadPage(0);
  LOG_INF("DESKTOP", "index ready books=%u rank_bytes=%u page_books=%d", unsigned(total), unsigned(rankBytes()), shown);
  return true;
}
bool Shelf::loadPage(int value) {
  cancel = true;
  while (coverBusy.load()) delay(1);
  coverDone = false;
  coverAttempted.fill(false);
  page = std::clamp(value, 0, pageCount(total) - 1);
  shown = 0;
  for (auto& book : books) book = {};
  int row = page * BOOKS_PER_PAGE;
  while (row < total && shown < BOOKS_PER_PAGE) {
    Book& book = books[shown];
    if (!index.readRecord(ranks[row].ordinal, record) || !index.readPath(record, book.path)) {
      error = true;
      break;
    }
    if (!Storage.exists(book.path.c_str())) {
      std::move(ranks + row + 1, ranks + total, ranks + row);
      --total;
      continue;
    }
    index.readTitle(record, book.title);
    if (book.title.empty()) index.readName(record, book.title);
    const auto history = readStamp(book.path);
    book.percent = history.percent;
    book.progress = history.finished ? Progress::Finished : ranks[row].stamp ? Progress::Unknown : Progress::Unread;
    if (book.percent < 0 && ranks[row].stamp) book.percent = loadBookProgress(book.path);
    if (!history.finished && book.percent >= 0)
      book.progress = book.percent == 100 ? Progress::Finished : Progress::Reading;
    const auto cache = cachePathFor(book.path);
    // Page materialization reads cached paths; visible missing covers are queued separately.
    if (!FsHelpers::hasTxtExtension(book.path) && !FsHelpers::hasMarkdownExtension(book.path)) {
      if (Storage.exists((cache + "/cover_legacy_v2.bmp").c_str()))
        book.cover = cache + "/cover_legacy_v2.bmp";
      else if (Storage.exists((cache + "/cover.bmp").c_str()))
        book.cover = cache + "/cover.bmp";
      if (book.cover.empty()) {
        for (const auto& recent : RECENT_BOOKS.getBooks())
          if (recent.path == book.path) book.cover = recent.coverBmpPath;
      }
    }
    ++shown;
    ++row;
  }
  if (page >= pageCount(total)) return loadPage(pageCount(total) - 1);
  return true;
}
bool Shelf::loadOrder() {
  HalFile file;
  uint32_t header[4];
  if (!Storage.openFileForRead("DESKTOP", "/.crosspoint/desktop-order.bin", file) ||
      file.read(header, sizeof(header)) != sizeof(header) || header[0] != 0x44534f32 || header[1] != capacity ||
      header[2] > capacity || file.size() != sizeof(header) + header[2] * sizeof(Rank))
    return false;
  if (file.read(ranks, header[2] * sizeof(Rank)) != int(header[2] * sizeof(Rank))) return false;
  if (orderChecksum(ranks, header[2]) != header[3]) return false;
  total = header[2];
  for (uint16_t i = 0; i < total; ++i)
    if (ranks[i].ordinal >= capacity || (i && recentFirst(ranks[i], ranks[i - 1]))) return false;
  return true;
}
void Shelf::saveOrder() {
  const uint32_t header[] = {0x44534f32, capacity, total, orderChecksum(ranks, total)};
  {
    HalFile file;
    if (!Storage.openFileForWrite("DESKTOP", "/.crosspoint/desktop-order.tmp", file) ||
        file.write(reinterpret_cast<const uint8_t*>(header), sizeof(header)) != sizeof(header) ||
        file.write(reinterpret_cast<const uint8_t*>(ranks), total * sizeof(Rank)) != total * sizeof(Rank))
      return;
    file.flush();
  }
  Storage.remove("/.crosspoint/desktop-order.bin");
  Storage.rename("/.crosspoint/desktop-order.tmp", "/.crosspoint/desktop-order.bin");
}
void Shelf::removeMissing() { loadPage(page); }
}  // namespace desktop
