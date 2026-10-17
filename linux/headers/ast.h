#ifndef AST_H
#define AST_H

typedef enum {
    AST_PROGRAM,
    AST_IMPORT,
    AST_NAMESPACE,
    AST_USING,
    AST_STRUCT,
    AST_STRUCT_FIELD,
    AST_UNION,
    AST_TYPEDEF,
    AST_ENUM,
    AST_ENUM_ITEM,
    AST_FUNCTION_DECLARATION,
    AST_PARAM,
    AST_BLOCK,
    AST_VAR_DECLARATION,
    AST_ASSIGNMENT,
    AST_RETURN_STATEMENT,
    AST_EXPR_STATEMENT,
    AST_CALL_EXPR,
    AST_UNARY_EXPR,
    AST_BINARY_EXPR,
    AST_MEMBER_EXPR,
    AST_INDEX_EXPR,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_DO_WHILE,
    AST_BREAK,
    AST_CONTINUE,
    AST_SWITCH,
    AST_CASE,
    AST_DEFAULT,
    AST_TERNARY,
    AST_COMPOUND_ASSIGNMENT,
    AST_INC_DEC,
    AST_BOOLEAN,
    AST_IDENTIFIER,
    AST_SCOPED_IDENTIFIER,
    AST_NUMBER,
    AST_STRING,
    AST_NULL,
    AST_CLASS,
    AST_THIS,
    AST_INLINE_ASM,
    AST_INLINE_C
} ASTNodeType;

typedef enum {
    ACCESS_PRIVATE = 0,
    ACCESS_PUBLIC = 1,
    ACCESS_PROTECTED = 2
} AccessSpecifier;

typedef struct ASTNode {
    ASTNodeType type;
    int line;
    struct ASTNode* next; // Para encadenar listas (declaraciones, sentencias, params, campos, args)

    char* name;             // Nombre (función, variable, struct, namespace, campo, identificador)
    char* type_name;        // Tipo: "int", "void", "fundamental::string", etc.
    char* namespace_prefix; // Namespace al que pertenece la función/declaración
    char* base_class;       // Nombre de la clase base para herencia (si aplica)
    int access_level;       // AccessSpecifier: 0 = private, 1 = public, 2 = protected
    int is_constructor;     // 1 si es constructor
    int is_destructor;      // 1 si es destructor

    // Hijos y listas
    struct ASTNode* children;     // Declaraciones en Program o Namespace
    struct ASTNode* fields;       // Campos en Struct / Class
    struct ASTNode* params;       // Parámetros en Function
    struct ASTNode* body;         // Bloque de la función, if, while, for, o métodos de class
    struct ASTNode* else_branch;  // Rama else en AST_IF
    struct ASTNode* statements;   // Sentencias dentro de Block
    struct ASTNode* init_value;   // Valor inicial de variable o asignación (o init en for)
    struct ASTNode* update_expr;  // Actualización en bucle for (ej. i++)
    struct ASTNode* return_value; // Expresión de return
    struct ASTNode* expr;         // Expresión en EXPR_STATEMENT, condición de if/while/for, o índice en array[idx]

    // Operaciones y llamadas
    struct ASTNode* callee;       // Función invocada (Identifier o ScopedIdentifier)
    struct ASTNode* args;         // Lista de argumentos
    struct ASTNode* left;         // Operando izquierdo o base en expr.member / expr[idx]
    struct ASTNode* right;        // Operando derecho
    char op;                      // '+', '-', '*', '/', '%', '<', '>'
    char op_str[8];               // "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "++", "--", etc.

    // Literales y arreglos
    int int_value;                // Valor entero, booleano (0/1), o is_postfix en inc_dec
    char* string_value;
    int is_array;                 // 1 si es declaración de arreglo
    int array_size;               // Tamaño del arreglo en elementos
    int is_inline;                // 1 si la función está marcada con inline
    int inline_slot_offset;       // Offset en la pila para llamadas inline
    int is_const;                 // 1 si es constante (const)
    int is_extern;                // 1 si está marcado con extern
    int is_static;                // 1 si está marcado con static
} ASTNode;

ASTNode* create_ast_node(ASTNodeType type, int line);

#endif // AST_H
