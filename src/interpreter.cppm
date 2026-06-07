module;

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

export module cyton.interpreter;

import cyton.ast;
import cyton.token;

export class Interpreter {
public:
    explicit Interpreter(std::string filename = "");

    int run(const Program& program);

private:
    struct ArrayValue;
    struct StructValue;

    struct UnitValue {};

    struct Value {
        std::variant<
            UnitValue,
            std::int64_t,
            double,
            bool,
            std::string,
            std::shared_ptr<ArrayValue>,
            std::shared_ptr<StructValue>
        > data;

        Value();
        explicit Value(UnitValue value);
        explicit Value(std::int64_t value);
        explicit Value(double value);
        explicit Value(bool value);
        explicit Value(std::string value);
        explicit Value(std::shared_ptr<ArrayValue> value);
        explicit Value(std::shared_ptr<StructValue> value);
    };

    struct ArrayValue {
        std::vector<Value> elements;
    };

    struct StructValue {
        std::string type_name;
        std::unordered_map<std::string, Value> fields;
    };

    struct RuntimeVar {
        Value value;
        bool is_mutable = false;
    };

    enum class FlowKind {
        None,
        Break,
        Continue,
        Return
    };

    struct ExecResult {
        FlowKind kind = FlowKind::None;
        Value value{};
    };

private:
    std::string filename_;

    bool had_runtime_error_ = false;
    bool stopped_ = false;
    int exit_code_ = 0;

    std::vector<std::unordered_map<std::string, RuntimeVar>> scopes_;
    std::unordered_map<std::string, const FunctionDecl*> functions_;

private:
    void runtime_error(Location loc, const std::string& message);

    void begin_scope();
    void end_scope();

    bool define_var(const std::string& name, Value value, bool is_mutable, Location loc);
    RuntimeVar* lookup_var(const std::string& name);

    void collect_functions(const Program& program);
    void collect_namespace_functions(const NamespaceDecl& ns);

    ExecResult exec_top_item(const TopItem& item);
    ExecResult exec_instr(const Instr& instr);
    ExecResult exec_block(const std::vector<std::unique_ptr<Instr>>& body);

    Value eval_expr(const Expr& expr);

    Value eval_num_lit(const NumLitExpr& expr);
    Value eval_float_lit(const FloatLitExpr& expr);
    Value eval_string_lit(const StringLitExpr& expr);
    Value eval_bool_lit(const BoolLitExpr& expr);
    Value eval_identifier(const IdentifExpr& expr);
    Value eval_unary(const UnaryExpr& expr);
    Value eval_binary(const BinaryExpr& expr);
    Value eval_call(const CallExpr& expr);
    Value eval_array_lit(const ArrayLitExpr& expr);
    Value eval_struct_lit(const StructLitExpr& expr);
    Value eval_field(const FieldExpr& expr);
    Value eval_index(const IndexExpr& expr);

    bool assign_to(const Expr& target, Value value);

    Value call_user_function(const FunctionDecl& fn, const std::vector<Value>& args, Location loc);
    Value call_builtin(const CallExpr& expr);

    static bool   is_truthy      (const Value& value);
    static std::string value_to_string(const Value& value);
    static Value  deep_copy      (const Value& value);
};