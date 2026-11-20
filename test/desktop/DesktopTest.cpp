#include <Epub.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <LibraryBuilder.h>
#include <gtest/gtest.h>

#include <set>

#include "activities/InputQueue.h"
#include "activities/reader/ProgressFile.h"
#include "desktop/DesktopShelf.h"
#include "desktop/ReadingHistory.h"
#include "util/BookProgress.h"

using namespace desktop;
namespace {
void makeLibrary(int n) {
  fake::reset();
  RECENT_BOOKS.books.clear();
  generatedCovers = 0;
  for (int i = 0; i < n; ++i) fake::add("/book" + std::to_string(i) + ".txt", std::string(1000, 'x'));
  library::BuildStats stats;
  ASSERT_TRUE(library::buildLibraryIndex("/", stats, false));
}
void ready(Shelf& shelf) {
  for (int i = 0; i < 2000 && shelf.loading(); ++i) shelf.step(0);
  ASSERT_FALSE(shelf.loading());
}
void addV4(const std::string& path, unsigned flags, unsigned page) {
  auto cache = cachePathFor(path);
  std::string bytes(30 + 12, '\0');
  auto put = [&](size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) bytes[at + i] = char(v >> (i * 8));
  };
  put(0, 0x54585449);
  bytes[4] = 4;
  put(5, 1000);
  put(26, flags);
  put(30, 0);
  put(34, 300);
  put(38, 700);
  fake::add(cache + "/index.bin", bytes);
  bytes.assign(4, '\0');
  bytes[0] = page;
  fake::add(cache + "/progress.bin", bytes);
}
}  // namespace
TEST(Desktop, WholeLibraryBeyondRecentCapAndMissingEntries) {
  makeLibrary(2000);
  rememberRead("/book1800.txt", 42, false);
  rememberRead("/book1600.txt", 100, true);
  fake::files.erase("/book30.txt");
  Shelf shelf;
  shelf.begin();
  ready(shelf);
  EXPECT_EQ(shelf.count(), 1999);
  EXPECT_EQ(shelf.visible()[0].path, "/book1600.txt");
  EXPECT_EQ(shelf.visible()[0].progress, Progress::Finished);
  EXPECT_EQ(shelf.visible()[1].percent, 42);
  std::set<std::string> paths;
  for (int p = 0; p < pageCount(shelf.count()); ++p) {
    shelf.loadPage(p);
    for (int i = 0; i < shelf.visibleCount(); ++i) paths.insert(shelf.visible()[i].path);
  }
  EXPECT_EQ(paths.size(), 1999);
  EXPECT_FALSE(paths.count("/book30.txt"));
  EXPECT_LE(shelf.rankBytes(), 4096 * sizeof(Rank));
}
TEST(Desktop, ReadingSnapshotPersistsBeyondTenAndRoundingIsNotCompletion) {
  makeLibrary(24);
  for (int i = 0; i < 24; ++i) rememberRead("/book" + std::to_string(i) + ".txt", 100, false);
  Shelf shelf;
  shelf.begin();
  ready(shelf);
  EXPECT_EQ(shelf.visible()[0].path, "/book23.txt");
  EXPECT_EQ(shelf.visible()[0].percent, 99);
  EXPECT_EQ(shelf.visible()[0].progress, Progress::Reading);
  shelf.loadPage(3);
  EXPECT_EQ(shelf.visible()[5].path, "/book0.txt");
}
TEST(Desktop, CacheReentryDoesNotEnumerateStorageOrDecodeCovers) {
  makeLibrary(500);
  {
    Shelf s;
    s.begin();
    ready(s);
  }
  fake::directoryEntriesByPath.clear();
  fake::resetIoCounters();
  {
    Shelf s;
    s.begin();
    EXPECT_FALSE(s.loading());
    EXPECT_EQ(s.count(), 500);
    EXPECT_LE(fake::reads, 3u + 8u * BOOKS_PER_PAGE);
    std::cout << "cache_reentry_reads=" << fake::reads << " rank_bytes=" << s.rankBytes() << "\n";
  }
  EXPECT_TRUE(fake::directoryEntriesByPath.empty());
  EXPECT_EQ(generatedCovers, 0);
}
TEST(Desktop, EmptyLibraryAndRepeatedLifetimesReleasePsram) {
  makeLibrary(0);
  for (int i = 0; i < 100; ++i) {
    Shelf s;
    s.begin();
    ready(s);
    EXPECT_EQ(s.count(), 0);
  }
  EXPECT_EQ(livePsram, 0);
}
TEST(Desktop, CorruptHistoryDoesNotInventProgressAndBackupRecovers) {
  makeLibrary(1);
  rememberRead("/book0.txt", 40, false);
  auto good = readStamp("/book0.txt");
  ASSERT_GT(good.sequence, 0u);
  for (auto& [path, node] : fake::files)
    if (path.starts_with("/.crosspoint/desktop/") && path.find("sequence") == std::string::npos &&
        path.ends_with(".bin")) {
      fake::files[path + ".bak"] = std::make_shared<fake::Node>(*node);
      node->bytes[10] ^= 0x20;
      break;
    }
  EXPECT_EQ(readStamp("/book0.txt").percent, 40);
}
TEST(Desktop, TxtV4PrefixProgressAndFinishedSentinel) {
  makeLibrary(1);
  addV4("/book0.txt", 0, 2);
  EXPECT_EQ(loadBookProgress("/book0.txt"), 70);
  addV4("/book0.txt", 0, 3);
  EXPECT_EQ(loadBookProgress("/book0.txt"), -1);
  addV4("/book0.txt", 0x80000000, 3);
  EXPECT_EQ(loadBookProgress("/book0.txt"), 100);
  addV4("/book0.txt", 0, 0);
  EXPECT_EQ(loadBookProgress("/book0.txt"), 0);
}
TEST(Desktop, UnreadUnknownAndReadAreDistinct) {
  makeLibrary(3);
  fake::add(cachePathFor("/book1.txt") + "/progress.bin", "bad");
  Shelf s;
  s.begin();
  ready(s);
  EXPECT_EQ(s.visible()[0].path, "/book1.txt");
  EXPECT_EQ(s.visible()[0].progress, Progress::Unknown);
  EXPECT_EQ(s.visible()[1].progress, Progress::Unread);
}
TEST(Desktop, VisibleCoverOnlyAndOneWorkerAtATime) {
  makeLibrary(0);
  fake::add("/one.epub");
  fake::add("/two.epub");
  library::BuildStats stats;
  ASSERT_TRUE(library::buildLibraryIndex("/", stats));
  Shelf s;
  s.begin();
  ready(s);
  EXPECT_EQ(generatedCovers, 0);
  s.step(144);
  ASSERT_NE(queuedTask, nullptr);
  EXPECT_EQ(generatedCovers, 0);
  runQueuedTask();
  EXPECT_EQ(generatedCovers, 1);
  EXPECT_TRUE(s.step(144));
  EXPECT_FALSE(s.visible()[0].cover.empty());
}
TEST(Desktop, BuilderCancellationPreservesExistingIndex) {
  makeLibrary(200);
  const auto old = fake::files[library::libraryIndexPath()]->bytes;
  int remaining = 20;
  library::BuildControl control{&remaining, [](void* v) { return --*static_cast<int*>(v) > 0; }};
  library::BuildStats stats;
  EXPECT_FALSE(library::buildLibraryIndex("/", stats, false, &control));
  EXPECT_EQ(fake::files[library::libraryIndexPath()]->bytes, old);
  EXPECT_TRUE(library::buildLibraryIndex("/", stats, false));
}
TEST(Desktop, PortraitLayoutTargetsAndTwoRowsDoNotOverlap) {
  for (int width : {480, 800})
    for (int bottom : {8, 38}) {
      Layout l(width, 800, 50, bottom, 16);
      for (auto b : l.books) {
        EXPECT_GE(b.width, 44);
        EXPECT_GE(b.height, 44);
        EXPECT_LT(b.y + b.height, l.appHeading.y);
      }
      for (int i = 0; i < APP_CAPACITY; ++i) {
        auto b = l.apps[i];
        EXPECT_GE(b.width, 44);
        EXPECT_GE(b.height, 44);
        EXPECT_LE(b.y + b.height, 800 - bottom);
        EXPECT_LT(b.x + b.width, width);
      }
    }
  for (int n = 0; n < 100; ++n) EXPECT_LE(homeAppCount(n, true), 8);
}

TEST(Desktop, CorruptOrderCacheRebuildsWithoutDroppingBooks) {
  makeLibrary(50);
  {
    Shelf s;
    s.begin();
    ready(s);
  }
  auto& bytes = fake::files["/.crosspoint/desktop-order.bin"]->bytes;
  bytes.back() ^= 0x10;
  Shelf s;
  s.begin();
  EXPECT_TRUE(s.loading());
  ready(s);
  EXPECT_EQ(s.count(), 50);
}
TEST(Desktop, AllocationFailureAndStaleDeletedBookAreRecoverable) {
  makeLibrary(10);
  fake::failAlloc = 0;
  {
    Shelf s;
    s.begin();
    EXPECT_TRUE(s.failed());
  }
  Shelf s;
  s.begin();
  ready(s);
  const auto path = s.visible()[0].path;
  fake::files.erase(path);
  s.removeMissing();
  EXPECT_EQ(s.count(), 9);
  for (int i = 0; i < s.visibleCount(); ++i) EXPECT_NE(s.visible()[i].path, path);
}

TEST(Desktop, MissingIndexPublishesLoadingBeforeAnyDirectoryWalk) {
  fake::reset();
  RECENT_BOOKS.books.clear();
  fake::add("/new.txt", "content");
  Shelf s;
  s.begin();
  EXPECT_TRUE(s.loading());
  EXPECT_TRUE(fake::directoryEntriesByPath.empty());
  s.step(0);
  ASSERT_NE(queuedTask, nullptr);
  EXPECT_TRUE(fake::directoryEntriesByPath.empty());
  runQueuedTask();
  s.step(0);
  ready(s);
  EXPECT_EQ(s.count(), 1);
}

TEST(Desktop, FormatCapacityAndDeletingLastPageAreBounded) {
  makeLibrary(4096);
  {
    Shelf s;
    s.begin();
    ready(s);
    EXPECT_EQ(s.count(), 4096);
    EXPECT_EQ(s.rankBytes(), 65536u);
    s.loadPage(pageCount(s.count()) - 1);
    EXPECT_EQ(s.visibleCount(), 4);
  }
  makeLibrary(7);
  Shelf s;
  s.begin();
  ready(s);
  s.loadPage(1);
  fake::files.erase(s.visible()[0].path);
  s.removeMissing();
  EXPECT_EQ(s.currentPage(), 0);
  EXPECT_EQ(s.visibleCount(), 6);
}

TEST(Desktop, ProgressRotationPreservesCommittedOrReadyData) {
  const std::string cache = "/cache";
  for (int fail = 0; fail < 3; ++fail) {
    fake::reset();
    uint8_t record[4]{1};
    ASSERT_TRUE(ProgressFile::writeAtomic(cache, record, sizeof(record)));
    record[0] = 2;
    fake::failRename = fail;
    EXPECT_FALSE(ProgressFile::writeAtomic(cache, record, sizeof(record)));
    HalFile file;
    ASSERT_TRUE(ProgressFile::open(cache, ProgressFile::Format::Xtc, file));
    ASSERT_EQ(file.read(record, 4), 4);
    EXPECT_EQ(record[0], fail == 2 ? 2 : 1);
    file.close();
    record[0] = 3;
    ASSERT_TRUE(ProgressFile::writeAtomic(cache, record, sizeof(record)));
  }
}
TEST(Desktop, DelayedInputKeepsFirstCommandAndOrder) {
  InputQueue<int, 2> queue;
  queue.push(1);
  queue.push(2);
  queue.push(3);
  EXPECT_EQ(queue.pop(), 1);
  queue.push(4);
  EXPECT_EQ(queue.pop(), 2);
  EXPECT_EQ(queue.pop(), 4);
  EXPECT_TRUE(queue.empty());
}
TEST(Desktop, ClosingHomeDoesNotWaitForQueuedCoverOrRetainActivity) {
  makeLibrary(0);
  fake::add("/one.epub");
  library::BuildStats stats;
  ASSERT_TRUE(library::buildLibraryIndex("/", stats));
  {
    Shelf shelf;
    shelf.begin();
    ready(shelf);
    shelf.step(144);
    ASSERT_NE(queuedTask, nullptr);
  }
  runQueuedTask();
  EXPECT_EQ(generatedCovers, 0);
}
