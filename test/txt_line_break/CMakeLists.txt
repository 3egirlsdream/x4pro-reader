add_executable(TxtLineBreakTest TxtLineBreakTest.cpp ../../lib/Txt/CjkLineBreak.cpp ../../lib/Utf8/Utf8.cpp)
target_include_directories(TxtLineBreakTest PRIVATE ../../lib/Txt ../../lib/Utf8)
target_link_libraries(TxtLineBreakTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(TxtLineBreakTest)
