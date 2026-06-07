#include <fstream>
#include <iostream>
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

// ── Точка входа CLI ───────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    // Аргументы командной строки
    if (argc < 2) {
        std::cerr << "usage: cyton <source file> [--dump-tokens] [--dump-ast]\n";
        return 1;
    }

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

    // Лексический анализ
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

    // Синтаксический анализ → семантический анализ → интерпретация
    try {
        Parser parser(tokens);
        Program program = parser.parse_program();

        if (dump_ast) {
            std::cout << "AST - нормально\n";
            std::cout << "top-level items: " << program.items.size() << "\n";
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
