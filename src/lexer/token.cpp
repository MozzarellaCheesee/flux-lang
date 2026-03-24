#include "flux/lexer/token.h"

namespace flux {
    std::string_view token_kind_name(TokenKind kind) {
        switch (kind) {
            case TokenKind::KW_FNC:      return "KW_FNC";
            case TokenKind::KW_LET:      return "KW_LET";
            case TokenKind::KW_RETURN:   return "KW_RETURN";
            case TokenKind::KW_FOR:      return "KW_FOR";
            case TokenKind::KW_WHILE:    return "KW_WHILE";
            case TokenKind::KW_IF:       return "KW_IF";
            case TokenKind::KW_ELSE:     return "KW_ELSE";
            case TokenKind::KW_CONTINUE: return "KW_CONTINUE";
            case TokenKind::KW_BREAK:    return "KW_BREAK";
            case TokenKind::KW_TRUE:     return "KW_TRUE";
            case TokenKind::KW_FALSE:    return "KW_FALSE";
            case TokenKind::KW_MATCH:    return "KW_MATCH";
            case TokenKind::KW_PUB:      return "KW_PUB";
            case TokenKind::KW_STRUCT:   return "KW_STRUCT";
            case TokenKind::KW_CLASS:    return "KW_CLASS";
            case TokenKind::KW_IMPL:     return "KW_IMPL";
            case TokenKind::KW_SELF:     return "KW_SELF";
            case TokenKind::FAT_ARROW:   return "FAT_ARROW";
            case TokenKind::UNDERSCORE:  return "UNDERSCORE";
            // Типы
            case TokenKind::KW_BOOL:     return "KW_BOOL";
            case TokenKind::KW_INT8:     return "KW_INT8";
            case TokenKind::KW_INT16:    return "KW_INT16";
            case TokenKind::KW_INT32:    return "KW_INT32";
            case TokenKind::KW_INT64:    return "KW_INT64";
            case TokenKind::KW_INT128:   return "KW_INT128";
            case TokenKind::KW_UINT8:    return "KW_UINT8";
            case TokenKind::KW_UINT16:   return "KW_UINT16";
            case TokenKind::KW_UINT32:   return "KW_UINT32";
            case TokenKind::KW_UINT64:   return "KW_UINT64";
            case TokenKind::KW_UINT128:  return "KW_UINT128";
            case TokenKind::KW_FLOAT:    return "KW_FLOAT";
            case TokenKind::KW_DOUBLE:   return "KW_DOUBLE";
            case TokenKind::KW_ISIZE_T:  return "KW_ISIZE_T";
            case TokenKind::KW_USIZE_T:  return "KW_USIZE_T";
            case TokenKind::KW_STR:      return "KW_STR";
            case TokenKind::KW_STRING:   return "KW_STRING";
            // Литералы
            case TokenKind::LIT_INT:     return "LIT_INT";
            case TokenKind::LIT_FLOAT:   return "LIT_FLOAT";
            case TokenKind::LIT_STRING:  return "LIT_STRING";
            // Идентификатор
            case TokenKind::IDENT:       return "IDENT";
            // Арифметика
            case TokenKind::PLUS:        return "PLUS";
            case TokenKind::MINUS:       return "MINUS";
            case TokenKind::STAR:        return "STAR";
            case TokenKind::SLASH:       return "SLASH";
            case TokenKind::PERCENT:     return "PERCENT";
            case TokenKind::TILDE:       return "TILDE";
            // Унарные
            case TokenKind::BANG:        return "BANG";
            // Инкремент/декремент
            case TokenKind::PLUS_PLUS:   return "PLUS_PLUS";
            case TokenKind::MINUS_MINUS: return "MINUS_MINUS";
            // Побитовые
            case TokenKind::AMP_AMP:     return "AMP_AMP";
            case TokenKind::PIPE:        return "PIPE";
            case TokenKind::PIPE_PIPE:   return "PIPE_PIPE";
            case TokenKind::CARET:       return "CARET";
            case TokenKind::LSHIFT:      return "LSHIFT";
            case TokenKind::RSHIFT:      return "RSHIFT";
            // Составное присваивание
            case TokenKind::PLUS_EQ:     return "PLUS_EQ";
            case TokenKind::MINUS_EQ:    return "MINUS_EQ";
            case TokenKind::STAR_EQ:     return "STAR_EQ";
            case TokenKind::SLASH_EQ:    return "SLASH_EQ";
            case TokenKind::PERCENT_EQ:  return "PERCENT_EQ";
            // Сравнение
            case TokenKind::EQ_EQ:       return "EQ_EQ";
            case TokenKind::BANG_EQ:     return "BANG_EQ";
            case TokenKind::LT:          return "LT";
            case TokenKind::GT:          return "GT";
            case TokenKind::LT_EQ:       return "LT_EQ";
            case TokenKind::GT_EQ:       return "GT_EQ";
            // Присваивание
            case TokenKind::EQ:          return "EQ";
            // Ссылки
            case TokenKind::AMP:         return "AMP";
            case TokenKind::DOT:         return "DOT";
            // Пунктуация
            case TokenKind::LPAREN:      return "LPAREN";
            case TokenKind::RPAREN:      return "RPAREN";
            case TokenKind::LBRACKET:    return "LBRACKET";
            case TokenKind::RBRACKET:    return "RBRACKET";
            case TokenKind::LBRACE:      return "LBRACE";
            case TokenKind::RBRACE:      return "RBRACE";
            case TokenKind::COLON:       return "COLON";
            case TokenKind::COMMA:       return "COMMA";
            case TokenKind::SEMICOLON:   return "SEMICOLON";
            case TokenKind::ARROW:       return "ARROW";
            case TokenKind::HASH:        return "HASH";
            case TokenKind::COLON_COLON: return "COLON_COLON";
            case TokenKind::QUESTION:    return "QUESTION";
            // Препроцессор
            case TokenKind::PP_IMPORT:     return "PP_IMPORT";
            case TokenKind::PP_IMPORT_SYS: return "PP_IMPORT_SYS";
            // Ключевые слова (продолжение)
            case TokenKind::KW_EXTERN:     return "KW_EXTERN";
            // Служебные
            case TokenKind::END_OF_FILE: return "EOF";
            default:                     return "UNKNOWN";
        }
    }
}
