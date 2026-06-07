module;

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

module cyton.interpreter;

import cyton.ast;
import cyton.token;

// ── Конструкторы Value ────────────────────────────────────────────────────────

Interpreter::Value::Value()
    : data(UnitValue{}) {}

Interpreter::Value::Value(UnitValue value)
    : data(value) {}

Interpreter::Value::Value(std::int64_t value)
    : data(value) {}

Interpreter::Value::Value(double value)
    : data(value) {}

Interpreter::Value::Value(bool value)
    : data(value) {}

Interpreter::Value::Value(std::string value)
    : data(std::move(value)) {}

Interpreter::Value::Value(std::shared_ptr<ArrayValue> value)
    : data(std::move(value)) {}

Interpreter::Value::Value(std::shared_ptr<StructValue> value)
    : data(std::move(value)) {}

// ── Конструктор интерпретатора ────────────────────────────────────────────────

Interpreter::Interpreter(std::string filename)
    : filename_(std::move(filename)) {}

// ── Hidden main: точка входа программы ───────────────────────────────────────
// Фаза 1 — сбор функций, Фаза 2 — выполнение top-level кода (hidden main)

int Interpreter::run(const Program& program) {
    had_runtime_error_ = false;
    stopped_ = false;
    exit_code_ = 0;
    scopes_.clear();
    functions_.clear();

    begin_scope();
    collect_functions(program);

    for (const TopItem& item : program.items) {
        ExecResult result = exec_top_item(item);

        if (had_runtime_error_ || stopped_) {
            break;
        }

        if (result.kind == FlowKind::Break) {
            runtime_error(item.instr ? item.instr->loc : Location{}, "break outside loop");
            break;
        }

        if (result.kind == FlowKind::Continue) {
            runtime_error(item.instr ? item.instr->loc : Location{}, "continue outside loop");
            break;
        }

        if (result.kind == FlowKind::Return) {
            runtime_error(item.instr ? item.instr->loc : Location{}, "return outside function");
            break;
        }
    }

    end_scope();

    if (had_runtime_error_) {
        return 1;
    }

    return exit_code_;
}

// ── Диагностика runtime ───────────────────────────────────────────────────────

void Interpreter::runtime_error(Location loc, const std::string& message) {
    if (!filename_.empty()) {
        std::cerr << filename_ << ":";
    }

    std::cerr
        << loc.line << ":"
        << loc.col
        << ": runtime error: "
        << message
        << "\n";

    had_runtime_error_ = true;
}

// ── Области видимости ─────────────────────────────────────────────────────────

void Interpreter::begin_scope() {
    scopes_.emplace_back();
}

void Interpreter::end_scope() {
    scopes_.pop_back();
}

// ── Переменные: определение и поиск ──────────────────────────────────────────

bool Interpreter::define_var(
    const std::string& name,
    Value value,
    bool is_mutable,
    Location loc
) {
    auto& scope = scopes_.back();

    if (scope.find(name) != scope.end()) {
        runtime_error(loc, "variable '" + name + "' is already defined");
        return false;
    }

    scope[name] = RuntimeVar{
        deep_copy(value),
        is_mutable
    };

    return true;
}

Interpreter::RuntimeVar* Interpreter::lookup_var(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);

        if (found != it->end()) {
            return &found->second;
        }
    }

    return nullptr;
}

// ── Глубокое копирование (value semantics для array / struct) ─────────────────

Interpreter::Value Interpreter::deep_copy(const Value& val) {
    // Примитивы (int, float, bool, string, unit) — копируются trivially
    if (auto* arr = std::get_if<std::shared_ptr<ArrayValue>>(&val.data)) {
        if (!*arr) return val;
        auto copy = std::make_shared<ArrayValue>();
        copy->elements.reserve((*arr)->elements.size());
        for (const auto& elem : (*arr)->elements)
            copy->elements.push_back(deep_copy(elem));
        return Value{copy};
    }
    if (auto* sv = std::get_if<std::shared_ptr<StructValue>>(&val.data)) {
        if (!*sv) return val;
        auto copy = std::make_shared<StructValue>();
        copy->type_name = (*sv)->type_name;
        for (const auto& [name, field] : (*sv)->fields)
            copy->fields[name] = deep_copy(field);
        return Value{copy};
    }
    return val;
}

// ── Сбор объявлений функций (fun / namespace::fun) ────────────────────────────

void Interpreter::collect_functions(const Program& program) {
    for (const TopItem& item : program.items) {
        if (item.kind == TopItem::Kind::Function) {
            functions_[item.function.name] = &item.function;
        } else if (item.kind == TopItem::Kind::Namespace) {
            collect_namespace_functions(item.ns);
        }
    }
}

void Interpreter::collect_namespace_functions(const NamespaceDecl& ns) {
    for (const FunctionDecl& fn : ns.functions) {
        functions_[ns.name + "::" + fn.name] = &fn;
    }
}

// ── Выполнение top-level элементов ───────────────────────────────────────────

Interpreter::ExecResult Interpreter::exec_top_item(const TopItem& item) {
    switch (item.kind) {
        case TopItem::Kind::Instr:
            return exec_instr(*item.instr);

        case TopItem::Kind::Function:
        case TopItem::Kind::Struct:
        case TopItem::Kind::TypeAlias:
        case TopItem::Kind::Namespace:
            return {};

        default:
            return {};
    }
}

// ── Выполнение инструкций ─────────────────────────────────────────────────────

Interpreter::ExecResult Interpreter::exec_instr(const Instr& instr) {
    switch (instr.type) {
        case InstrType::Let: {
            const auto& let_instr = static_cast<const LetInstr&>(instr);
            Value value = eval_expr(*let_instr.init);

            if (had_runtime_error_) {
                return {};
            }

            define_var(let_instr.name, std::move(value), false, let_instr.loc);
            return {};
        }

        case InstrType::Var: {
            const auto& var_instr = static_cast<const VarInstr&>(instr);
            Value value = eval_expr(*var_instr.init);

            if (had_runtime_error_) {
                return {};
            }

            define_var(var_instr.name, std::move(value), true, var_instr.loc);
            return {};
        }

        case InstrType::Expr: {
            const auto& expr_instr = static_cast<const ExprInstr&>(instr);
            eval_expr(*expr_instr.expr);
            return {};
        }

        case InstrType::Assign: {
            const auto& assign_instr = static_cast<const AssignInstr&>(instr);
            Value value = eval_expr(*assign_instr.value);

            if (had_runtime_error_) {
                return {};
            }

            assign_to(*assign_instr.target, std::move(value));
            return {};
        }

        case InstrType::If: {
            const auto& if_instr = static_cast<const IfInstr&>(instr);
            Value condition = eval_expr(*if_instr.condition);

            if (had_runtime_error_) {
                return {};
            }

            if (is_truthy(condition)) {
                return exec_block(if_instr.then_branch);
            }

            return exec_block(if_instr.else_branch);
        }

        case InstrType::While: {
            const auto& while_instr = static_cast<const WhileInstr&>(instr);

            while (true) {
                Value condition = eval_expr(*while_instr.condition);

                if (had_runtime_error_) {
                    return {};
                }

                if (!is_truthy(condition)) {
                    break;
                }

                ExecResult result = exec_block(while_instr.body);

                if (result.kind == FlowKind::Break) {
                    break;
                }

                if (result.kind == FlowKind::Continue) {
                    continue;
                }

                if (result.kind == FlowKind::Return) {
                    return result;
                }

                if (had_runtime_error_ || stopped_) {
                    break;
                }
            }

            return {};
        }

        case InstrType::Break:
            return ExecResult{FlowKind::Break, Value{}};

        case InstrType::Continue:
            return ExecResult{FlowKind::Continue, Value{}};

        case InstrType::Return: {
            const auto& return_instr = static_cast<const ReturnInstr&>(instr);

            if (return_instr.value == nullptr) {
                return ExecResult{FlowKind::Return, Value{}};
            }

            return ExecResult{
                FlowKind::Return,
                eval_expr(*return_instr.value)
            };
        }

        case InstrType::Block: {
            const auto& block_instr = static_cast<const BlockInstr&>(instr);
            return exec_block(block_instr.body);
        }
    }

    return {};
}

// ── Выполнение блока (открывает/закрывает область видимости) ──────────────────

Interpreter::ExecResult Interpreter::exec_block(
    const std::vector<std::unique_ptr<Instr>>& body
) {
    begin_scope();

    for (const auto& instr : body) {
        ExecResult result = exec_instr(*instr);

        if (
            result.kind != FlowKind::None ||
            had_runtime_error_ ||
            stopped_
        ) {
            end_scope();
            return result;
        }
    }

    end_scope();
    return {};
}

// ── Вычисление выражений ──────────────────────────────────────────────────────

Interpreter::Value Interpreter::eval_expr(const Expr& expr) {
    switch (expr.type) {
        case ExprType::NumLit:
            return eval_num_lit(static_cast<const NumLitExpr&>(expr));

        case ExprType::FloatLit:
            return eval_float_lit(static_cast<const FloatLitExpr&>(expr));

        case ExprType::StringLit:
            return eval_string_lit(static_cast<const StringLitExpr&>(expr));

        case ExprType::BoolLit:
            return eval_bool_lit(static_cast<const BoolLitExpr&>(expr));

        case ExprType::UnitLit:
            return Value{};

        case ExprType::Identif:
            return eval_identifier(static_cast<const IdentifExpr&>(expr));

        case ExprType::Unary:
            return eval_unary(static_cast<const UnaryExpr&>(expr));

        case ExprType::Binary:
            return eval_binary(static_cast<const BinaryExpr&>(expr));

        case ExprType::Call:
            return eval_call(static_cast<const CallExpr&>(expr));

        case ExprType::ArrayLit:
            return eval_array_lit(static_cast<const ArrayLitExpr&>(expr));

        case ExprType::StructLit:
            return eval_struct_lit(static_cast<const StructLitExpr&>(expr));

        case ExprType::Field:
            return eval_field(static_cast<const FieldExpr&>(expr));

        case ExprType::Index:
            return eval_index(static_cast<const IndexExpr&>(expr));

        case ExprType::NamespaceAccess:
        case ExprType::Cast:
            runtime_error(expr.loc, "this expression is not implemented in interpreter yet");
            return {};
    }

    return {};
}

// ── Литералы: int, float, string, bool ───────────────────────────────────────

Interpreter::Value Interpreter::eval_num_lit(const NumLitExpr& expr) {
    return Value{static_cast<std::int64_t>(std::stoll(expr.value))};
}

Interpreter::Value Interpreter::eval_float_lit(const FloatLitExpr& expr) {
    return Value{std::stod(expr.value)};
}

Interpreter::Value Interpreter::eval_string_lit(const StringLitExpr& expr) {
    return Value{expr.value};
}

Interpreter::Value Interpreter::eval_bool_lit(const BoolLitExpr& expr) {
    return Value{expr.value};
}

// ── Идентификатор ─────────────────────────────────────────────────────────────

Interpreter::Value Interpreter::eval_identifier(const IdentifExpr& expr) {
    RuntimeVar* var = lookup_var(expr.name);

    if (var == nullptr) {
        runtime_error(expr.loc, "use of undeclared variable '" + expr.name + "'");
        return {};
    }

    return var->value;
}

// ── Унарные операции: -, not ──────────────────────────────────────────────────

Interpreter::Value Interpreter::eval_unary(const UnaryExpr& expr) {
    Value operand = eval_expr(*expr.operand);

    if (had_runtime_error_) {
        return {};
    }

    if (expr.op == TokenKind::Minus) {
        if (auto* value = std::get_if<std::int64_t>(&operand.data)) {
            return Value{-(*value)};
        }

        if (auto* value = std::get_if<double>(&operand.data)) {
            return Value{-(*value)};
        }

        runtime_error(expr.loc, "unary '-' requires numeric operand");
        return {};
    }

    if (expr.op == TokenKind::KwNot) {
        if (auto* value = std::get_if<bool>(&operand.data)) {
            return Value{!(*value)};
        }

        runtime_error(expr.loc, "operator 'not' requires bool operand");
        return {};
    }

    runtime_error(expr.loc, "unsupported unary operator");
    return {};
}

// ── Бинарные операции: and/or (short-circuit), арифметика, сравнения ──────────

Interpreter::Value Interpreter::eval_binary(const BinaryExpr& expr) {
    if (expr.op == TokenKind::KwAnd) {
        Value left = eval_expr(*expr.left);

        if (had_runtime_error_) {
            return {};
        }

        if (!is_truthy(left)) {
            return Value{false};
        }

        Value right = eval_expr(*expr.right);
        return Value{is_truthy(right)};
    }

    if (expr.op == TokenKind::KwOr) {
        Value left = eval_expr(*expr.left);

        if (had_runtime_error_) {
            return {};
        }

        if (is_truthy(left)) {
            return Value{true};
        }

        Value right = eval_expr(*expr.right);
        return Value{is_truthy(right)};
    }

    Value left = eval_expr(*expr.left);
    Value right = eval_expr(*expr.right);

    if (had_runtime_error_) {
        return {};
    }

    if (auto* l = std::get_if<std::int64_t>(&left.data)) {
        if (auto* r = std::get_if<std::int64_t>(&right.data)) {
            switch (expr.op) {
                case TokenKind::Plus:
                    return Value{*l + *r};

                case TokenKind::Minus:
                    return Value{*l - *r};

                case TokenKind::Star:
                    return Value{*l * *r};

                case TokenKind::Slash:
                    if (*r == 0) {
                        runtime_error(expr.loc, "division by zero");
                        return {};
                    }
                    return Value{*l / *r};

                case TokenKind::Percent:
                    if (*r == 0) {
                        runtime_error(expr.loc, "division by zero");
                        return {};
                    }
                    return Value{*l % *r};

                case TokenKind::EqualEqual:
                    return Value{*l == *r};

                case TokenKind::BangEqual:
                    return Value{*l != *r};

                case TokenKind::Less:
                    return Value{*l < *r};

                case TokenKind::LessEqual:
                    return Value{*l <= *r};

                case TokenKind::Greater:
                    return Value{*l > *r};

                case TokenKind::GreaterEqual:
                    return Value{*l >= *r};

                default:
                    break;
            }
        }
    }

    if (auto* l = std::get_if<double>(&left.data)) {
        if (auto* r = std::get_if<double>(&right.data)) {
            switch (expr.op) {
                case TokenKind::Plus:
                    return Value{*l + *r};

                case TokenKind::Minus:
                    return Value{*l - *r};

                case TokenKind::Star:
                    return Value{*l * *r};

                case TokenKind::Slash:
                    if (*r == 0.0) {
                        runtime_error(expr.loc, "division by zero");
                        return {};
                    }
                    return Value{*l / *r};

                case TokenKind::EqualEqual:
                    return Value{*l == *r};

                case TokenKind::BangEqual:
                    return Value{*l != *r};

                case TokenKind::Less:
                    return Value{*l < *r};

                case TokenKind::LessEqual:
                    return Value{*l <= *r};

                case TokenKind::Greater:
                    return Value{*l > *r};

                case TokenKind::GreaterEqual:
                    return Value{*l >= *r};

                default:
                    break;
            }
        }
    }

    if (auto* l = std::get_if<std::string>(&left.data)) {
        if (auto* r = std::get_if<std::string>(&right.data)) {
            switch (expr.op) {
                case TokenKind::Plus:
                    return Value{*l + *r};

                case TokenKind::EqualEqual:
                    return Value{*l == *r};

                case TokenKind::BangEqual:
                    return Value{*l != *r};

                default:
                    break;
            }
        }
    }

    if (auto* l = std::get_if<bool>(&left.data)) {
        if (auto* r = std::get_if<bool>(&right.data)) {
            switch (expr.op) {
                case TokenKind::EqualEqual:
                    return Value{*l == *r};

                case TokenKind::BangEqual:
                    return Value{*l != *r};

                default:
                    break;
            }
        }
    }

    if (std::holds_alternative<UnitValue>(left.data) &&
        std::holds_alternative<UnitValue>(right.data)) {
        if (expr.op == TokenKind::EqualEqual) return Value{true};
        if (expr.op == TokenKind::BangEqual)  return Value{false};
    }

    runtime_error(expr.loc, "unsupported binary operation");
    return {};
}

// ── Вызов функции (builtin / пользовательская) ────────────────────────────────

Interpreter::Value Interpreter::eval_call(const CallExpr& expr) {
    if (
        expr.callee == "print" ||
        expr.callee == "input" ||
        expr.callee == "exit" ||
        expr.callee == "panic" ||
        expr.callee == "assert"
    ) {
        return call_builtin(expr);
    }

    auto it = functions_.find(expr.callee);

    if (it == functions_.end()) {
        runtime_error(expr.loc, "unknown function '" + expr.callee + "'");
        return {};
    }

    std::vector<Value> args;

    for (const auto& arg : expr.args) {
        args.push_back(eval_expr(*arg));

        if (had_runtime_error_) {
            return {};
        }
    }

    return call_user_function(*it->second, args, expr.loc);
}

// ── Вызов пользовательской функции (fun) ─────────────────────────────────────

Interpreter::Value Interpreter::call_user_function(
    const FunctionDecl& fn,
    const std::vector<Value>& args,
    Location loc
) {
    if (args.size() != fn.params.size()) {
        runtime_error(loc, "wrong number of function arguments");
        return {};
    }

    begin_scope();

    for (std::size_t i = 0; i < args.size(); ++i) {
        define_var(fn.params[i].name, args[i], false, fn.loc);
    }

    ExecResult result = exec_block(fn.body);

    end_scope();

    if (result.kind == FlowKind::Return) {
        return result.value;
    }

    return {};
}

// ── Встроенные функции: print, input, exit, panic, assert ────────────────────

Interpreter::Value Interpreter::call_builtin(const CallExpr& expr) {
    if (expr.callee == "print") {
        if (expr.args.size() != 1) {
            runtime_error(expr.loc, "print expects 1 argument");
            return {};
        }

        Value value = eval_expr(*expr.args[0]);

        if (had_runtime_error_) {
            return {};
        }

        std::cout << value_to_string(value) << "\n";
        return {};
    }

    if (expr.callee == "input") {
        std::string line;
        std::getline(std::cin, line);
        return Value{line};
    }

    if (expr.callee == "exit") {
        int code = 0;

        if (expr.args.size() == 1) {
            Value value = eval_expr(*expr.args[0]);

            if (auto* int_value = std::get_if<std::int64_t>(&value.data)) {
                code = static_cast<int>(*int_value);
            } else {
                runtime_error(expr.loc, "exit expects int argument");
                return {};
            }
        }

        exit_code_ = code;
        stopped_ = true;
        return {};
    }

    if (expr.callee == "panic") {
        std::string message = "panic";

        if (expr.args.size() == 1) {
            Value value = eval_expr(*expr.args[0]);
            message = value_to_string(value);
        }

        runtime_error(expr.loc, message);
        return {};
    }

    if (expr.callee == "assert") {
        if (expr.args.empty()) {
            runtime_error(expr.loc, "assert expects condition");
            return {};
        }

        Value condition = eval_expr(*expr.args[0]);

        if (!is_truthy(condition)) {
            runtime_error(expr.loc, "assertion failed");
        }

        return {};
    }

    runtime_error(expr.loc, "unknown builtin function");
    return {};
}

// ── Массивы: литерал, struct литерал, доступ к полю, индексирование ──────────

Interpreter::Value Interpreter::eval_array_lit(const ArrayLitExpr& expr) {
    auto array = std::make_shared<ArrayValue>();

    for (const auto& element : expr.elements) {
        array->elements.push_back(eval_expr(*element));

        if (had_runtime_error_) {
            return {};
        }
    }

    return Value{array};
}

Interpreter::Value Interpreter::eval_struct_lit(const StructLitExpr& expr) {
    auto object = std::make_shared<StructValue>();
    object->type_name = expr.type_name;

    for (const StructFieldInit& field : expr.fields) {
        object->fields[field.name] = eval_expr(*field.value);

        if (had_runtime_error_) {
            return {};
        }
    }

    return Value{object};
}

Interpreter::Value Interpreter::eval_field(const FieldExpr& expr) {
    Value object = eval_expr(*expr.object);

    auto* struct_value = std::get_if<std::shared_ptr<StructValue>>(&object.data);

    if (struct_value == nullptr || *struct_value == nullptr) {
        runtime_error(expr.loc, "field access requires struct value");
        return {};
    }

    auto found = (*struct_value)->fields.find(expr.field);

    if (found == (*struct_value)->fields.end()) {
        runtime_error(expr.loc, "unknown field '" + expr.field + "'");
        return {};
    }

    return found->second;
}

Interpreter::Value Interpreter::eval_index(const IndexExpr& expr) {
    Value object = eval_expr(*expr.object);
    Value index = eval_expr(*expr.index);

    auto* array_value = std::get_if<std::shared_ptr<ArrayValue>>(&object.data);
    auto* int_index = std::get_if<std::int64_t>(&index.data);

    if (array_value == nullptr || *array_value == nullptr) {
        runtime_error(expr.loc, "indexing requires array value");
        return {};
    }

    if (int_index == nullptr) {
        runtime_error(expr.index->loc, "array index must be int");
        return {};
    }

    if (*int_index < 0 || static_cast<std::size_t>(*int_index) >= (*array_value)->elements.size()) {
        runtime_error(expr.loc, "array index out of bounds");
        return {};
    }

    return (*array_value)->elements[static_cast<std::size_t>(*int_index)];
}

// ── Присваивание: идентификатор, поле struct, индекс массива ─────────────────

bool Interpreter::assign_to(const Expr& target, Value value) {
    if (target.type == ExprType::Identif) {
        const auto& id = static_cast<const IdentifExpr&>(target);

        RuntimeVar* var = lookup_var(id.name);

        if (var == nullptr) {
            runtime_error(id.loc, "assignment to undeclared variable '" + id.name + "'");
            return false;
        }

        if (!var->is_mutable) {
            runtime_error(id.loc, "cannot assign to immutable variable '" + id.name + "'");
            return false;
        }

        var->value = deep_copy(value);
        return true;
    }

    if (target.type == ExprType::Field) {
        const auto& field = static_cast<const FieldExpr&>(target);
        Value object = eval_expr(*field.object);

        auto* struct_value = std::get_if<std::shared_ptr<StructValue>>(&object.data);

        if (struct_value == nullptr || *struct_value == nullptr) {
            runtime_error(field.loc, "field assignment requires struct value");
            return false;
        }

        (*struct_value)->fields[field.field] = std::move(value);
        return true;
    }

    if (target.type == ExprType::Index) {
        const auto& index_expr = static_cast<const IndexExpr&>(target);

        Value object = eval_expr(*index_expr.object);
        Value index = eval_expr(*index_expr.index);

        auto* array_value = std::get_if<std::shared_ptr<ArrayValue>>(&object.data);
        auto* int_index = std::get_if<std::int64_t>(&index.data);

        if (array_value == nullptr || *array_value == nullptr) {
            runtime_error(index_expr.loc, "index assignment requires array value");
            return false;
        }

        if (int_index == nullptr) {
            runtime_error(index_expr.index->loc, "array index must be int");
            return false;
        }

        if (*int_index < 0 || static_cast<std::size_t>(*int_index) >= (*array_value)->elements.size()) {
            runtime_error(index_expr.loc, "array index out of bounds");
            return false;
        }

        (*array_value)->elements[static_cast<std::size_t>(*int_index)] = std::move(value);
        return true;
    }

    runtime_error(target.loc, "expression is not assignable");
    return false;
}

// ── Вспомогательные утилиты ───────────────────────────────────────────────────

bool Interpreter::is_truthy(const Value& value) {
    if (auto* bool_value = std::get_if<bool>(&value.data)) {
        return *bool_value;
    }

    return false;
}

std::string Interpreter::value_to_string(const Value& value) {
    if (std::holds_alternative<UnitValue>(value.data)) {
        return "unit";
    }

    if (auto* int_value = std::get_if<std::int64_t>(&value.data)) {
        return std::to_string(*int_value);
    }

    if (auto* float_value = std::get_if<double>(&value.data)) {
        std::ostringstream out;
        out << *float_value;
        return out.str();
    }

    if (auto* bool_value = std::get_if<bool>(&value.data)) {
        return *bool_value ? "true" : "false";
    }

    if (auto* string_value = std::get_if<std::string>(&value.data)) {
        return *string_value;
    }

    if (auto* array_value = std::get_if<std::shared_ptr<ArrayValue>>(&value.data)) {
        std::string result = "[";

        for (std::size_t i = 0; i < (*array_value)->elements.size(); ++i) {
            if (i != 0) {
                result += ", ";
            }

            result += value_to_string((*array_value)->elements[i]);
        }

        result += "]";
        return result;
    }

    if (auto* struct_value = std::get_if<std::shared_ptr<StructValue>>(&value.data)) {
        std::string result = (*struct_value)->type_name + " { ";

        bool first = true;

        for (const auto& [name, field_value] : (*struct_value)->fields) {
            if (!first) {
                result += ", ";
            }

            first = false;
            result += name;
            result += ": ";
            result += value_to_string(field_value);
        }

        result += " }";
        return result;
    }

    return "<unknown>";
}