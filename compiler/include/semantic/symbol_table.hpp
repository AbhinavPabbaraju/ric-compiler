#pragma once

#include "ast/types.hpp"
#include "diagnostics/source_location.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ric {

enum class SymbolKind { Variable, Parameter, Function };

struct Symbol {
    std::string    name;
    TypePtr        type;
    SymbolKind     kind;
    bool           is_mutable{false};
    bool           is_initialized{false};
    bool           is_moved{false};
    SourceLocation declaration_location;

    [[nodiscard]] auto is_variable()  const noexcept -> bool { return kind == SymbolKind::Variable || kind == SymbolKind::Parameter; }
    [[nodiscard]] auto is_function()  const noexcept -> bool { return kind == SymbolKind::Function; }
    [[nodiscard]] auto is_assignable()const noexcept -> bool { return is_variable() && is_mutable && !is_moved; }
    [[nodiscard]] auto is_usable()    const noexcept -> bool { return is_initialized && !is_moved; }
};

class SymbolTable {
public:
    struct Scope {
        std::unordered_map<std::string, Symbol> symbols;
        bool is_function_scope{false};
        bool is_loop_scope{false};
    };

    SymbolTable();

    void push_scope(bool is_function_scope = false, bool is_loop_scope = false);
    void pop_scope();

    [[nodiscard]] auto declare(Symbol sym) -> bool;
    [[nodiscard]] auto lookup(const std::string& name)               -> Symbol*;
    [[nodiscard]] auto lookup(const std::string& name)         const -> const Symbol*;
    [[nodiscard]] auto lookup_current_scope(const std::string& name) -> Symbol*;
    [[nodiscard]] auto in_loop_scope()     const noexcept -> bool;
    [[nodiscard]] auto in_function_scope() const noexcept -> bool;
    [[nodiscard]] auto scope_depth()       const noexcept -> std::size_t;

    void mark_moved(const std::string& name);

private:
    std::vector<Scope> scopes_;
};

}
