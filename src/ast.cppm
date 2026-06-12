module;
#include <memory>
#include <string>
#include <utility>
#include <vector>

export module cyton.ast;

import cyton.token;

export enum class ExprType {
    NumLit,
    FloatLit,
    StringLit,
    BoolLit,
    UnitLit,
    Identif,
    Unary,
    Binary,
    Call,
    Index,
    Field,
    NamespaceAccess,
    Cast,
    ArrayLit,
    StructLit,
    Sizeof,
    Typeid,
    Typeof,
};

export enum class InstrType {
    Var,
    Let,
    Expr,
    Assign,
    If,
    While,
    Break,
    Continue,
    Return,
    Block,
};

export struct Expr {
    ExprType type;
    Location loc;
    explicit Expr(ExprType kind, Location loc)
        : type(kind), loc(loc) {}
    virtual ~Expr() = default;
};

export struct NumLitExpr : Expr {
    std::string value;
    NumLitExpr(std::string value, Location loc)
        : Expr(ExprType::NumLit, loc), value(std::move(value)) {}
};

export struct FloatLitExpr : Expr {
    std::string value;
    FloatLitExpr(std::string value, Location loc)
        : Expr(ExprType::FloatLit, loc), value(std::move(value)) {}
};

export struct StringLitExpr : Expr {
    std::string value;
    StringLitExpr(std::string value, Location loc)
        : Expr(ExprType::StringLit, loc), value(std::move(value)) {}
};

export struct BoolLitExpr : Expr {
    bool value;
    BoolLitExpr(bool value, Location loc)
        : Expr(ExprType::BoolLit, loc), value(value) {}
};

export struct UnitLitExpr : Expr {
    explicit UnitLitExpr(Location loc)
        : Expr(ExprType::UnitLit, loc) {}
};

export struct IdentifExpr : Expr {
    std::string name;
    IdentifExpr(std::string name, Location loc)
        : Expr(ExprType::Identif, loc), name(std::move(name)) {}
};

export struct UnaryExpr : Expr {
    TokenKind op;
    std::unique_ptr<Expr> operand;
    UnaryExpr(TokenKind op, std::unique_ptr<Expr> operand, Location loc)
        : Expr(ExprType::Unary, loc),
          op(op),
          operand(std::move(operand)) {}
};

export struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    TokenKind op;
    std::unique_ptr<Expr> right;
    BinaryExpr(std::unique_ptr<Expr> left, TokenKind op,
               std::unique_ptr<Expr> right, Location loc)
        : Expr(ExprType::Binary, loc),
          left(std::move(left)),
          op(op),
          right(std::move(right)) {}
};

export struct CallExpr : Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args,
             Location loc)
        : Expr(ExprType::Call, loc),
          callee(std::move(callee)),
          args(std::move(args)) {}
};

export struct IndexExpr : Expr {
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
    IndexExpr(std::unique_ptr<Expr> object, std::unique_ptr<Expr> index,
              Location loc)
        : Expr(ExprType::Index, loc),
          object(std::move(object)),
          index(std::move(index)) {}
};

export struct FieldExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string field;
    FieldExpr(std::unique_ptr<Expr> object, std::string field, Location loc)
        : Expr(ExprType::Field, loc),
          object(std::move(object)),
          field(std::move(field)) {}
};

export struct NamespaceAccessExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string member;
    NamespaceAccessExpr(std::unique_ptr<Expr> object, std::string member,
                        Location loc)
        : Expr(ExprType::NamespaceAccess, loc),
          object(std::move(object)),
          member(std::move(member)) {}
};

export struct CastExpr : Expr {
    std::unique_ptr<Expr> expr;
    std::string target_type;
    CastExpr(std::unique_ptr<Expr> expr, std::string target_type, Location loc)
        : Expr(ExprType::Cast, loc),
          expr(std::move(expr)),
          target_type(std::move(target_type)) {}
};

// sizeof(type_or_expr) → int64
export struct SizeofExpr : Expr {
    std::unique_ptr<Expr> operand;
    SizeofExpr(std::unique_ptr<Expr> operand, Location loc)
        : Expr(ExprType::Sizeof, loc), operand(std::move(operand)) {}
};

// typeid(expr) → string: runtime type name
export struct TypeidExpr : Expr {
    std::unique_ptr<Expr> operand;
    TypeidExpr(std::unique_ptr<Expr> operand, Location loc)
        : Expr(ExprType::Typeid, loc), operand(std::move(operand)) {}
};

// typeof(expr) → string: static type name
export struct TypeofExpr : Expr {
    std::unique_ptr<Expr> operand;
    TypeofExpr(std::unique_ptr<Expr> operand, Location loc)
        : Expr(ExprType::Typeof, loc), operand(std::move(operand)) {}
};

export struct ArrayLitExpr : Expr {
    std::vector<std::unique_ptr<Expr>> elements;
    ArrayLitExpr(std::vector<std::unique_ptr<Expr>> elements, Location loc)
        : Expr(ExprType::ArrayLit, loc),
          elements(std::move(elements)) {}
};

export struct StructFieldInit {
    std::string name;
    std::unique_ptr<Expr> value;
};

export struct StructLitExpr : Expr {
    std::string type_name;
    std::vector<StructFieldInit> fields;
    StructLitExpr(std::string type_name,
                  std::vector<StructFieldInit> fields, Location loc)
        : Expr(ExprType::StructLit, loc),
          type_name(std::move(type_name)),
          fields(std::move(fields)) {}
};

// ── Инструкции ──────────────────────────────────────────────────────────────

export struct Instr {
    InstrType type;
    Location loc;
    explicit Instr(InstrType kind, Location loc)
        : type(kind), loc(loc) {}
    virtual ~Instr() = default;
};

export struct LetInstr : Instr {
    std::string name;
    std::string explicit_type; // пусто если тип не указан
    std::unique_ptr<Expr> init;
    LetInstr(std::string name, std::string explicit_type,
             std::unique_ptr<Expr> init, Location loc)
        : Instr(InstrType::Let, loc),
          name(std::move(name)),
          explicit_type(std::move(explicit_type)),
          init(std::move(init)) {}
};

export struct VarInstr : Instr {
    std::string name;
    std::string explicit_type;
    std::unique_ptr<Expr> init;
    VarInstr(std::string name, std::string explicit_type,
             std::unique_ptr<Expr> init, Location loc)
        : Instr(InstrType::Var, loc),
          name(std::move(name)),
          explicit_type(std::move(explicit_type)),
          init(std::move(init)) {}
};

export struct ExprInstr : Instr {
    std::unique_ptr<Expr> expr;
    ExprInstr(std::unique_ptr<Expr> expr, Location loc)
        : Instr(InstrType::Expr, loc),
          expr(std::move(expr)) {}
};

export struct AssignInstr : Instr {
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> value;
    AssignInstr(std::unique_ptr<Expr> target, std::unique_ptr<Expr> value,
                Location loc)
        : Instr(InstrType::Assign, loc),
          target(std::move(target)),
          value(std::move(value)) {}
};

export struct IfInstr : Instr {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Instr>> then_branch;
    std::vector<std::unique_ptr<Instr>> else_branch;
    IfInstr(std::unique_ptr<Expr> condition,
            std::vector<std::unique_ptr<Instr>> then_branch,
            std::vector<std::unique_ptr<Instr>> else_branch,
            Location loc)
        : Instr(InstrType::If, loc),
          condition(std::move(condition)),
          then_branch(std::move(then_branch)),
          else_branch(std::move(else_branch)) {}
};

export struct WhileInstr : Instr {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Instr>> body;
    WhileInstr(std::unique_ptr<Expr> condition,
               std::vector<std::unique_ptr<Instr>> body, Location loc)
        : Instr(InstrType::While, loc),
          condition(std::move(condition)),
          body(std::move(body)) {}
};

export struct BreakInstr : Instr {
    explicit BreakInstr(Location loc)
        : Instr(InstrType::Break, loc) {}
};

export struct ContinueInstr : Instr {
    explicit ContinueInstr(Location loc)
        : Instr(InstrType::Continue, loc) {}
};

export struct BlockInstr : Instr {
    std::vector<std::unique_ptr<Instr>> body;
    BlockInstr(std::vector<std::unique_ptr<Instr>> body, Location loc)
        : Instr(InstrType::Block, loc),
          body(std::move(body)) {}
};

export struct ReturnInstr : Instr {
    std::unique_ptr<Expr> value; // nullptr если просто return
    ReturnInstr(std::unique_ptr<Expr> value, Location loc)
        : Instr(InstrType::Return, loc),
          value(std::move(value)) {}
};

// ── Объявления верхнего уровня ───────────────────────────────────────────────

export struct Parameter {
    std::string type_name;
    std::string name;
};

export struct FunctionDecl {
    std::string name;
    std::vector<Parameter> params;
    std::string return_type; // пусто → unit
    std::vector<std::unique_ptr<Instr>> body;
    Location loc;
};

export struct FieldDecl {
    std::string type_name;
    std::string name;
};

export struct StructDecl {
    std::string name;
    std::vector<FieldDecl> fields;
    Location loc;
};

export struct TypeAliasDecl {
    std::string name;
    std::string target;
    Location loc;
};

export struct NamespaceDecl {
    std::string name;
    std::vector<FunctionDecl> functions;
    Location loc;
};

export struct TopItem {
    enum class Kind { Function, Struct, TypeAlias, Namespace, Instr };
    Kind kind;

    // только одно поле активно в зависимости от kind
    FunctionDecl  function;
    StructDecl    structure;
    TypeAliasDecl type_alias;
    NamespaceDecl ns;
    std::unique_ptr<Instr> instr;

    static TopItem make_instr(std::unique_ptr<Instr> i) {
        TopItem t;
        t.kind  = Kind::Instr;
        t.instr = std::move(i);
        return t;
    }
    static TopItem make_function(FunctionDecl f) {
        TopItem t;
        t.kind     = Kind::Function;
        t.function = std::move(f);
        return t;
    }
    static TopItem make_struct(StructDecl s) {
        TopItem t;
        t.kind      = Kind::Struct;
        t.structure = std::move(s);
        return t;
    }
    static TopItem make_type_alias(TypeAliasDecl ta) {
        TopItem t;
        t.kind       = Kind::TypeAlias;
        t.type_alias = std::move(ta);
        return t;
    }
    static TopItem make_namespace(NamespaceDecl n) {
        TopItem t;
        t.kind = Kind::Namespace;
        t.ns   = std::move(n);
        return t;
    }
};

export struct Program {
    std::vector<TopItem> items;
};
