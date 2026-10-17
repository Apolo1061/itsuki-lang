#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING,

    // Palabras clave y directivas
    TOKEN_IMPORT,      // #import
    TOKEN_INCLUDE,     // #include
    TOKEN_NAMESPACE,   // namespace
    TOKEN_USING,       // using
    TOKEN_STRUCT,      // struct
    TOKEN_VAR,         // var
    TOKEN_CONST,       // const
    TOKEN_FN,          // fn
    TOKEN_RETURN,      // return
    TOKEN_INT,         // int
    TOKEN_FLOAT,       // float
    TOKEN_DOUBLE,      // double
    TOKEN_INT64,       // int64
    TOKEN_UINT64,      // uint64
    TOKEN_UINT32,      // uint32
    TOKEN_UINT16,      // uint16
    TOKEN_UINT8,       // uint8
    TOKEN_INT16,       // int16
    TOKEN_INT8,        // int8
    TOKEN_VOID,        // void
    TOKEN_CHAR,        // char
    TOKEN_STRING_TYPE, // string
    TOKEN_BOOL,        // bool
    TOKEN_TRUE,        // true
    TOKEN_FALSE,       // false
    TOKEN_NULL,        // NULL o null
    TOKEN_NULLPTR,     // nullptr
    TOKEN_IF,          // if
    TOKEN_ELSE,        // else
    TOKEN_WHILE,       // while
    TOKEN_FOR,         // for
    TOKEN_UNION,       // union
    TOKEN_TYPEDEF,     // typedef
    TOKEN_ENUM,        // enum
    TOKEN_EXTERN,      // extern
    TOKEN_INLINE,      // inline
    TOKEN_STATIC,      // static
    TOKEN_BREAK,       // break
    TOKEN_CONTINUE,    // continue
    TOKEN_DO,          // do
    TOKEN_SWITCH,      // switch
    TOKEN_CASE,        // case
    TOKEN_DEFAULT,     // default
    TOKEN_CLASS,       // class
    TOKEN_PUBLIC,      // public
    TOKEN_PRIVATE,     // private
    TOKEN_PROTECTED,   // protected
    TOKEN_THIS,        // this
    TOKEN_ASM,         // __asm__, asm, __asm
    TOKEN_TEMPLATE,    // template
    TOKEN_TYPENAME,    // typename
    TOKEN_C_CODE,      // __c__, __c_code__, c_code

    // Operadores y símbolos
    TOKEN_COLON_COLON, // ::
    TOKEN_ARROW,       // ->
    TOKEN_COLON,       // :
    TOKEN_SEMICOLON,   // ;
    TOKEN_COMMA,       // ,
    TOKEN_DOT,         // .
    TOKEN_ELLIPSIS,    // ...
    TOKEN_ASSIGN,      // =
    TOKEN_PLUS,        // +
    TOKEN_MINUS,       // -
    TOKEN_STAR,        // *
    TOKEN_SLASH,       // /
    TOKEN_MOD,         // %
    TOKEN_AMPERSAND,   // &
    TOKEN_PIPE,        // |
    TOKEN_CARET,       // ^
    TOKEN_TILDE,       // ~
    TOKEN_QUESTION,    // ?
    TOKEN_BANG,        // !
    TOKEN_LPAREN,      // (
    TOKEN_RPAREN,      // )
    TOKEN_LBRACE,      // {
    TOKEN_RBRACE,      // }
    TOKEN_LBRACKET,    // [
    TOKEN_RBRACKET,    // ]
    TOKEN_LANGLE,      // <
    TOKEN_RANGLE,      // >
    TOKEN_SHL,         // <<
    TOKEN_SHR,         // >>

    // Operadores de comparación y lógicos
    TOKEN_EQ,          // ==
    TOKEN_NEQ,         // !=
    TOKEN_LE,          // <=
    TOKEN_GE,          // >=
    TOKEN_AND,         // &&
    TOKEN_OR,          // ||

    // Incrementos y asignaciones compuestas
    TOKEN_INC,          // ++
    TOKEN_DEC,          // --
    TOKEN_PLUS_ASSIGN,  // +=
    TOKEN_MINUS_ASSIGN, // -=
    TOKEN_STAR_ASSIGN,  // *=
    TOKEN_SLASH_ASSIGN, // /=
    TOKEN_MOD_ASSIGN,   // %=
    TOKEN_AMP_ASSIGN,   // &=
    TOKEN_PIPE_ASSIGN,  // |=
    TOKEN_CARET_ASSIGN, // ^=
    TOKEN_SHL_ASSIGN,   // <<=
    TOKEN_SHR_ASSIGN,   // >>=

    TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int line;
    int column;
} Token;

#endif // TOKEN_H
