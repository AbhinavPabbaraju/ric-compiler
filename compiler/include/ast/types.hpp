#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ric {

enum class TypeKind { I32, I64, Bool, Char, Unit, Function, Unknown };

class Type {
public:
    virtual ~Type() = default;

    [[nodiscard]] virtual auto kind()        const noexcept -> TypeKind   = 0;
    [[nodiscard]] virtual auto name()        const          -> std::string = 0;
    [[nodiscard]] virtual auto is_copy()     const noexcept -> bool        = 0;
    [[nodiscard]] virtual auto size_bytes()  const noexcept -> std::size_t = 0;
    [[nodiscard]] virtual auto equals(const Type& other) const noexcept -> bool = 0;

    [[nodiscard]] auto operator==(const Type& o) const noexcept -> bool { return equals(o); }
    [[nodiscard]] auto operator!=(const Type& o) const noexcept -> bool { return !equals(o); }

    [[nodiscard]] auto is_numeric()  const noexcept -> bool {
        return kind() == TypeKind::I32 || kind() == TypeKind::I64;
    }
    [[nodiscard]] auto is_integer()  const noexcept -> bool { return is_numeric(); }
    [[nodiscard]] auto is_bool()     const noexcept -> bool { return kind() == TypeKind::Bool; }
    [[nodiscard]] auto is_unit()     const noexcept -> bool { return kind() == TypeKind::Unit; }
    [[nodiscard]] auto is_unknown()  const noexcept -> bool { return kind() == TypeKind::Unknown; }
    [[nodiscard]] auto is_function() const noexcept -> bool { return kind() == TypeKind::Function; }
};

using TypePtr = std::shared_ptr<const Type>;

class I32Type final : public Type {
public:
    [[nodiscard]] auto kind()       const noexcept -> TypeKind   override { return TypeKind::I32; }
    [[nodiscard]] auto name()       const          -> std::string override { return "i32"; }
    [[nodiscard]] auto is_copy()    const noexcept -> bool        override { return true; }
    [[nodiscard]] auto size_bytes() const noexcept -> std::size_t override { return 4; }
    [[nodiscard]] auto equals(const Type& o) const noexcept -> bool override {
        return o.kind() == TypeKind::I32;
    }
    [[nodiscard]] static auto get() -> TypePtr;
};

class I64Type final : public Type {
public:
    [[nodiscard]] auto kind()       const noexcept -> TypeKind   override { return TypeKind::I64; }
    [[nodiscard]] auto name()       const          -> std::string override { return "i64"; }
    [[nodiscard]] auto is_copy()    const noexcept -> bool        override { return true; }
    [[nodiscard]] auto size_bytes() const noexcept -> std::size_t override { return 8; }
    [[nodiscard]] auto equals(const Type& o) const noexcept -> bool override {
        return o.kind() == TypeKind::I64;
    }
    [[nodiscard]] static auto get() -> TypePtr;
};

class BoolType final : public Type {
public:
    [[nodiscard]] auto kind()       const noexcept -> TypeKind   override { return TypeKind::Bool; }
    [[nodiscard]] auto name()       const          -> std::string override { return "bool"; }
    [[nodiscard]] auto is_copy()    const noexcept -> bool        override { return true; }
    [[nodiscard]] auto size_bytes() const noexcept -> std::size_t override { return 1; }
    [[nodiscard]] auto equals(const Type& o) const noexcept -> bool override {
        return o.kind() == TypeKind::Bool;
    }
    [[nodiscard]] static auto get() -> TypePtr;
};

class CharType final : public Type {
public:
    [[nodiscard]] auto kind()       const noexcept -> TypeKind   override { return TypeKind::Char; }
    [[nodiscard]] auto name()       const          -> std::string override { return "char"; }
    [[nodiscard]] auto is_copy()    const noexcept -> bool        override { return true; }
    [[nodiscard]] auto size_bytes() const noexcept -> std::size_t override { return 4; }
    [[nodiscard]] auto equals(const Type& o) const noexcept -> bool override {
        return o.kind() == TypeKind::Char;
    }
    [[nodiscard]] static auto get() -> TypePtr;
};

class UnitType final : public Type {
public:
    [[nodiscard]] auto kind()       const noexcept -> TypeKind   override { return TypeKind::Unit; }
    [[nodiscard]] auto name()       const          -> std::string override { return "()"; }
    [[nodiscard]] auto is_copy()    const noexcept -> bool        override { return true; }
    [[nodiscard]] auto size_bytes() const noexcept -> std::size_t override { return 0; }
    [[nodiscard]] auto equals(const Type& o) const noexcept -> bool override {
        return o.kind() == TypeKind::Unit;
    }
    [[nodiscard]] static auto get() -> TypePtr;
};

class FunctionType final : public Type {
public:
    FunctionType(std::vector<TypePtr> params, TypePtr ret)
        : params_(std::move(params)), return_type_(std::move(ret)) {}

    [[nodiscard]] auto kind()       const noexcept -> TypeKind   override { return TypeKind::Function; }
    [[nodiscard]] auto name()       const          -> std::string override;
    [[nodiscard]] auto is_copy()    const noexcept -> bool        override { return false; }
    [[nodiscard]] auto size_bytes() const noexcept -> std::size_t override { return 8; }
    [[nodiscard]] auto equals(const Type& o) const noexcept -> bool override;

    [[nodiscard]] auto param_types()  const noexcept -> const std::vector<TypePtr>& { return params_; }
    [[nodiscard]] auto return_type()  const noexcept -> const TypePtr&              { return return_type_; }

private:
    std::vector<TypePtr> params_;
    TypePtr              return_type_;
};

class UnknownType final : public Type {
public:
    [[nodiscard]] auto kind()       const noexcept -> TypeKind   override { return TypeKind::Unknown; }
    [[nodiscard]] auto name()       const          -> std::string override { return "<unknown>"; }
    [[nodiscard]] auto is_copy()    const noexcept -> bool        override { return false; }
    [[nodiscard]] auto size_bytes() const noexcept -> std::size_t override { return 0; }
    [[nodiscard]] auto equals(const Type& o) const noexcept -> bool override {
        return o.kind() == TypeKind::Unknown;
    }
    [[nodiscard]] static auto get() -> TypePtr;
};

namespace types {
[[nodiscard]] inline auto i32()     -> TypePtr { return I32Type::get(); }
[[nodiscard]] inline auto i64()     -> TypePtr { return I64Type::get(); }
[[nodiscard]] inline auto boolean() -> TypePtr { return BoolType::get(); }
[[nodiscard]] inline auto ch()      -> TypePtr { return CharType::get(); }
[[nodiscard]] inline auto unit()    -> TypePtr { return UnitType::get(); }
[[nodiscard]] inline auto unknown() -> TypePtr { return UnknownType::get(); }
[[nodiscard]] auto make_fn(std::vector<TypePtr> params, TypePtr ret) -> TypePtr;
[[nodiscard]] auto compatible(const TypePtr& a, const TypePtr& b) noexcept -> bool;
}

}
