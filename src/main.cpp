#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

import cyton.token;
import cyton.ast;
import cyton.lexer;
import cyton.parser;
import cyton.semantic;
import cyton.interpreter;

// ── Чтение исходного файла ────────────────────────────────────────────────────

static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("не может открыть файл" + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ── AST Printer ───────────────────────────────────────────────────────────────

static void print_expr (const Expr&  expr,  const std::string& pfx, bool last);
static void print_instr(const Instr& instr, const std::string& pfx, bool last);

static std::string child_pfx(const std::string& pfx, bool last) {
    return pfx + (last ? "    " : "│   ");
}
static void tree_line(const std::string& pfx, bool last, const std::string& text) {
    std::cout << pfx << (last ? "└── " : "├── ") << text << "\n";
}

static std::string op_str(TokenKind k) {
    switch (k) {
        case TokenKind::Plus:           return "+";
        case TokenKind::Minus:          return "-";
        case TokenKind::Star:           return "*";
        case TokenKind::Slash:          return "/";
        case TokenKind::Percent:        return "%";
        case TokenKind::EqualEqual:     return "==";
        case TokenKind::BangEqual:      return "!=";
        case TokenKind::Less:           return "<";
        case TokenKind::LessEqual:      return "<=";
        case TokenKind::Greater:        return ">";
        case TokenKind::GreaterEqual:   return ">=";
        case TokenKind::KwAnd:          return "and";
        case TokenKind::KwOr:           return "or";
        case TokenKind::KwNot:          return "not";
        case TokenKind::Ampersand:      return "&";  // A.1.8
        case TokenKind::Pipe:           return "|";  // A.1.8
        case TokenKind::Caret:          return "^";  // A.1.8
        case TokenKind::Tilde:          return "~";  // A.1.8
        case TokenKind::LessLess:       return "<<"; // A.1.8
        case TokenKind::GreaterGreater: return ">>"; // A.1.8
        default:                        return "?";
    }
}

static void print_exprs(const std::vector<std::unique_ptr<Expr>>& v, const std::string& pfx) {
    for (std::size_t i = 0; i < v.size(); ++i)
        print_expr(*v[i], pfx, i + 1 == v.size());
}

static void print_instrs(const std::vector<std::unique_ptr<Instr>>& v, const std::string& pfx) {
    for (std::size_t i = 0; i < v.size(); ++i)
        print_instr(*v[i], pfx, i + 1 == v.size());
}

static void print_expr(const Expr& expr, const std::string& pfx, bool last) {
    switch (expr.type) {
        case ExprType::NumLit:
            tree_line(pfx, last, "NumLit " + static_cast<const NumLitExpr&>(expr).value);
            break;
        case ExprType::FloatLit:
            tree_line(pfx, last, "FloatLit " + static_cast<const FloatLitExpr&>(expr).value);
            break;
        case ExprType::StringLit:
            tree_line(pfx, last, "StringLit \"" + static_cast<const StringLitExpr&>(expr).value + "\"");
            break;
        case ExprType::BoolLit:
            tree_line(pfx, last, std::string("BoolLit ") +
                      (static_cast<const BoolLitExpr&>(expr).value ? "true" : "false"));
            break;
        case ExprType::UnitLit:
            tree_line(pfx, last, "UnitLit");
            break;
        case ExprType::Identif:
            tree_line(pfx, last, "Identif " + static_cast<const IdentifExpr&>(expr).name);
            break;
        case ExprType::Unary: {
            const auto& n = static_cast<const UnaryExpr&>(expr);
            tree_line(pfx, last, "Unary [" + op_str(n.op) + "]");
            print_expr(*n.operand, child_pfx(pfx, last), true);
            break;
        }
        case ExprType::Binary: {
            const auto& n = static_cast<const BinaryExpr&>(expr);
            tree_line(pfx, last, "Binary [" + op_str(n.op) + "]");
            std::string cp = child_pfx(pfx, last);
            print_expr(*n.left,  cp, false);
            print_expr(*n.right, cp, true);
            break;
        }
        case ExprType::Call: {
            const auto& n = static_cast<const CallExpr&>(expr);
            std::size_t ac = n.args.size();
            tree_line(pfx, last, "Call " + n.callee +
                      " (" + std::to_string(ac) + " arg" + (ac == 1 ? "" : "s") + ")");
            print_exprs(n.args, child_pfx(pfx, last));
            break;
        }
        case ExprType::Index: {
            const auto& n = static_cast<const IndexExpr&>(expr);
            tree_line(pfx, last, "Index");
            std::string cp = child_pfx(pfx, last);
            print_expr(*n.object, cp, false);
            print_expr(*n.index,  cp, true);
            break;
        }
        case ExprType::Field: {
            const auto& n = static_cast<const FieldExpr&>(expr);
            tree_line(pfx, last, "Field ." + n.field);
            print_expr(*n.object, child_pfx(pfx, last), true);
            break;
        }
        case ExprType::NamespaceAccess: {
            const auto& n = static_cast<const NamespaceAccessExpr&>(expr);
            tree_line(pfx, last, "NamespaceAccess ::" + n.member);
            print_expr(*n.object, child_pfx(pfx, last), true);
            break;
        }
        case ExprType::Cast: {
            const auto& n = static_cast<const CastExpr&>(expr);
            tree_line(pfx, last, "Cast -> " + n.target_type);
            print_expr(*n.expr, child_pfx(pfx, last), true);
            break;
        }
        case ExprType::ArrayLit: {
            const auto& n = static_cast<const ArrayLitExpr&>(expr);
            tree_line(pfx, last, "ArrayLit [" + std::to_string(n.elements.size()) + " elem]");
            print_exprs(n.elements, child_pfx(pfx, last));
            break;
        }
        case ExprType::StructLit: {
            const auto& n = static_cast<const StructLitExpr&>(expr);
            tree_line(pfx, last, "StructLit " + n.type_name);
            std::string cp = child_pfx(pfx, last);
            for (std::size_t i = 0; i < n.fields.size(); ++i) {
                bool fl = i + 1 == n.fields.size();
                const auto& f = n.fields[i];
                tree_line(cp, fl, "." + f.name + ":");
                print_expr(*f.value, child_pfx(cp, fl), true);
            }
            break;
        }
        // [Доп A.1.11] Мета-функции
        case ExprType::Sizeof:
            tree_line(pfx, last, "Sizeof");
            print_expr(*static_cast<const SizeofExpr&>(expr).operand, child_pfx(pfx, last), true);
            break;
        case ExprType::Typeid:
            tree_line(pfx, last, "Typeid");
            print_expr(*static_cast<const TypeidExpr&>(expr).operand, child_pfx(pfx, last), true);
            break;
        case ExprType::Typeof:
            tree_line(pfx, last, "Typeof");
            print_expr(*static_cast<const TypeofExpr&>(expr).operand, child_pfx(pfx, last), true);
            break;
    }
}

static void print_instr(const Instr& instr, const std::string& pfx, bool last) {
    switch (instr.type) {
        case InstrType::Let: {
            const auto& n = static_cast<const LetInstr&>(instr);
            std::string lbl = "Let " + n.name;
            if (!n.explicit_type.empty()) lbl += " : " + n.explicit_type;
            tree_line(pfx, last, lbl);
            print_expr(*n.init, child_pfx(pfx, last), true);
            break;
        }
        case InstrType::Var: {
            const auto& n = static_cast<const VarInstr&>(instr);
            std::string lbl = "Var " + n.name;
            if (!n.explicit_type.empty()) lbl += " : " + n.explicit_type;
            tree_line(pfx, last, lbl);
            print_expr(*n.init, child_pfx(pfx, last), true);
            break;
        }
        case InstrType::Expr:
            tree_line(pfx, last, "ExprStmt");
            print_expr(*static_cast<const ExprInstr&>(instr).expr, child_pfx(pfx, last), true);
            break;
        case InstrType::Assign: {
            const auto& n = static_cast<const AssignInstr&>(instr);
            tree_line(pfx, last, "Assign");
            std::string cp = child_pfx(pfx, last);
            print_expr(*n.target, cp, false);
            print_expr(*n.value,  cp, true);
            break;
        }
        case InstrType::If: {
            const auto& n = static_cast<const IfInstr&>(instr);
            bool has_else = !n.else_branch.empty();
            tree_line(pfx, last, "If");
            std::string cp = child_pfx(pfx, last);
            // condition — не последний, если есть then/else
            print_expr(*n.condition, cp, false);
            {
                bool then_last = !has_else;
                std::size_t ts = n.then_branch.size();
                tree_line(cp, then_last, "Then (" + std::to_string(ts) +
                          " stmt" + (ts == 1 ? "" : "s") + ")");
                print_instrs(n.then_branch, child_pfx(cp, then_last));
            }
            if (has_else) {
                std::size_t es = n.else_branch.size();
                tree_line(cp, true, "Else (" + std::to_string(es) +
                          " stmt" + (es == 1 ? "" : "s") + ")");
                print_instrs(n.else_branch, child_pfx(cp, true));
            }
            break;
        }
        case InstrType::While: {
            const auto& n = static_cast<const WhileInstr&>(instr);
            tree_line(pfx, last, "While");
            std::string cp = child_pfx(pfx, last);
            print_expr(*n.condition, cp, false);
            std::size_t bs = n.body.size();
            tree_line(cp, true, "Body (" + std::to_string(bs) +
                      " stmt" + (bs == 1 ? "" : "s") + ")");
            print_instrs(n.body, child_pfx(cp, true));
            break;
        }
        case InstrType::Return: {
            const auto& n = static_cast<const ReturnInstr&>(instr);
            tree_line(pfx, last, "Return");
            if (n.value)
                print_expr(*n.value, child_pfx(pfx, last), true);
            break;
        }
        case InstrType::Break:
            tree_line(pfx, last, "Break");
            break;
        case InstrType::Continue:
            tree_line(pfx, last, "Continue");
            break;
        case InstrType::Block: {
            const auto& n = static_cast<const BlockInstr&>(instr);
            std::size_t bs = n.body.size();
            tree_line(pfx, last, "Block (" + std::to_string(bs) + " stmt" +
                      (bs == 1 ? "" : "s") + ")");
            print_instrs(n.body, child_pfx(pfx, last));
            break;
        }
    }
}

static void print_fn_decl(const FunctionDecl& fn, const std::string& pfx, bool last) {
    std::string sig = fn.name + " (";
    for (std::size_t i = 0; i < fn.params.size(); ++i) {
        if (i) sig += ", ";
        sig += fn.params[i].type_name + " " + fn.params[i].name;
    }
    sig += ") -> " + (fn.return_type.empty() ? "unit" : fn.return_type);
    tree_line(pfx, last, "FunctionDecl " + sig);
    print_instrs(fn.body, child_pfx(pfx, last));
}

static void print_ast(const Program& program) {
    std::size_t n = program.items.size();
    std::cout << "Program (" << n << " item" << (n == 1 ? "" : "s") << ")\n";

    for (std::size_t i = 0; i < program.items.size(); ++i) {
        bool last = i + 1 == program.items.size();
        const TopItem& item = program.items[i];

        switch (item.kind) {
            case TopItem::Kind::Function:
                print_fn_decl(item.function, "", last);
                break;
            case TopItem::Kind::Struct: {
                const auto& s = item.structure;
                tree_line("", last, "StructDecl " + s.name);
                std::string cp = child_pfx("", last);
                for (std::size_t fi = 0; fi < s.fields.size(); ++fi) {
                    bool fl = fi + 1 == s.fields.size();
                    tree_line(cp, fl, s.fields[fi].type_name + " " + s.fields[fi].name);
                }
                break;
            }
            case TopItem::Kind::TypeAlias: {
                const auto& ta = item.type_alias;
                tree_line("", last, "TypeAlias " + ta.name + " = " + ta.target);
                break;
            }
            case TopItem::Kind::Namespace: {
                const auto& ns = item.ns;
                tree_line("", last, "Namespace " + ns.name);
                std::string cp = child_pfx("", last);
                for (std::size_t fi = 0; fi < ns.functions.size(); ++fi)
                    print_fn_decl(ns.functions[fi], cp, fi + 1 == ns.functions.size());
                break;
            }
            case TopItem::Kind::Instr:
                print_instr(*item.instr, "", last);
                break;
        }
    }
}

// ── [Доп B.2.4] Интерактивный режим (REPL) с сохранением состояния ───────────

static int block_depth(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    int depth = 0;
    for (const auto& tok : tokens) {
        if (tok.kind == TokenKind::Colon)     ++depth;
        if (tok.kind == TokenKind::Semicolon) --depth;
    }
    return depth < 0 ? 0 : depth;
}

static int run_repl() {
    std::cout << "Cyton REPL  (Ctrl+D / \"exit\" для выхода)\n";

    SemanticAnalyzer semantic("<repl>");
    Interpreter      interpreter("<repl>");
    semantic.init_repl();
    interpreter.init_repl();

    std::vector<Program> programs;
    std::string buffer;

    for (;;) {
        std::cout << (block_depth(buffer) == 0 ? ">> " : ".. ") << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << "\n"; break; }

        if (buffer.empty()) {
            if (line.empty()) continue;
            if (line == "exit" || line == "quit") break;
        }

        buffer += line + "\n";

        if (block_depth(buffer) > 0) continue;

        try {
            Lexer  lexer(buffer);
            auto   tokens = lexer.tokenize();

            bool lex_ok = true;
            for (const auto& tok : tokens) {
                if (tok.kind == TokenKind::Invalid) {
                    std::cerr << tok.loca.line << ":" << tok.loca.col
                              << ": error: " << tok.lexeme << "\n";
                    lex_ok = false;
                }
            }

            if (lex_ok) {
                programs.push_back(Parser(tokens).parse_program());
                const Program& prog = programs.back();
                if (semantic.analyze_chunk(prog))
                    interpreter.exec_chunk(prog);
            }
        } catch (const std::exception&) {
            if (!programs.empty() && programs.back().items.empty())
                programs.pop_back();
        }

        buffer.clear();
    }
    return 0;
}

// ── Точка входа CLI ───────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) return run_repl();
    if (argc == 2 && std::string(argv[1]) == "--repl") return run_repl();

    std::string source_path = argv[1];
    bool dump_tokens = false;
    bool dump_ast    = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--dump-tokens") dump_tokens = true;
        else if (arg == "--dump-ast")    dump_ast    = true;
        else {
            std::cerr << "Непонятный аргумент: " << arg << "\n";
            return 1;
        }
    }

    std::string source;
    try {
        source = read_file(source_path);
    } catch (const std::exception& e) {
        std::cerr << source_path << ": error: " << e.what() << "\n";
        return 1;
    }

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    if (dump_tokens) {
        for (const Token& token : tokens) {
            std::cout
                << token_kind_to_string(token.kind)
                << " \"" << token.lexeme << "\" "
                << token.loca.line << ":"
                << token.loca.col  << "\n";
        }
        return 0;
    }

    try {
        Parser parser(tokens);
        Program program = parser.parse_program();

        if (dump_ast) {
            print_ast(program);
            return 0;
        }

        SemanticAnalyzer semantic(source_path);
        if (!semantic.analyze(program)) return 1;

        Interpreter interpreter(source_path);
        return interpreter.run(program);

    } catch (const std::exception&) {
        return 1;
    }
}
