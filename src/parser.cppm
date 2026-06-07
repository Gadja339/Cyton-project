module;
#include <memory>
#include <string>
#include <vector>

export module cyton.parser;

import cyton.token;
import cyton.ast;

export class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Program parse_program();

private:
    std::vector<Token> tokens_;
    std::size_t pos_ = 0;

    const Token& peek()     const;
    const Token& previous() const;

    bool  is_at_end()                                        const;
    bool  check(TokenKind kind)                              const;
    bool  match(TokenKind kind);
    Token consume(TokenKind kind, const std::string& message);

    void skip_newlines();
    void consume_newline(const std::string& message);

    // Объявления верхнего уровня
    TopItem parse_top_item();
    FunctionDecl  parse_function_decl();
    StructDecl    parse_struct_decl();
    TypeAliasDecl parse_type_alias();
    NamespaceDecl parse_namespace_decl();

    // Блок и инструкции
    std::vector<std::unique_ptr<Instr>> parse_block();
    std::unique_ptr<Instr> parse_instr();
    std::unique_ptr<Instr> parse_var_decl(bool is_var);
    std::unique_ptr<Instr> parse_if();
    std::unique_ptr<Instr> parse_while();
    std::unique_ptr<Instr> parse_return();

    // Выражения (по уровням приоритета)
    std::unique_ptr<Expr> parse_expr();
    std::unique_ptr<Expr> parse_cast();
    std::unique_ptr<Expr> parse_logical_or();
    std::unique_ptr<Expr> parse_logical_and();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_comparison();
    std::unique_ptr<Expr> parse_term();
    std::unique_ptr<Expr> parse_factor();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_postfix();
    std::unique_ptr<Expr> parse_primary();

    // Вспомогательные
    std::string parse_type_name();
    std::vector<std::unique_ptr<Expr>> parse_arg_list();
};
