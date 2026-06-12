module;
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

module cyton.parser;

// ── Вспомогательная: разворачивает цепочку NamespaceAccess/Identif в строку ──

static std::string flatten_callee(const Expr* expr) {
    if (expr->type == ExprType::Identif)
        return static_cast<const IdentifExpr*>(expr)->name;
    if (expr->type == ExprType::NamespaceAccess) {
        const auto* ns = static_cast<const NamespaceAccessExpr*>(expr);
        return flatten_callee(ns->object.get()) + "::" + ns->member;
    }
    return "?";
}

// ── Вспомогательные функции ──────────────────────────────────────────────────────────────────

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)) {}

const Token& Parser::peek() const     { return tokens_[pos_]; }
const Token& Parser::previous() const { return tokens_[pos_ - 1]; }

bool Parser::is_at_end() const { return peek().kind == TokenKind::Eof; }

bool Parser::check(TokenKind kind) const {
    return !is_at_end() && peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (!check(kind)) return false;
    ++pos_;
    return true;
}

Token Parser::consume(TokenKind kind, const std::string& message) {
    if (check(kind)) return tokens_[pos_++];
    const Token& t = peek();
    std::cerr << t.loca.line << ":" << t.loca.col << ": error: " << message << "\n";
    throw std::runtime_error("Ошибка парсера");
}

void Parser::skip_newlines() {
    while (match(TokenKind::Newline)) {}
}

void Parser::consume_newline(const std::string& message) {
    if (is_at_end()) return;
    if (check(TokenKind::Semicolon)) return; // ';' закрывает блок — неявный конец инструкции
    consume(TokenKind::Newline, message);
}

// ── Тип ──────────────────────────────────────────────────────────────────────

std::string Parser::parse_type_name() {
    // unit — ключевое слово, но допустимо как имя типа
    if (match(TokenKind::KwUnit)) return "unit";
    std::string name = consume(TokenKind::Identifier, "Неожидаеммый тип").lexeme;
    while (check(TokenKind::DoubleColon)) {
        match(TokenKind::DoubleColon);
        name += "::" + consume(TokenKind::Identifier, "Неожидаемый тип после '::'").lexeme;
    }
    return name;
}

// ── Программа ────────────────────────────────────────────────────────────────

Program Parser::parse_program() {
    Program program;
    skip_newlines();
    while (!is_at_end()) {
        program.items.push_back(parse_top_item());
        skip_newlines();
    }
    return program;
}

// ── Объявления верхнего уровня ───────────────────────────────────────────────

TopItem Parser::parse_top_item() {
    if (check(TokenKind::KwFun))       return TopItem::make_function(parse_function_decl());
    if (check(TokenKind::KwStruct))    return TopItem::make_struct(parse_struct_decl());
    if (check(TokenKind::KwType))      return TopItem::make_type_alias(parse_type_alias());
    if (check(TokenKind::KwNamespace)) return TopItem::make_namespace(parse_namespace_decl());
    return TopItem::make_instr(parse_instr());
}

// fun
FunctionDecl Parser::parse_function_decl() {
    consume(TokenKind::KwFun, "Ожидается 'fun'");
    FunctionDecl decl;
    decl.loc = peek().loca;

    // [Доп A.2.11] Перегрузка операторов: fun operator+(...)
    if (check(TokenKind::Identifier) && peek().lexeme == "operator") {
        ++pos_;  // consume "operator"
        std::string op = token_kind_to_op_string(peek().kind);
        if (op.empty()) {
            const Token& t = peek();
            std::cerr << t.loca.line << ":" << t.loca.col
                      << ": error: ожидается символ оператора после 'operator'\n";
            throw std::runtime_error("parse error");
        }
        ++pos_;  // consume operator symbol
        decl.name = "operator" + op;
    } else {
        decl.name = consume(TokenKind::Identifier, "Ожидается имя функции").lexeme;
    }

    consume(TokenKind::LParen, "Ожидается '(' после имени функции");
    if (!check(TokenKind::RParen)) {
        do {
            Parameter p;
            p.type_name = parse_type_name();
            p.name      = consume(TokenKind::Identifier, "Ожидается имя параметра").lexeme;
            decl.params.push_back(std::move(p));
        } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "Ожидается ')' после параметров");

    if (match(TokenKind::Arrow))
        decl.return_type = parse_type_name();

    decl.body = parse_block();
    return decl;
}

// struct
StructDecl Parser::parse_struct_decl() {
    consume(TokenKind::KwStruct, "Ожидается 'struct'");
    StructDecl decl;
    decl.loc  = peek().loca;
    decl.name = consume(TokenKind::Identifier, "Ожидается имя структуры").lexeme;
    consume(TokenKind::Colon, "Ожидается ':' после имени структуры");
    consume(TokenKind::Newline, "Ожидается новая строка после ':'");

    while (!check(TokenKind::Semicolon) && !is_at_end()) {
        skip_newlines();
        if (check(TokenKind::Semicolon)) break;
        FieldDecl f;
        f.type_name = parse_type_name();
        f.name      = consume(TokenKind::Identifier, "Ожидается имя поля").lexeme;
        decl.fields.push_back(std::move(f));
        if (!check(TokenKind::Semicolon))
            consume(TokenKind::Newline, "Ожидается новая строка после поля");
    }
    consume(TokenKind::Semicolon, "Ожидается ';' чтобы закрыть структуру");
    return decl;
}

// type alias
TypeAliasDecl Parser::parse_type_alias() {
    consume(TokenKind::KwType, "Ожидается 'type'");
    TypeAliasDecl decl;
    decl.loc    = peek().loca;
    decl.name   = consume(TokenKind::Identifier, "Ожидается имя псевдонима").lexeme;
    consume(TokenKind::Assign, "Ожидается '=' после имени псевдонима");
    decl.target = parse_type_name();
    consume(TokenKind::Newline, "Ожидается новая строка после псевдонима типа");
    return decl;
}

// namespace
NamespaceDecl Parser::parse_namespace_decl() {
    consume(TokenKind::KwNamespace, "Ожидается 'namespace'");
    NamespaceDecl decl;
    decl.loc  = peek().loca;
    decl.name = consume(TokenKind::Identifier, "Ожидается имя пространства имён").lexeme;
    consume(TokenKind::Colon, "Ожидается ':' после имени пространства имён");
    consume(TokenKind::Newline, "Ожидается новая строка после ':'");
    skip_newlines();

    while (!check(TokenKind::Semicolon) && !is_at_end()) {
        if (check(TokenKind::KwFun))
            decl.functions.push_back(parse_function_decl());
        else {
            const Token& t = peek();
            std::cerr << t.loca.line << ":" << t.loca.col
                      << ": error: only function declarations allowed inside namespace\n";
            throw std::runtime_error("parse error");
        }
        skip_newlines();
    }
    consume(TokenKind::Semicolon, "Ожидается ';' чтобы закрыть пространство имён");
    return decl;
}

// ── Блок ─────────────────────────────────────────────────────────────────────

std::vector<std::unique_ptr<Instr>> Parser::parse_block() {
    consume(TokenKind::Colon, "Ожидается ':' чтобы открыть блок");
    consume(TokenKind::Newline, "Ожидается новая строка после ':'");

    std::vector<std::unique_ptr<Instr>> body;
    skip_newlines();
    while (!check(TokenKind::Semicolon) && !is_at_end()) {
        body.push_back(parse_instr());
        skip_newlines();
    }
    consume(TokenKind::Semicolon, "Ожидается ';'");
    return body;
}

// ── Инструкции ───────────────────────────────────────────────────────────────

std::unique_ptr<Instr> Parser::parse_instr() {
    if (match(TokenKind::KwLet))    return parse_var_decl(false);
    if (match(TokenKind::KwVar))    return parse_var_decl(true);
    if (check(TokenKind::KwIf))     return parse_if();
    if (check(TokenKind::KwWhile))  return parse_while();
    if (check(TokenKind::KwReturn)) return parse_return();

    if (match(TokenKind::KwBreak)) {
        Location loc = previous().loca;
        consume_newline("Ожидается переход после 'break'");
        return std::make_unique<BreakInstr>(loc);
    }
    if (match(TokenKind::KwContinue)) {
        Location loc = previous().loca;
        consume_newline("Ожидается переход после 'continue'");
        return std::make_unique<ContinueInstr>(loc);
    }

    auto expr    = parse_expr();
    Location loc = expr->loc;

    if (match(TokenKind::Assign)) {
        auto value = parse_expr();
        consume_newline("Ожидается переход после assignment");
        return std::make_unique<AssignInstr>(std::move(expr), std::move(value), loc);
    }

    consume_newline("Ожидается переход после expression");
    return std::make_unique<ExprInstr>(std::move(expr), loc);
}

// let / var
std::unique_ptr<Instr> Parser::parse_var_decl(bool is_var) {
    Location loc = previous().loca;

    std::string explicit_type;
    if (check(TokenKind::Identifier) || check(TokenKind::KwUnit)) {
        std::size_t saved      = pos_;
        std::string maybe_type = parse_type_name();
        if (check(TokenKind::Identifier))
            explicit_type = std::move(maybe_type);
        else
            pos_ = saved;
    }

    std::string name = consume(TokenKind::Identifier, "ожидает variable name").lexeme;
    consume(TokenKind::Assign, "ожидает '=' после variable name");
    auto init = parse_expr();
    consume_newline("ожидает переход после variable declaration");

    if (is_var) return std::make_unique<VarInstr>(name, explicit_type, std::move(init), loc);
    return std::make_unique<LetInstr>(name, explicit_type, std::move(init), loc);
}

// if / else
std::unique_ptr<Instr> Parser::parse_if() {
    consume(TokenKind::KwIf, "ожидает 'if'");
    Location loc = previous().loca;
    auto condition = parse_expr();

    consume(TokenKind::Colon, "ожидает ':' после if condition");
    consume(TokenKind::Newline, "ожидает переход after ':'");

    // then-ветка заканчивается на ';' или 'else'
    std::vector<std::unique_ptr<Instr>> then_branch;
    skip_newlines();
    while (!check(TokenKind::Semicolon) && !check(TokenKind::KwElse) && !is_at_end()) {
        then_branch.push_back(parse_instr());
        skip_newlines();
    }

    std::vector<std::unique_ptr<Instr>> else_branch;
    if (match(TokenKind::KwElse)) {
        consume(TokenKind::Colon, "ожидает ':' после 'else'");
        consume(TokenKind::Newline, "ожидает переход after ':'");
        skip_newlines();
        while (!check(TokenKind::Semicolon) && !is_at_end()) {
            else_branch.push_back(parse_instr());
            skip_newlines();
        }
    }

    consume(TokenKind::Semicolon, "ожидает ';' to close if");
    return std::make_unique<IfInstr>(
        std::move(condition), std::move(then_branch), std::move(else_branch), loc);
}

// while
std::unique_ptr<Instr> Parser::parse_while() {
    consume(TokenKind::KwWhile, "ожидает 'while'");
    Location loc   = previous().loca;
    auto condition = parse_expr();
    auto body      = parse_block();
    return std::make_unique<WhileInstr>(std::move(condition), std::move(body), loc);
}

// return
std::unique_ptr<Instr> Parser::parse_return() {
    consume(TokenKind::KwReturn, "ожидает 'return'");
    Location loc = previous().loca;
    std::unique_ptr<Expr> value;
    if (!check(TokenKind::Newline) && !is_at_end())
        value = parse_expr();
    consume_newline("ожидает перезод после 'return'");
    return std::make_unique<ReturnInstr>(std::move(value), loc);
}

// ── Выражения ────────────────────────────────────────────────────────────────

std::unique_ptr<Expr> Parser::parse_expr() { return parse_pipeline(); }

// [Доп A.1.9] Конвейерный оператор: x |> f  →  f(x),  x |> f(y)  →  f(x, y)
std::unique_ptr<Expr> Parser::parse_pipeline() {
    auto expr = parse_cast();
    while (match(TokenKind::PipeGreater)) {
        Location    loc  = previous().loca;
        std::string name = consume(TokenKind::Identifier,
                                   "ожидается имя функции после '|>'").lexeme;
        std::vector<std::unique_ptr<Expr>> args;
        args.push_back(std::move(expr));
        if (match(TokenKind::LParen)) {
            if (!check(TokenKind::RParen)) {
                do { args.push_back(parse_expr()); } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RParen, "ожидается ')' после аргументов");
        }
        expr = std::make_unique<CallExpr>(std::move(name), std::move(args), loc);
    }
    return expr;
}

// as (приведение типа)
std::unique_ptr<Expr> Parser::parse_cast() {
    auto expr = parse_logical_or();
    if (match(TokenKind::KwAs)) {
        Location    loc = previous().loca;
        std::string tgt = parse_type_name();
        return std::make_unique<CastExpr>(std::move(expr), std::move(tgt), loc);
    }
    return expr;
}

// or / and / not
std::unique_ptr<Expr> Parser::parse_logical_or() {
    auto expr = parse_logical_and();
    while (match(TokenKind::KwOr)) {
        Token op    = previous();
        auto  right = parse_logical_and();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_logical_and() {
    auto expr = parse_bitwise_or();
    while (match(TokenKind::KwAnd)) {
        Token op    = previous();
        auto  right = parse_bitwise_or();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

// [Доп A.1.8] Битовые операции: | ^ & << >> ~
std::unique_ptr<Expr> Parser::parse_bitwise_or() {
    auto expr = parse_bitwise_xor();
    while (match(TokenKind::Pipe)) {
        Token op    = previous();
        auto  right = parse_bitwise_xor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_bitwise_xor() {
    auto expr = parse_bitwise_and();
    while (match(TokenKind::Caret)) {
        Token op    = previous();
        auto  right = parse_bitwise_and();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_bitwise_and() {
    auto expr = parse_equality();
    while (match(TokenKind::Ampersand)) {
        Token op    = previous();
        auto  right = parse_equality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

// == / !=
std::unique_ptr<Expr> Parser::parse_equality() {
    auto expr = parse_comparison();
    while (match(TokenKind::EqualEqual) || match(TokenKind::BangEqual)) {
        Token op    = previous();
        auto  right = parse_comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

// < / <= / > / >=
std::unique_ptr<Expr> Parser::parse_comparison() {
    auto expr = parse_shift();
    while (match(TokenKind::Less)    || match(TokenKind::LessEqual) ||
           match(TokenKind::Greater) || match(TokenKind::GreaterEqual)) {
        Token op    = previous();
        auto  right = parse_shift();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

// << / >> (битовые сдвиги)
std::unique_ptr<Expr> Parser::parse_shift() {
    auto expr = parse_term();
    while (match(TokenKind::LessLess) || match(TokenKind::GreaterGreater)) {
        Token op    = previous();
        auto  right = parse_term();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

// + / - (сложение, вычитание)
std::unique_ptr<Expr> Parser::parse_term() {
    auto expr = parse_factor();
    while (match(TokenKind::Plus) || match(TokenKind::Minus)) {
        Token op    = previous();
        auto  right = parse_factor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

// * / / / % (умножение, деление, остаток)
std::unique_ptr<Expr> Parser::parse_factor() {
    auto expr = parse_unary();
    while (match(TokenKind::Star) || match(TokenKind::Slash) || match(TokenKind::Percent)) {
        Token op    = previous();
        auto  right = parse_unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.kind, std::move(right), op.loca);
    }
    return expr;
}

// унарный - / not / ~ (битовое NOT)
std::unique_ptr<Expr> Parser::parse_unary() {
    if (match(TokenKind::Minus) || match(TokenKind::KwNot) || match(TokenKind::Tilde)) {
        Token op    = previous();
        auto  right = parse_unary();
        return std::make_unique<UnaryExpr>(op.kind, std::move(right), op.loca);
    }
    return parse_postfix();
}

// постфиксные операции: вызов(), индекс[], поле., namespace::
std::unique_ptr<Expr> Parser::parse_postfix() {
    auto expr = parse_primary();

    while (true) {
        if (match(TokenKind::LParen)) {
            Location loc = previous().loca;
            auto     args = parse_arg_list();
            consume(TokenKind::RParen, "ожидает ')' после arguments");
            expr = std::make_unique<CallExpr>(flatten_callee(expr.get()),
                                               std::move(args), loc);
        } else if (match(TokenKind::LBracket)) {
            Location loc   = previous().loca;
            auto     index = parse_expr();
            consume(TokenKind::RBracket, "ожидает ']' после index");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index), loc);
        } else if (match(TokenKind::Dot)) {
            Location    loc   = previous().loca;
            std::string field = consume(TokenKind::Identifier, "ожидает field name").lexeme;
            expr = std::make_unique<FieldExpr>(std::move(expr), std::move(field), loc);
        } else if (match(TokenKind::DoubleColon)) {
            Location    loc    = previous().loca;
            std::string member = consume(TokenKind::Identifier, "ожидает member name after '::'").lexeme;
            expr = std::make_unique<NamespaceAccessExpr>(std::move(expr), std::move(member), loc);
        } else {
            break;
        }
    }
    return expr;
}

// первичные выражения: литералы int, float, string, bool, unit, array, struct, идентификатор
std::unique_ptr<Expr> Parser::parse_primary() {
    if (match(TokenKind::IntLiteral))
        return std::make_unique<NumLitExpr>(previous().lexeme, previous().loca);

    if (match(TokenKind::FloatLiteral))
        return std::make_unique<FloatLitExpr>(previous().lexeme, previous().loca);

    if (match(TokenKind::StringLiteral))
        return std::make_unique<StringLitExpr>(previous().lexeme, previous().loca);

    if (match(TokenKind::KwTrue))
        return std::make_unique<BoolLitExpr>(true, previous().loca);

    if (match(TokenKind::KwFalse))
        return std::make_unique<BoolLitExpr>(false, previous().loca);

    if (match(TokenKind::KwUnit))
        return std::make_unique<UnitLitExpr>(previous().loca);

    if (match(TokenKind::LBracket)) {
        Location loc = previous().loca;
        std::vector<std::unique_ptr<Expr>> elements;
        if (!check(TokenKind::RBracket)) {
            do { elements.push_back(parse_expr()); } while (match(TokenKind::Comma));
        }
        consume(TokenKind::RBracket, "expected ']' after array elements");
        return std::make_unique<ArrayLitExpr>(std::move(elements), loc);
    }

    if (match(TokenKind::Identifier)) {
        Token name = previous();
        if (match(TokenKind::LBrace)) {
            std::vector<StructFieldInit> fields;
            skip_newlines();
            if (!check(TokenKind::RBrace)) {
                do {
                    skip_newlines();
                    StructFieldInit fi;
                    fi.name  = consume(TokenKind::Identifier, "ожидает field name").lexeme;
                    consume(TokenKind::Colon, "ожидает ':' после field name");
                    fi.value = parse_expr();
                    fields.push_back(std::move(fi));
                } while (match(TokenKind::Comma));
            }
            skip_newlines();
            consume(TokenKind::RBrace, "ожидает '}' после struct literal");
            return std::make_unique<StructLitExpr>(name.lexeme, std::move(fields), name.loca);
        }
        return std::make_unique<IdentifExpr>(name.lexeme, name.loca);
    }

    // [Доп A.1.11] Мета-функции: sizeof(…), typeid(…), typeof(…)
    if (match(TokenKind::KwSizeof) || match(TokenKind::KwTypeid) || match(TokenKind::KwTypeof)) {
        Token kw  = previous();
        Location loc = kw.loca;
        consume(TokenKind::LParen, "ожидает '(' после '" + kw.lexeme + "'");
        auto operand = parse_expr();
        consume(TokenKind::RParen, "ожидает ')' после argument");
        if (kw.kind == TokenKind::KwSizeof)
            return std::make_unique<SizeofExpr>(std::move(operand), loc);
        if (kw.kind == TokenKind::KwTypeid)
            return std::make_unique<TypeidExpr>(std::move(operand), loc);
        return std::make_unique<TypeofExpr>(std::move(operand), loc);
    }

    if (match(TokenKind::LParen)) {
        auto expr = parse_expr();
        consume(TokenKind::RParen, "ожидает ')' после expression");
        return expr;
    }

    const Token& t = peek();
    std::cerr << t.loca.line << ":" << t.loca.col << ": error: ожидает expression\n";
    throw std::runtime_error("ошибка парсера");
}

std::vector<std::unique_ptr<Expr>> Parser::parse_arg_list() {
    std::vector<std::unique_ptr<Expr>> args;
    if (!check(TokenKind::RParen)) {
        do { args.push_back(parse_expr()); } while (match(TokenKind::Comma));
    }
    return args;
}
