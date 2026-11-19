#pragma once
#include <rich_core.h>
#include <vector>
#include <string>

namespace rich::table {

struct Column {
    std::string header;
};

class Table {
public:
    Table(std::vector<Column> columns) : columns_(std::move(columns)) {}

    void add_row(std::vector<rich::AnyRenderable> row) {
        rows_.push_back(std::move(row));
    }

    std::vector<rich::Segment> render(const rich::Console& con, const rich::Options& opt) const {
        std::vector<rich::Segment> out;
        // Simple table rendering: header + rows
        for (const auto& col : columns_) {
            out.push_back({col.header + " | ", {}});
        }
        out.push_back({"\n", {}});
        for (const auto& row : rows_) {
            for (const auto& item : row) {
                for (const auto& seg : item.render(con, opt)) {
                    out.push_back(seg);
                }
                out.push_back({" | ", {}});
            }
            out.push_back({"\n", {}});
        }
        return out;
    }

    std::pair<size_t, size_t> measure(const rich::Console& con, const rich::Options& opt) const {
        return {20, rows_.size() + 1}; // Simplistic measure
    }

private:
    std::vector<Column> columns_;
    std::vector<std::vector<rich::AnyRenderable>> rows_;
};

inline std::vector<rich::Segment> render(const Table& t, const rich::Console& c, const rich::Options& o) { return t.render(c, o); }
inline std::pair<size_t, size_t> measure(const Table& t, const rich::Console& c, const rich::Options& o) { return t.measure(c, o); }

} // namespace rich::table
