#pragma once
// Python rich.file_proxy.FileProxy + rich._null_file.NullFile.
//
// FileProxy wraps a stream and redirects writes to a Console: complete
// lines are ANSI-decoded (AnsiDecoder) and printed, partial lines stay in
// the buffer until the next newline or flush(). flush() prints the buffer
// as-is (Python prints the raw string — markup parsing included).
#include "rich_core.h"
#include "Ansi.h"
#include <ostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace rich {

class FileProxy {
public:
    FileProxy(Console& console, std::ostream& file)
        : console_(console), file_(file) {}

    // Python FileProxy.write: returns len() of the unprocessed tail
    // (the Python quirk — not the number of characters consumed).
    size_t write(std::string_view text_in) {
        std::vector<std::string> lines;
        size_t rest = 0;
        size_t p = 0;
        while (p < text_in.size()) {
            const auto nl = text_in.find('\n', p);
            if (nl == std::string::npos) {
                rest = text_in.size() - p;
                buffer_.append(text_in.substr(p));
                break;
            }
            // Python: lines.append("".join(buffer) + line); buffer.clear()
            lines.push_back(buffer_ + std::string(text_in.substr(p, nl - p)));
            buffer_.clear();
            p = nl + 1;
        }
        if (!lines.empty()) {
            // Python: Text("\n").join(decoder.decode_line(line) for line)
            std::string content;
            Style style;
            for (size_t i = 0; i < lines.size(); ++i) {
                const Text t = decoder_.decode_line(lines[i]);
                if (i != 0) content.push_back('\n');
                content += t.content;
                style = t.style;
            }
            console_.print(AnyRenderable(Text{std::move(content), std::move(style)}));
        }
        return rest;
    }

    // Python FileProxy.flush: print the buffer (no ANSI decoding).
    void flush() {
        if (!buffer_.empty()) {
            console_.print(std::string_view(buffer_));
            buffer_.clear();
        }
    }

    std::ostream& file() const { return file_; }

    // Python delegates fileno()/isatty() to the wrapped file; a C++ stream
    // carries no fd, so the caller may supply one.
    FileProxy& set_fd(int fd) {
        fd_ = fd;
        return *this;
    }
    int fileno() const { return fd_; }
    bool isatty() const {
        if (fd_ < 0) return false;
#ifdef _WIN32
        return _isatty(fd_) != 0;
#else
        return isatty(fd_) != 0;
#endif
    }

private:
    Console& console_;
    std::ostream& file_;
    int fd_{-1};
    std::string buffer_;
    AnsiDecoder decoder_;  // Python self.__ansi_decoder (stateful)
};

// Python rich._null_file.NullFile — an IO[str] that absorbs everything.
class NullFile {
public:
    bool isatty() const { return false; }
    std::string read(size_t = 1) const { return ""; }
    bool readable() const { return false; }
    std::string readline(int = 1) const { return ""; }
    std::vector<std::string> readlines(int = 1) const { return {}; }
    long seek(long, int = 1) const { return 0; }
    bool seekable() const { return false; }
    long tell() const { return 0; }
    long truncate(int = 1) const { return 0; }
    bool writable() const { return false; }
    void writelines(const std::vector<std::string>&) const {}
    int write(const std::string&) const { return 0; }
    void flush() const {}
    int fileno() const { return -1; }
};

} // namespace rich
