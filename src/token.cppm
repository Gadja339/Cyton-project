module;
#include <string>
#include <string_view>

export module cyton.token;

export enum class TokenKind {
    Invalid,    //ошибка
    Eof,        //конец
    Newline,    //перенос строки

    Identifier,     //идентификатор
    IntLiteral,     //целочисленный литерал
    FloatLiteral,   //вещественный литерал
    StringLiteral,  //строки

    //ключевые слова
    KwFun,
    KwLet,
    KwVar,
    KwIf,
    KwElse,
    KwWhile,
    KwBreak,
    KwContinue,
    KwReturn,
    KwTrue,
    KwFalse,
    KwAnd,
    KwOr,
    KwNot,
    KwStruct,
    KwType,
    KwNamespace,
    KwAs,
    KwUnit,

    //символы
    LParen,      // (
    RParen,      // )
    LBracket,    // [
    RBracket,    // ]
    LBrace,      // {
    RBrace,      // }
    Colon,       // :
    Semicolon,   // ;
    Comma,       // ,
    Dot,         // .

    Plus,        // +
    Minus,       // -
    Star,        // *
    Slash,       // /
    Percent,     // %

    Assign,      // =
    EqualEqual,  // ==
    BangEqual,   // !=

    Less,        // <
    LessEqual,   // <=
    Greater,     // >
    GreaterEqual,// >=

    Arrow,       // ->
    DoubleColon  // ::
};

export struct Location {
    std::size_t line = 1;
    std::size_t col  = 1;
};

export struct Token {
    TokenKind kind = TokenKind::Invalid;
    Location  loca{};
    std::string lexeme;
};

export inline std::string_view token_kind_to_string(TokenKind kind) {
    switch (kind) {
        case TokenKind::Invalid:      return "Invalid";
        case TokenKind::Eof:          return "Eof";
        case TokenKind::Newline:      return "Newline";

        case TokenKind::Identifier:   return "Identifier";
        case TokenKind::IntLiteral:   return "IntLiteral";
        case TokenKind::FloatLiteral: return "FloatLiteral";
        case TokenKind::StringLiteral:return "StringLiteral";

        case TokenKind::KwFun:        return "KwFun";
        case TokenKind::KwLet:        return "KwLet";
        case TokenKind::KwVar:        return "KwVar";
        case TokenKind::KwIf:         return "KwIf";
        case TokenKind::KwElse:       return "KwElse";
        case TokenKind::KwWhile:      return "KwWhile";
        case TokenKind::KwBreak:      return "KwBreak";
        case TokenKind::KwContinue:   return "KwContinue";
        case TokenKind::KwReturn:     return "KwReturn";
        case TokenKind::KwTrue:       return "KwTrue";
        case TokenKind::KwFalse:      return "KwFalse";
        case TokenKind::KwAnd:        return "KwAnd";
        case TokenKind::KwOr:         return "KwOr";
        case TokenKind::KwNot:        return "KwNot";
        case TokenKind::KwStruct:     return "KwStruct";
        case TokenKind::KwType:       return "KwType";
        case TokenKind::KwNamespace:  return "KwNamespace";
        case TokenKind::KwAs:         return "KwAs";
        case TokenKind::KwUnit:       return "KwUnit";

        case TokenKind::LParen:       return "LParen";
        case TokenKind::RParen:       return "RParen";
        case TokenKind::LBracket:     return "LBracket";
        case TokenKind::RBracket:     return "RBracket";
        case TokenKind::LBrace:       return "LBrace";
        case TokenKind::RBrace:       return "RBrace";
        case TokenKind::Colon:        return "Colon";
        case TokenKind::Semicolon:    return "Semicolon";
        case TokenKind::Comma:        return "Comma";
        case TokenKind::Dot:          return "Dot";

        case TokenKind::Plus:         return "Plus";
        case TokenKind::Minus:        return "Minus";
        case TokenKind::Star:         return "Star";
        case TokenKind::Slash:        return "Slash";
        case TokenKind::Percent:      return "Percent";

        case TokenKind::Assign:       return "Assign";
        case TokenKind::EqualEqual:   return "EqualEqual";
        case TokenKind::BangEqual:    return "BangEqual";

        case TokenKind::Less:         return "Less";
        case TokenKind::LessEqual:    return "LessEqual";
        case TokenKind::Greater:      return "Greater";
        case TokenKind::GreaterEqual: return "GreaterEqual";

        case TokenKind::Arrow:        return "Arrow";
        case TokenKind::DoubleColon:  return "DoubleColon";
    }
    return "Unknown";
}
