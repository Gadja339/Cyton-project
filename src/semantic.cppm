module;
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module cyton.semantic;

import cyton.token;
import cyton.ast;

// ── Система типов ─────────────────────────────────────────────────────────────

/// Базовый вид типа.
export enum class ValueType {
    Int64, Uint64, Float64, Bool, String, Unit, Struct, Unknown,
};

/// Полная информация о типе: вид + уточнение для составных типов.
export struct TypeInfo {
    ValueType   base = ValueType::Unknown;
    std::string name; ///< имя структуры, если base == Struct

    // фабричные методы
    static TypeInfo unknown()                  { return {}; }
    static TypeInfo unit()                     { return {ValueType::Unit,   {}}; }
    static TypeInfo of(ValueType v)            { return {v, {}}; }
    static TypeInfo struct_type(std::string n) { return {ValueType::Struct, std::move(n)}; }

    bool is_unknown() const { return base == ValueType::Unknown; }

    bool operator==(const TypeInfo& o) const {
        if (is_unknown() || o.is_unknown()) return true;
        if (base != o.base)                 return false;
        if (base == ValueType::Struct)      return name == o.name;
        return true;
    }
    bool operator!=(const TypeInfo& o) const { return !(*this == o); }
};

// ── Таблица символов ──────────────────────────────────────────────────────────

export struct Symbol {
    TypeInfo type_info;
    bool     is_mutable;
};

export struct FunctionSymbol {
    std::vector<TypeInfo> param_types;
    TypeInfo              return_type;
};

export struct StructFieldSymbol {
    std::string name;
    TypeInfo    type_info;
};

export struct StructSymbol {
    std::vector<StructFieldSymbol> fields;
};

// ── Семантический анализатор ──────────────────────────────────────────────────

export class SemanticAnalyzer {
public:
    /// filename используется в диагностических сообщениях.
    explicit SemanticAnalyzer(std::string filename = "");

    bool analyze(const Program& program);

private:
    std::string filename_;
    bool        had_error_ = false;

    // таблицы символов
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
    std::unordered_map<std::string, FunctionSymbol>      functions_;
    std::unordered_map<std::string, StructSymbol>        structs_;
    std::unordered_map<std::string, std::string>         type_aliases_;

    // контекст анализа
    std::vector<std::string> namespace_stack_;
    int      loop_depth_          = 0;
    bool     inside_function_     = false;
    TypeInfo current_return_type_ = TypeInfo::unit();

    // диагностика
    void error(Location loc, const std::string& message);

    // управление областями видимости
    void    begin_scope();
    void    end_scope();
    bool    declare_symbol(const std::string& name, Symbol symbol, Location loc);
    Symbol* lookup_symbol (const std::string& name);
    std::string qualified_name(const std::string& name) const;

    // проверка покрытия return во всех ветках
    bool returns_on_all_paths(const std::vector<std::unique_ptr<Instr>>& body) const;

    // первый проход: сбор деклараций
    void collect_type_alias(const TypeAliasDecl& decl);
    void collect_struct    (const StructDecl&    decl);
    void collect_function  (const FunctionDecl&  decl);
    void collect_namespace (const NamespaceDecl& decl);

    // второй проход: анализ тел
    void     analyze_top_item (const TopItem&       item);
    void     analyze_function (const FunctionDecl&  decl);
    void     analyze_namespace(const NamespaceDecl& decl);
    void     analyze_instr    (const Instr&         instr);
    TypeInfo analyze_expr     (const Expr&          expr);
    TypeInfo analyze_binary   (const BinaryExpr&    expr);
    TypeInfo analyze_unary    (const UnaryExpr&     expr);
    TypeInfo analyze_call     (const CallExpr&      expr);

    // l-value: определение типа и проверка мутабельности
    TypeInfo resolve_lvalue_type (const Expr& expr);
    bool     check_lvalue_mutable(const Expr& expr);

    // утилиты типов
    TypeInfo    type_from_string(const std::string& name) const;
    std::string type_to_string  (const TypeInfo& type)    const;
    static bool same_type(const TypeInfo& a, const TypeInfo& b);
};
