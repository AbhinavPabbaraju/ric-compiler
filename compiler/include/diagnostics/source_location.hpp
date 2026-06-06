#pragma once

#include <cstdint>
#include <string>

namespace ric {

struct SourceLocation {
    std::string filename;
    std::uint32_t line{1};
    std::uint32_t column{1};
    std::uint32_t offset{0};

    [[nodiscard]] static auto invalid() noexcept -> SourceLocation {
        return {.filename = "<invalid>", .line = 0, .column = 0, .offset = 0};
    }

    [[nodiscard]] auto is_valid() const noexcept -> bool { return line > 0; }
};

struct SourceRange {
    SourceLocation begin;
    SourceLocation end;

    [[nodiscard]] static auto at(SourceLocation loc) noexcept -> SourceRange {
        return {.begin = loc, .end = loc};
    }

    [[nodiscard]] static auto spanning(SourceLocation b, SourceLocation e) noexcept -> SourceRange {
        return {.begin = b, .end = e};
    }
};

}
