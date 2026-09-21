#include "qroute/common/file_io.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace qroute {

std::string read_text_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

std::vector<std::string> read_lines(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::vector<std::string> lines;
    // 1-based indexing: index 0 is a dummy empty string
    lines.push_back("");

    std::string line;
    while (std::getline(file, line)) {
        // Strip trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

bool write_text_file(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file.good();
}

std::vector<std::string> split_tokens(const std::string& s) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string strip_comment(const std::string& line) {
    size_t hash_pos = line.find('#');
    size_t slash_pos = line.find("//");
    size_t comment_pos = std::string::npos;

    if (hash_pos != std::string::npos && slash_pos != std::string::npos) {
        comment_pos = std::min(hash_pos, slash_pos);
    } else if (hash_pos != std::string::npos) {
        comment_pos = hash_pos;
    } else if (slash_pos != std::string::npos) {
        comment_pos = slash_pos;
    }

    if (comment_pos != std::string::npos) {
        return line.substr(0, comment_pos);
    }
    return line;
}

} // namespace qroute