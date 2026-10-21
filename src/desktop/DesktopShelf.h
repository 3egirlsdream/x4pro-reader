#pragma once
#include <HalMemory.h>
#include <LibraryIndexFile.h>

#include <array>
#include <atomic>

#include "DesktopModel.h"
#include "RecentBooksStore.h"

namespace desktop {
struct Book {
  std::string path, title, cover;
  Progress progress = Progress::Unread;
  int percent = -1;
};
class Shelf {
 public:
  ~Shelf();
  void begin();
  // Main-loop slices; never called while processing a touch contact.
  bool step(int coverHeight);
  void stop();
  bool loadPage(int page);
  void removeMissing();
  const std::array<Book, BOOKS_PER_PAGE>& visible() const { return books; }
  int visibleCount() const { return shown; }
  int count() const { return total; }
  bool loading() const { return needsBuild || scanning || building.load(); }
  bool failed() const { return error; }
  int currentPage() const { return page; }
  size_t rankBytes() const { return capacity * sizeof(Rank); }

 private:
  static void buildTask(void* context);
  static void coverTask(void* context);
  std::atomic<bool> coverBusy{false}, coverDone{false};
  std::array<bool, BOOKS_PER_PAGE> coverAttempted{};
  std::string coverJobPath, coverResult;
  int coverSlot = 0, jobHeight = 0;
  void openIndex();
  bool loadOrder();
  void saveOrder();
  library::LibraryIndexFile index;
  library::ClixRecord record{};
  HalMemory::PsramBuffer ranksMemory;
  Rank* ranks = nullptr;
  uint16_t capacity = 0, cursor = 0, total = 0;
  int page = 0, shown = 0;
  bool scanning = false, error = false, needsBuild = false;
  std::atomic<bool> building{false}, cancel{false}, buildDone{false}, buildOk{false};
  std::array<Book, BOOKS_PER_PAGE> books;
};
}  // namespace desktop
