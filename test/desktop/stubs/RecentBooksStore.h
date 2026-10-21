#pragma once
#include <string>
#include <vector>
struct RecentBook {
  std::string path, title, author, coverBmpPath;
};
struct RecentBooksStore {
  std::vector<RecentBook> books;
  const auto& getBooks() const { return books; }
};
inline RecentBooksStore recentTest;
#define RECENT_BOOKS recentTest
