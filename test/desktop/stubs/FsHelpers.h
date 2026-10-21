#pragma once
#include <string>
namespace FsHelpers {
inline bool checkFileExtension(const std::string& p, const char* ext) { return p.ends_with(ext); }
inline bool hasEpubExtension(const std::string& s) { return s.ends_with(".epub"); }
inline bool hasXtcExtension(const std::string& s) { return s.ends_with(".xtc") || s.ends_with(".xtch"); }
inline bool hasTxtExtension(const std::string& s) { return s.ends_with(".txt"); }
inline bool hasMarkdownExtension(const std::string& s) { return s.ends_with(".md"); }
}  // namespace FsHelpers
