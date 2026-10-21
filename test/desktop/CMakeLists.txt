add_executable(DesktopTest DesktopTest.cpp
  ${REPO_ROOT}/src/desktop/DesktopShelf.cpp ${REPO_ROOT}/src/desktop/ReadingHistory.cpp
  ${REPO_ROOT}/src/util/BookProgress.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryBuilder.cpp ${REPO_ROOT}/lib/LibraryIndex/LibraryIndexFile.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryFormat.cpp ${REPO_ROOT}/lib/LibraryIndex/LibraryText.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp)
target_include_directories(DesktopTest PRIVATE stubs ../library_builder/stubs
 ${REPO_ROOT}/src ${REPO_ROOT}/lib/Performance ${REPO_ROOT}/lib/LibraryIndex ${REPO_ROOT}/lib/Serialization ${REPO_ROOT}/lib/Utf8)
target_compile_options(DesktopTest PRIVATE -fsanitize=undefined)
target_link_options(DesktopTest PRIVATE -fsanitize=undefined)
target_link_libraries(DesktopTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(DesktopTest)
