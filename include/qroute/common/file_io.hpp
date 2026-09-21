#pragma once

#include <string>
#include <vector>

namespace qroute {

std::string read_text_file(const std::string& path); // throws std::runtime_error
inline std::string readtextfile(const std::string& path) { return read_text_file(path); }

std::vector<std::string> read_lines(const std::string& path); // strips \r, keeps line numbers 1-based

bool write_text_file(const std::string& path, const std::string& content);
inline bool writetextfile(const std::string& path, const std::string& content) {
    return write_text_file(path, content);
}

std::vector<std::string> split_tokens(const std::string& s); // whitespace split

std::string strip_comment(const std::string& line); // removes '#' and '//' comments

} // namespace qroute