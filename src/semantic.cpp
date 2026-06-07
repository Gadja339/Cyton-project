module;
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

module cyton.semantic;

static bool is_integer(ValueType v) {
    return v == ValueType::Int64 || v == ValueType::Uint64;
}

// ── Конструктор ───────────────────────────────────────────────────────────────

SemanticAnalyzer::SemanticAnalyzer(std::string filename)
    : filename_(std::move(filename)) {}

// ── Точка входа ───────────────────────────────────────────────────────────────

bool SemanticAnalyzer::analyze(const Program& program) {
    // Сброс состояния
    had_error_            = false;
    scopes_.clear();
    functions_.clear();
    structs_.clear();
    type_aliases_.clear();
    namespace_stack_.clear();
    loop_depth_           = 0;
    inside_function_      = false;
    current_return_type_  = TypeInfo::unit();

    begin_scope();

    // Встроенные функции
    // print принимает аргумент любого типа (Unknown = wildcard)
    functions_["print"]  = FunctionSymbol{{TypeInfo::unknown()},              TypeInfo::unit()};
    functions_["input"]  = FunctionSymbol{{},                                 TypeInfo::of(ValueType::String)};
    functions_["exit"]   = FunctionSymbol{{TypeInfo::of(ValueType::Int64)},  TypeInfo::unit()};
    functions_["panic"]  = FunctionSymbol{{TypeInfo::of(ValueType::String)}, TypeInfo::unit()};
    functions_["assert"] = FunctionSymbol{{TypeInfo::of(ValueType::Bool)},   TypeInfo::unit()};

    // Проход 1: type aliases — должны быть известны до struct fields
    for (const auto& item : program.items)
        if (item.kind == TopItem::Kind::TypeAlias)
            collect_type_alias(item.type_alias);

    // Проход 2а: регистрируем имена структур (без полей), чтобы поля
    //            могли ссылаться на другие структуры в любом порядке
    for (const auto& item : program.items)
        if (item.kind == TopItem::Kind::Struct) {
            const auto& decl = item.structure;
            if (structs_.count(decl.name))
                error(decl.loc, "struct '" + decl.name + "' is already declared");
            else
                structs_[decl.name] = StructSymbol{};
        }

    // Проход 2б: заполняем поля структур
    for (const auto& item : program.items)
        if (item.kind == TopItem::Kind::Struct)
            collect_struct(item.structure);

    // Проход 3: сигнатуры функций и пространств имён
    for (const auto& item : program.items) {
        if      (item.kind == TopItem::Kind::Function)  collect_function(item.function);
        else if (item.kind == TopItem::Kind::Namespace) collect_namespace(item.ns);
    }

    // Проход 4: анализ тел
    for (const auto& item : program.items)
        analyze_top_item(item);

    end_scope();
    return !had_error_;
}

// ── Диагностика ───────────────────────────────────────────────────────────────

void SemanticAnalyzer::error(Location loc, const std::string& message) {
    // Формат: <файл>:<строка>:<столбец>: error: <сообщение>
    if (!filename_.empty()) std::cerr << filename_ << ":";
    std::cerr << loc.line << ":" << loc.col << ": error: " << message << "\n";
    had_error_ = true;
}

// ── Области видимости ─────────────────────────────────────────────────────────

void SemanticAnalyzer::begin_scope() { scopes_.emplace_back(); }
void SemanticAnalyzer::end_scope()   { scopes_.pop_back(); }

bool SemanticAnalyzer::declare_symbol(const std::string& name, Symbol symbol,
                                       Location loc) {
    auto& cur = scopes_.back();
    if (cur.count(name)) {
        error(loc, "name '" + name + "' is already declared in this scope");
        return false;
    }
    cur[name] = std::move(symbol);
    return true;
}

Symbol* SemanticAnalyzer::lookup_symbol(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

std::string SemanticAnalyzer::qualified_name(const std::string& name) const {
    if (namespace_stack_.empty()) return name;
    std::string result;
    for (const auto& ns : namespace_stack_) {
        if (!result.empty()) result += "::";
        result += ns;
    }
    return result + "::" + name;
}

// ── Проверка покрытия return ──────────────────────────────────────────────────

/// Возвращает true, если каждый путь выполнения в body заканчивается return.
bool SemanticAnalyzer::returns_on_all_paths(
        const std::vector<std::unique_ptr<Instr>>& body) const {
    for (const auto& instr : body) {
        switch (instr->type) {
            case InstrType::Return:
                return true;
            case InstrType::If: {
                const auto& n = static_cast<const IfInstr&>(*instr);
                // if+else, где обе ветки возвращают → возврат гарантирован
                if (!n.else_branch.empty() &&
                    returns_on_all_paths(n.then_branch) &&
                    returns_on_all_paths(n.else_branch))
                    return true;
                break;
            }
            case InstrType::Block: {
                const auto& n = static_cast<const BlockInstr&>(*instr);
                if (returns_on_all_paths(n.body)) return true;
                break;
            }
            default: break;
        }
    }
    return false;
}

// ── Первый проход: сбор деклараций ───────────────────────────────────────────

void SemanticAnalyzer::collect_type_alias(const TypeAliasDecl& decl) {
    if (type_aliases_.count(decl.name)) {
        error(decl.loc, "type alias '" + decl.name + "' is already declared");
        return;
    }
    type_aliases_[decl.name] = decl.target;
}

void SemanticAnalyzer::collect_struct(const StructDecl& decl) {
    // Имя уже зарегистрировано в проходе 2а; заполняем поля
    auto it = structs_.find(decl.name);
    if (it == structs_.end()) return; // была ошибка дубликата — пропускаем
    if (!it->second.fields.empty())   return; // уже заполнено

    for (const auto& field : decl.fields) {
        TypeInfo ft = type_from_string(field.type_name);
        if (ft.is_unknown())
            error(decl.loc, "struct '" + decl.name
                  + "': unknown type '" + field.type_name
                  + "' for field '" + field.name + "'");
        it->second.fields.push_back({field.name, ft});
    }
}

void SemanticAnalyzer::collect_function(const FunctionDecl& decl) {
    std::string qname = qualified_name(decl.name);
    if (functions_.count(qname)) {
        error(decl.loc, "function '" + qname + "' is already declared");
        return;
    }
    std::vector<TypeInfo> param_types;
    for (const auto& p : decl.params)
        param_types.push_back(type_from_string(p.type_name));
    TypeInfo ret = decl.return_type.empty() ? TypeInfo::unit()
                                            : type_from_string(decl.return_type);
    functions_[qname] = FunctionSymbol{std::move(param_types), ret};
}

void SemanticAnalyzer::collect_namespace(const NamespaceDecl& decl) {
    namespace_stack_.push_back(decl.name);
    for (const auto& fn : decl.functions)
        collect_function(fn);
    namespace_stack_.pop_back();
}

// ── Верхний уровень ───────────────────────────────────────────────────────────

void SemanticAnalyzer::analyze_top_item(const TopItem& item) {
    switch (item.kind) {
        case TopItem::Kind::Instr:     analyze_instr(*item.instr);      break;
        case TopItem::Kind::Function:  analyze_function(item.function);  break;
        case TopItem::Kind::Namespace: analyze_namespace(item.ns);       break;
        case TopItem::Kind::Struct:
        case TopItem::Kind::TypeAlias: break; // уже обработаны в первых проходах
    }
}

void SemanticAnalyzer::analyze_function(const FunctionDecl& decl) {
    bool     prev_inside  = inside_function_;
    TypeInfo prev_ret     = current_return_type_;

    inside_function_     = true;
    current_return_type_ = decl.return_type.empty()
                               ? TypeInfo::unit()
                               : type_from_string(decl.return_type);

    begin_scope();
    for (const auto& p : decl.params)
        declare_symbol(p.name, Symbol{type_from_string(p.type_name), false}, decl.loc);
    for (const auto& instr : decl.body)
        analyze_instr(*instr);
    end_scope();

    // Проверяем, что все пути возвращают значение (только для не-unit функций)
    if (current_return_type_.base != ValueType::Unit &&
        !returns_on_all_paths(decl.body)) {
        error(decl.loc, "function '" + decl.name
              + "' does not return a value on all execution paths");
    }

    inside_function_     = prev_inside;
    current_return_type_ = prev_ret;
}

void SemanticAnalyzer::analyze_namespace(const NamespaceDecl& decl) {
    namespace_stack_.push_back(decl.name);
    for (const auto& fn : decl.functions)
        analyze_function(fn);
    namespace_stack_.pop_back();
}

// ── Инструкции ────────────────────────────────────────────────────────────────

void SemanticAnalyzer::analyze_instr(const Instr& instr) {
    switch (instr.type) {

        case InstrType::Let: {
            const auto& n = static_cast<const LetInstr&>(instr);
            TypeInfo init_type  = analyze_expr(*n.init);
            TypeInfo final_type = init_type;
            if (!n.explicit_type.empty()) {
                TypeInfo declared = type_from_string(n.explicit_type);
                if (declared.is_unknown())
                    error(n.loc, "unknown type '" + n.explicit_type + "'");
                else if (!init_type.is_unknown() && !same_type(declared, init_type))
                    error(n.loc, "cannot initialize '" + n.name + "': expected "
                                 + type_to_string(declared)
                                 + ", got " + type_to_string(init_type));
                final_type = declared;
            }
            declare_symbol(n.name, Symbol{final_type, /*mutable=*/false}, n.loc);
            break;
        }

        case InstrType::Var: {
            const auto& n = static_cast<const VarInstr&>(instr);
            TypeInfo init_type  = analyze_expr(*n.init);
            TypeInfo final_type = init_type;
            if (!n.explicit_type.empty()) {
                TypeInfo declared = type_from_string(n.explicit_type);
                if (declared.is_unknown())
                    error(n.loc, "unknown type '" + n.explicit_type + "'");
                else if (!init_type.is_unknown() && !same_type(declared, init_type))
                    error(n.loc, "cannot initialize '" + n.name + "': expected "
                                 + type_to_string(declared)
                                 + ", got " + type_to_string(init_type));
                final_type = declared;
            }
            declare_symbol(n.name, Symbol{final_type, /*mutable=*/true}, n.loc);
            break;
        }

        case InstrType::Expr:
            analyze_expr(*static_cast<const ExprInstr&>(instr).expr);
            break;

        case InstrType::Assign: {
            const auto& n   = static_cast<const AssignInstr&>(instr);
            TypeInfo tgt    = resolve_lvalue_type(*n.target);
            TypeInfo val    = analyze_expr(*n.value);
            check_lvalue_mutable(*n.target);          // сообщает об ошибке внутри
            if (!tgt.is_unknown() && !val.is_unknown() && !same_type(tgt, val))
                error(n.loc, "assignment type mismatch: expected "
                             + type_to_string(tgt)
                             + ", got " + type_to_string(val));
            break;
        }

        case InstrType::If: {
            const auto& n = static_cast<const IfInstr&>(instr);
            TypeInfo cond = analyze_expr(*n.condition);
            if (!cond.is_unknown() && cond.base != ValueType::Bool)
                error(n.condition->loc,
                      "if condition must be bool, got " + type_to_string(cond));
            begin_scope();
            for (const auto& i : n.then_branch) analyze_instr(*i);
            end_scope();
            begin_scope();
            for (const auto& i : n.else_branch) analyze_instr(*i);
            end_scope();
            break;
        }

        case InstrType::While: {
            const auto& n = static_cast<const WhileInstr&>(instr);
            TypeInfo cond = analyze_expr(*n.condition);
            if (!cond.is_unknown() && cond.base != ValueType::Bool)
                error(n.condition->loc,
                      "while condition must be bool, got " + type_to_string(cond));
            ++loop_depth_;
            begin_scope();
            for (const auto& i : n.body) analyze_instr(*i);
            end_scope();
            --loop_depth_;
            break;
        }

        case InstrType::Return: {
            const auto& n = static_cast<const ReturnInstr&>(instr);
            if (!inside_function_) {
                error(n.loc, "return is not allowed outside a function");
                break;
            }
            TypeInfo ret = n.value ? analyze_expr(*n.value) : TypeInfo::unit();
            if (!ret.is_unknown() && !same_type(current_return_type_, ret))
                error(n.loc, "return type mismatch: expected "
                             + type_to_string(current_return_type_)
                             + ", got " + type_to_string(ret));
            break;
        }

        case InstrType::Break:
            if (loop_depth_ == 0)
                error(instr.loc, "break is not allowed outside a loop");
            break;

        case InstrType::Continue:
            if (loop_depth_ == 0)
                error(instr.loc, "continue is not allowed outside a loop");
            break;

        case InstrType::Block: {
            const auto& n = static_cast<const BlockInstr&>(instr);
            begin_scope();
            for (const auto& i : n.body) analyze_instr(*i);
            end_scope();
            break;
        }
    }
}

// ── Выражения ─────────────────────────────────────────────────────────────────

TypeInfo SemanticAnalyzer::analyze_expr(const Expr& expr) {
    switch (expr.type) {

        case ExprType::NumLit:    return TypeInfo::of(ValueType::Int64);
        case ExprType::FloatLit:  return TypeInfo::of(ValueType::Float64);
        case ExprType::StringLit: return TypeInfo::of(ValueType::String);
        case ExprType::BoolLit:   return TypeInfo::of(ValueType::Bool);
        case ExprType::UnitLit:   return TypeInfo::unit();

        case ExprType::Identif: {
            const auto& n = static_cast<const IdentifExpr&>(expr);
            Symbol* sym = lookup_symbol(n.name);
            if (!sym) {
                error(n.loc, "use of undeclared variable '" + n.name + "'");
                return TypeInfo::unknown();
            }
            return sym->type_info;
        }

        case ExprType::Binary:
            return analyze_binary(static_cast<const BinaryExpr&>(expr));

        case ExprType::Unary:
            return analyze_unary(static_cast<const UnaryExpr&>(expr));

        case ExprType::Call:
            return analyze_call(static_cast<const CallExpr&>(expr));

        case ExprType::Index: {
            const auto& n = static_cast<const IndexExpr&>(expr);
            analyze_expr(*n.object);
            TypeInfo idx = analyze_expr(*n.index);
            if (!idx.is_unknown() && !is_integer(idx.base))
                error(n.index->loc,
                      "array index must be int64, got " + type_to_string(idx));
            return TypeInfo::unknown(); // тип элемента без параметрических типов неизвестен
        }

        case ExprType::Field: {
            const auto& n = static_cast<const FieldExpr&>(expr);
            TypeInfo obj = analyze_expr(*n.object);
            if (obj.base == ValueType::Struct) {
                auto it = structs_.find(obj.name);
                if (it != structs_.end()) {
                    for (const auto& f : it->second.fields)
                        if (f.name == n.field) return f.type_info;
                    error(n.loc, "struct '" + obj.name
                          + "' has no field '" + n.field + "'");
                    return TypeInfo::unknown();
                }
            }
            if (!obj.is_unknown())
                error(n.loc, "field access on non-struct type " + type_to_string(obj));
            return TypeInfo::unknown();
        }

        case ExprType::NamespaceAccess:
            // Namespace-qualified ссылки используются только внутри CallExpr
            // (flatten_callee в парсере), здесь не должны появляться.
            return TypeInfo::unknown();

        case ExprType::Cast: {
            const auto& n = static_cast<const CastExpr&>(expr);
            analyze_expr(*n.expr);
            TypeInfo target = type_from_string(n.target_type);
            if (target.is_unknown())
                error(n.loc, "unknown cast target type '" + n.target_type + "'");
            return target;
        }

        case ExprType::ArrayLit: {
            const auto& n = static_cast<const ArrayLitExpr&>(expr);
            if (n.elements.empty()) return TypeInfo::unknown();
            TypeInfo first = analyze_expr(*n.elements[0]);
            for (std::size_t i = 1; i < n.elements.size(); ++i) {
                TypeInfo cur = analyze_expr(*n.elements[i]);
                if (!first.is_unknown() && !cur.is_unknown() && !same_type(first, cur))
                    error(n.elements[i]->loc,
                          "array elements must have the same type, got "
                          + type_to_string(first) + " and " + type_to_string(cur));
            }
            return TypeInfo::unknown(); // без параметрических типов не несёт информации
        }

        case ExprType::StructLit: {
            const auto& n = static_cast<const StructLitExpr&>(expr);

            // Разрешаем type alias
            std::string struct_name = n.type_name;
            {
                auto alias = type_aliases_.find(struct_name);
                if (alias != type_aliases_.end()) struct_name = alias->second;
            }

            auto it = structs_.find(struct_name);
            if (it == structs_.end()) {
                error(n.loc, "unknown struct type '" + n.type_name + "'");
                for (const auto& f : n.fields) analyze_expr(*f.value);
                return TypeInfo::unknown();
            }
            const StructSymbol& s = it->second;

            if (n.fields.size() != s.fields.size()) {
                error(n.loc, "struct '" + struct_name + "' has "
                      + std::to_string(s.fields.size())
                      + " field(s), but "
                      + std::to_string(n.fields.size()) + " were provided");
            } else {
                for (const auto& init : n.fields) {
                    // Ищем поле по имени
                    const StructFieldSymbol* field = nullptr;
                    for (const auto& f : s.fields)
                        if (f.name == init.name) { field = &f; break; }
                    if (!field) {
                        error(n.loc, "struct '" + struct_name
                              + "' has no field '" + init.name + "'");
                        analyze_expr(*init.value);
                        continue;
                    }
                    TypeInfo val = analyze_expr(*init.value);
                    if (!val.is_unknown() && !same_type(field->type_info, val))
                        error(init.value->loc,
                              "field '" + init.name + "' expects "
                              + type_to_string(field->type_info)
                              + ", got " + type_to_string(val));
                }
            }
            return TypeInfo::struct_type(struct_name);
        }
    }
    return TypeInfo::unknown();
}

TypeInfo SemanticAnalyzer::analyze_binary(const BinaryExpr& expr) {
    TypeInfo lhs = analyze_expr(*expr.left);
    TypeInfo rhs = analyze_expr(*expr.right);

    // Если один из операндов Unknown — возвращаем разумный тип без ошибки
    if (lhs.is_unknown() || rhs.is_unknown()) {
        switch (expr.op) {
            case TokenKind::EqualEqual: case TokenKind::BangEqual:
            case TokenKind::Less:       case TokenKind::LessEqual:
            case TokenKind::Greater:    case TokenKind::GreaterEqual:
            case TokenKind::KwAnd:      case TokenKind::KwOr:
                return TypeInfo::of(ValueType::Bool);
            default:
                return TypeInfo::unknown();
        }
    }

    switch (expr.op) {
        // Арифметика
        case TokenKind::Plus:
            if (lhs.base == ValueType::String && rhs.base == ValueType::String)
                return TypeInfo::of(ValueType::String);
            [[fallthrough]];
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
        case TokenKind::Percent:
            if (is_integer(lhs.base) && is_integer(rhs.base))
                return TypeInfo::of(lhs.base);
            if (expr.op != TokenKind::Percent &&
                lhs.base == ValueType::Float64 && rhs.base == ValueType::Float64)
                return TypeInfo::of(ValueType::Float64);
            error(expr.loc, "arithmetic operator requires numeric operands of the same type"
                            ", got " + type_to_string(lhs) + " and " + type_to_string(rhs));
            return TypeInfo::unknown();

        // Равенство
        case TokenKind::EqualEqual:
        case TokenKind::BangEqual:
            if (!same_type(lhs, rhs))
                error(expr.loc,
                      "equality comparison requires operands of the same type, got "
                      + type_to_string(lhs) + " and " + type_to_string(rhs));
            return TypeInfo::of(ValueType::Bool);

        // Порядковые сравнения
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
            if ((is_integer(lhs.base) && is_integer(rhs.base)) ||
                (lhs.base == ValueType::Float64 && rhs.base == ValueType::Float64))
                return TypeInfo::of(ValueType::Bool);
            error(expr.loc, "comparison requires numeric operands of the same type, got "
                            + type_to_string(lhs) + " and " + type_to_string(rhs));
            return TypeInfo::unknown();

        // Логика
        case TokenKind::KwAnd:
        case TokenKind::KwOr:
            if (lhs.base != ValueType::Bool || rhs.base != ValueType::Bool) {
                error(expr.loc, "logical operator requires bool operands, got "
                                + type_to_string(lhs) + " and " + type_to_string(rhs));
                return TypeInfo::unknown();
            }
            return TypeInfo::of(ValueType::Bool);

        default:
            error(expr.loc, "unsupported binary operator");
            return TypeInfo::unknown();
    }
}

TypeInfo SemanticAnalyzer::analyze_unary(const UnaryExpr& expr) {
    TypeInfo operand = analyze_expr(*expr.operand);
    switch (expr.op) {
        case TokenKind::Minus:
            if (operand.is_unknown())                return TypeInfo::unknown();
            if (is_integer(operand.base))            return TypeInfo::of(operand.base);
            if (operand.base == ValueType::Float64)  return TypeInfo::of(ValueType::Float64);
            error(expr.loc, "unary '-' requires numeric operand, got "
                            + type_to_string(operand));
            return TypeInfo::unknown();

        case TokenKind::KwNot:
            if (operand.is_unknown())              return TypeInfo::of(ValueType::Bool);
            if (operand.base != ValueType::Bool) {
                error(expr.loc, "operator 'not' requires bool operand, got "
                                + type_to_string(operand));
                return TypeInfo::unknown();
            }
            return TypeInfo::of(ValueType::Bool);

        default:
            error(expr.loc, "unsupported unary operator");
            return TypeInfo::unknown();
    }
}

TypeInfo SemanticAnalyzer::analyze_call(const CallExpr& expr) {
    auto it = functions_.find(expr.callee);
    if (it == functions_.end()) {
        error(expr.loc, "call to unknown function '" + expr.callee + "'");
        for (const auto& arg : expr.args) analyze_expr(*arg);
        return TypeInfo::unknown();
    }

    const FunctionSymbol& fn = it->second;

    if (expr.args.size() != fn.param_types.size()) {
        error(expr.loc, "function '" + expr.callee + "' expects "
                        + std::to_string(fn.param_types.size())
                        + " argument(s), got "
                        + std::to_string(expr.args.size()));
        for (const auto& arg : expr.args) analyze_expr(*arg);
        return fn.return_type;
    }

    for (std::size_t i = 0; i < expr.args.size(); ++i) {
        TypeInfo arg_type = analyze_expr(*expr.args[i]);
        const TypeInfo& expected = fn.param_types[i];
        if (!expected.is_unknown() && !arg_type.is_unknown() &&
            !same_type(expected, arg_type)) {
            error(expr.args[i]->loc,
                  "argument " + std::to_string(i + 1)
                  + " of '" + expr.callee + "' expects "
                  + type_to_string(expected)
                  + ", got " + type_to_string(arg_type));
        }
    }
    return fn.return_type;
}

// ── L-value ───────────────────────────────────────────────────────────────────

/// Определяет тип l-value через обычный analyze_expr.
TypeInfo SemanticAnalyzer::resolve_lvalue_type(const Expr& expr) {
    return analyze_expr(expr);
}

/// Проверяет мутабельность l-value; при ошибке сообщает сам.
bool SemanticAnalyzer::check_lvalue_mutable(const Expr& expr) {
    switch (expr.type) {
        case ExprType::Identif: {
            const auto& id = static_cast<const IdentifExpr&>(expr);
            Symbol* sym = lookup_symbol(id.name);
            if (!sym) return false;           // ошибка уже выдана в analyze_expr
            if (!sym->is_mutable) {
                error(id.loc, "cannot assign to immutable variable '" + id.name + "'");
                return false;
            }
            return true;
        }
        case ExprType::Field:
            return check_lvalue_mutable(*static_cast<const FieldExpr&>(expr).object);
        case ExprType::Index:
            return check_lvalue_mutable(*static_cast<const IndexExpr&>(expr).object);
        default:
            error(expr.loc, "expression is not assignable");
            return false;
    }
}

// ── Утилиты типов ─────────────────────────────────────────────────────────────

TypeInfo SemanticAnalyzer::type_from_string(const std::string& name) const {
    // Сначала разрешаем type aliases (без рекурсии в бесконечность)
    {
        auto alias = type_aliases_.find(name);
        if (alias != type_aliases_.end())
            return type_from_string(alias->second);
    }

    if (name == "int64"  || name == "int32"  || name == "int16"  ||
        name == "int8"   || name == "int")
        return TypeInfo::of(ValueType::Int64);
    if (name == "uint64" || name == "uint32" || name == "uint16" || name == "uint8")
        return TypeInfo::of(ValueType::Uint64);
    if (name == "float64" || name == "float32")
        return TypeInfo::of(ValueType::Float64);
    if (name == "bool")                   return TypeInfo::of(ValueType::Bool);
    if (name == "string")                 return TypeInfo::of(ValueType::String);
    if (name == "unit" || name == "void") return TypeInfo::unit();

    // Структура?
    if (structs_.count(name)) return TypeInfo::struct_type(name);

    return TypeInfo::unknown();
}

std::string SemanticAnalyzer::type_to_string(const TypeInfo& type) const {
    switch (type.base) {
        case ValueType::Int64:   return "int64";
        case ValueType::Uint64:  return "uint64";
        case ValueType::Float64: return "float64";
        case ValueType::Bool:    return "bool";
        case ValueType::String:  return "string";
        case ValueType::Unit:    return "unit";
        case ValueType::Struct:  return type.name;
        case ValueType::Unknown: return "<unknown>";
    }
    return "<unknown>";
}

bool SemanticAnalyzer::same_type(const TypeInfo& a, const TypeInfo& b) {
    if (a.is_unknown() || b.is_unknown()) return true;
    // Int64 и Uint64 — оба целочисленные, совместимы для присваивания
    if (is_integer(a.base) && is_integer(b.base)) return true;
    if (a.base != b.base)                          return false;
    if (a.base == ValueType::Struct)               return a.name == b.name;
    return true;
}
