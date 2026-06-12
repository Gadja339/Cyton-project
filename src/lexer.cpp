module;
#include <cctype>
#include <string>
#include <utility>
#include <vector>

module cyton.lexer;

// ── Конструктор ───────────────────────────────────────────────────────────────

Lexer::Lexer(std::string_view source)
    : source_(source) {}

// ── Курсор: навигация по исходному тексту ─────────────────────────────────────

bool Lexer::is_at_end() const {
    return pos_ >= source_.size();
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[pos_];
}

char Lexer::peek_next() const {
    if (pos_ + 1 >= source_.size()) return '\0';
    return source_[pos_ + 1];
}

char Lexer::advance() {
    if (is_at_end()) return '\0';
    char ch = source_[pos_++];
    if (ch == '\n') { ++line_; column_ = 1; }
    else            { ++column_; }
    return ch;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[pos_] != expected) return false;
    advance();
    return true;
}

// ── Пропуск пробелов и комментариев ──────────────────────────────────────────

void Lexer::skip_spaces() {
    while (!is_at_end()) {
        char ch = peek();
        if (ch == ' ' || ch == '\t' || ch == '\r') advance();
        else break;
    }
}

void Lexer::skip_comment() {
    while (!is_at_end() && peek() != '\n') advance();
}

// ── [Доп A.1.12] Блочные комментарии /* … */ с поддержкой вложенности ────────
void Lexer::skip_block_comment() {
    int depth = 1;
    while (!is_at_end() && depth > 0) {
        if (peek() == '/' && peek_next() == '*') {
            advance(); advance();
            ++depth;
        } else if (peek() == '*' && peek_next() == '/') {
            advance(); advance();
            --depth;
        } else {
            advance();
        }
    }
}

// ── Конструирование токенов ───────────────────────────────────────────────────

Token Lexer::make_token(TokenKind kind, std::size_t start_pos,
                        std::size_t start_line, std::size_t start_column) const {
    Token token;
    token.kind   = kind;
    token.lexeme = std::string(source_.substr(start_pos, pos_ - start_pos));
    token.loca   = Location{start_line, start_column};
    return token;
}

Token Lexer::make_simple_token(TokenKind kind, char ch,
                               std::size_t start_line, std::size_t start_column) const {
    Token token;
    token.kind   = kind;
    token.lexeme = std::string(1, ch);
    token.loca   = Location{start_line, start_column};
    return token;
}

Token Lexer::make_invalid_token(std::string message,
                                std::size_t start_line, std::size_t start_column) const {
    Token token;
    token.kind   = TokenKind::Invalid;
    token.lexeme = std::move(message);
    token.loca   = Location{start_line, start_column};
    return token;
}

// ── Классификация символов ────────────────────────────────────────────────────

bool Lexer::is_identifier_start(char ch) {
    unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalpha(uch) || ch == '_';
}

bool Lexer::is_identifier_part(char ch) {
    unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) || ch == '_';
}

// ── Ключевые слова ────────────────────────────────────────────────────────────

TokenKind Lexer::keyword_kind(std::string_view text) {
    if (text == "fun")       return TokenKind::KwFun;
    if (text == "let")       return TokenKind::KwLet;
    if (text == "var")       return TokenKind::KwVar;
    if (text == "if")        return TokenKind::KwIf;
    if (text == "else")      return TokenKind::KwElse;
    if (text == "while")     return TokenKind::KwWhile;
    if (text == "break")     return TokenKind::KwBreak;
    if (text == "continue")  return TokenKind::KwContinue;
    if (text == "return")    return TokenKind::KwReturn;
    if (text == "true")      return TokenKind::KwTrue;
    if (text == "false")     return TokenKind::KwFalse;
    if (text == "and")       return TokenKind::KwAnd;
    if (text == "or")        return TokenKind::KwOr;
    if (text == "not")       return TokenKind::KwNot;
    if (text == "struct")    return TokenKind::KwStruct;
    if (text == "type")      return TokenKind::KwType;
    if (text == "namespace") return TokenKind::KwNamespace;
    if (text == "as")        return TokenKind::KwAs;
    if (text == "unit")      return TokenKind::KwUnit;
    if (text == "sizeof")    return TokenKind::KwSizeof;  // A.1.11
    if (text == "typeid")    return TokenKind::KwTypeid;  // A.1.11
    if (text == "typeof")    return TokenKind::KwTypeof;  // A.1.11
    return TokenKind::Identifier;
}

// ── Числовые литералы (int, float) ───────────────────────────────────────────

Token Lexer::lex_number() {
    std::size_t start_pos    = pos_;
    std::size_t start_line   = line_;
    std::size_t start_column = column_;

    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();

    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek_next()))) {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
        return make_token(TokenKind::FloatLiteral, start_pos, start_line, start_column);
    }

    return make_token(TokenKind::IntLiteral, start_pos, start_line, start_column);
}

// ── Строковые литералы ────────────────────────────────────────────────────────

Token Lexer::lex_string(std::size_t start_line, std::size_t start_column) {
    std::string value;

    while (!is_at_end() && peek() != '"') {
        char ch = advance();

        if (ch == '\n')
            return make_invalid_token("Незавершенно", start_line, start_column);

        if (ch == '\\') {
            if (is_at_end())
                return make_invalid_token("Незавершенно", start_line, start_column);
            char esc = advance();
            switch (esc) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"':  value += '"';  break;
                case '0':  value += '\0'; break;
                default:
                    return make_invalid_token(
                        std::string("Неизвестная последовательность '\\") + esc + "'",
                        start_line, start_column);
            }
        } else {
            value += ch;
        }
    }

    if (is_at_end())
        return make_invalid_token("Незавершенно", start_line, start_column);

    advance(); // closing '"'

    Token token;
    token.kind   = TokenKind::StringLiteral;
    token.lexeme = std::move(value);
    token.loca   = Location{start_line, start_column};
    return token;
}

// ── Идентификаторы и ключевые слова ──────────────────────────────────────────

Token Lexer::lex_identifier_or_keyword() {
    std::size_t start_pos    = pos_;
    std::size_t start_line   = line_;
    std::size_t start_column = column_;

    while (is_identifier_part(peek())) advance();

    std::string_view text = source_.substr(start_pos, pos_ - start_pos);
    TokenKind kind = keyword_kind(text);
    return make_token(kind, start_pos, start_line, start_column);
}

// ── Главный цикл сканирования ─────────────────────────────────────────────────

Token Lexer::next_token() {
    while (true) {
        skip_spaces();

        if (is_at_end()) {
            Token token;
            token.kind   = TokenKind::Eof;
            token.lexeme = "";
            token.loca   = Location{line_, column_};
            return token;
        }

        char ch = peek();

        if (ch == '#') { skip_comment(); continue; }

        if (ch == '\n') {
            std::size_t start_line   = line_;
            std::size_t start_column = column_;
            advance();
            Token token;
            token.kind   = TokenKind::Newline;
            token.lexeme = "\\n";
            token.loca   = Location{start_line, start_column};
            return token;
        }

        if (std::isdigit(static_cast<unsigned char>(ch))) return lex_number();
        if (is_identifier_start(ch))                      return lex_identifier_or_keyword();

        std::size_t start_line   = line_;
        std::size_t start_column = column_;
        std::size_t start_pos    = pos_;

        switch (advance()) {
            case '"': return lex_string(start_line, start_column);

            case '+': return make_simple_token(TokenKind::Plus,    '+', start_line, start_column);
            case '*': return make_simple_token(TokenKind::Star,    '*', start_line, start_column);
            case '/':
                if (match('*')) { skip_block_comment(); continue; } // A.1.12
                return make_simple_token(TokenKind::Slash, '/', start_line, start_column);
            case '%': return make_simple_token(TokenKind::Percent, '%', start_line, start_column);

            case '-':
                if (match('>')) return make_token(TokenKind::Arrow, start_pos, start_line, start_column);
                return make_simple_token(TokenKind::Minus, '-', start_line, start_column);

            case '=':
                if (match('=')) return make_token(TokenKind::EqualEqual, start_pos, start_line, start_column);
                return make_simple_token(TokenKind::Assign, '=', start_line, start_column);

            case '!':
                if (match('=')) return make_token(TokenKind::BangEqual, start_pos, start_line, start_column);
                return make_invalid_token("unexpected character '!'", start_line, start_column);

            case '<':
                if (match('<')) return make_token(TokenKind::LessLess,    start_pos, start_line, start_column); // A.1.8 сдвиг влево
                if (match('=')) return make_token(TokenKind::LessEqual,    start_pos, start_line, start_column);
                return make_simple_token(TokenKind::Less, '<', start_line, start_column);

            case '>':
                if (match('>')) return make_token(TokenKind::GreaterGreater, start_pos, start_line, start_column); // A.1.8 сдвиг вправо
                if (match('=')) return make_token(TokenKind::GreaterEqual,   start_pos, start_line, start_column);
                return make_simple_token(TokenKind::Greater, '>', start_line, start_column);

            // A.1.8 Битовые операции: & | ^ ~
            case '&': return make_simple_token(TokenKind::Ampersand, '&', start_line, start_column);
            case '|':
                if (match('>')) return make_token(TokenKind::PipeGreater, start_pos, start_line, start_column); // A.1.9 конвейер |>
                return make_simple_token(TokenKind::Pipe, '|', start_line, start_column);
            case '^': return make_simple_token(TokenKind::Caret,     '^', start_line, start_column);
            case '~': return make_simple_token(TokenKind::Tilde,     '~', start_line, start_column);

            case ':':
                if (match(':')) return make_token(TokenKind::DoubleColon, start_pos, start_line, start_column);
                return make_simple_token(TokenKind::Colon, ':', start_line, start_column);

            case '(': return make_simple_token(TokenKind::LParen,    '(', start_line, start_column);
            case ')': return make_simple_token(TokenKind::RParen,    ')', start_line, start_column);
            case '[': return make_simple_token(TokenKind::LBracket,  '[', start_line, start_column);
            case ']': return make_simple_token(TokenKind::RBracket,  ']', start_line, start_column);
            case '{': return make_simple_token(TokenKind::LBrace,    '{', start_line, start_column);
            case '}': return make_simple_token(TokenKind::RBrace,    '}', start_line, start_column);
            case ';': return make_simple_token(TokenKind::Semicolon, ';', start_line, start_column);
            case ',': return make_simple_token(TokenKind::Comma,     ',', start_line, start_column);
            case '.': return make_simple_token(TokenKind::Dot,       '.', start_line, start_column);

            default:
                return make_invalid_token(
                    std::string("unexpected character '") + source_[start_pos] + "'",
                    start_line, start_column);
        }
    }
}

// ── Точка входа: полная токенизация ──────────────────────────────────────────

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token token = next_token();
        tokens.push_back(token);
        if (token.kind == TokenKind::Eof || token.kind == TokenKind::Invalid) break;
    }
    return tokens;
}
