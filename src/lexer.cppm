module;
#include <string>
#include <string_view>
#include <vector>

export module cyton.lexer;

import cyton.token;

export class Lexer {
public:
    explicit Lexer(std::string_view source);

    Token next_token();
    std::vector<Token> tokenize();

private:
    std::string_view source_;
    std::size_t pos_    = 0;
    std::size_t line_   = 1;
    std::size_t column_ = 1;

    [[nodiscard]] bool is_at_end() const;
    [[nodiscard]] char peek()      const;
    [[nodiscard]] char peek_next() const;
    char advance();
    bool match(char expected);

    void skip_spaces();
    void skip_comment();

    Token make_token(TokenKind kind, std::size_t start_pos,
                     std::size_t start_line, std::size_t start_column) const;
    Token make_simple_token(TokenKind kind, char ch,
                            std::size_t start_line, std::size_t start_column) const;
    Token make_invalid_token(std::string message,
                             std::size_t start_line, std::size_t start_column) const;

    Token lex_number();
    Token lex_string(std::size_t start_line, std::size_t start_column);
    Token lex_identifier_or_keyword();

    [[nodiscard]] static bool      is_identifier_start(char ch);
    [[nodiscard]] static bool      is_identifier_part(char ch);
    [[nodiscard]] static TokenKind keyword_kind(std::string_view text);
};
