## Что такое AST?

AST (Abstract Syntax Tree) — дерево, в котором каждый узел представляет синтаксическую конструкцию программы. Он абстрактный, потому что убирает несущественные детали (скобки, точки с запятой, пробелы), оставляя только структуру.

```flux
fnc add(a: int32, b: int32) -> int32 {
    return a + b;
}
```

```
ProgramNode
└── FuncDecl "add"
    ├── params: [ParamDecl "a": int32, ParamDecl "b": int32]
    ├── return_type: PrimitiveType "int32"
    └── body: BlockStmt
        └── ReturnStmt
            └── BinaryExpr "+"
                ├── IdentExpr "a"
                └── IdentExpr "b"
```

## Базовые классы

Все узлы наследуются от одного из трёх базовых классов:

```cpp
// include/flux/ast/ast_node.h

struct ASTNode {       // выражения, операторы, объявления
    SourceLocation loc;
    virtual void accept(ASTVisitor& v) = 0;
    virtual ~ASTNode() = default;
};

struct TypeNode {      // типы: int32, &str, Option<T>...
    SourceLocation loc;
    virtual void accept(ASTVisitor& v) = 0;
};

struct PatternNode {   // паттерны в match: 1, Ok(x), _
    SourceLocation loc;
    virtual void accept(ASTVisitor& v) = 0;
};
```

Разделение на `ASTNode` / `TypeNode` / `PatternNode` отражает смысловые роли:
- `TypeNode` никогда не является выражением и не может стоять там, где ожидается значение
- `PatternNode` встречается только в ветках `match`

Это предотвращает случайное использование узлов не по назначению.

## Управление памятью

Все дочерние узлы хранятся через `std::unique_ptr<ASTNode>`. Это гарантирует:
- Автоматическое освобождение памяти при уничтожении дерева
- Единственный владелец каждого узла (нет разделённого владения)
- Нет утечек памяти

```cpp
struct BinaryExpr : ASTNode {
    std::string              op;
    std::unique_ptr<ASTNode> lhs;
    std::unique_ptr<ASTNode> rhs;
};
```

## Паттерн Visitor

AST использует паттерн **Visitor** для обхода дерева без изменения классов узлов.

```cpp
// include/flux/ast/ast_visitor.h
struct ASTVisitor {
    virtual void visit(IntLiteral&)   = 0;
    virtual void visit(BinaryExpr&)   = 0;
    virtual void visit(FuncDecl&)     = 0;
    // ... для каждого типа узла
};

// Каждый узел реализует accept:
void BinaryExpr::accept(ASTVisitor& v) { v.visit(*this); }
```

Пользователи паттерна:
- `SemanticAnalyzer` — проверяет типы
- `CodeGen` — генерирует C++ (использует свой `expr()`/`gen()`, не Visitor напрямую)
- `ASTPrinter` — выводит дерево в текст для `--print-ast`

**Почему Visitor, а не виртуальные методы в узлах?** Если бы каждый узел имел методы `typecheck()` и `codegen()`, добавление новой операции (например, оптимизатора) потребовало бы изменения всех классов узлов. С Visitor достаточно написать новый класс-посетитель.

---

## Объявления (`include/flux/ast/decl.h`)

### ProgramNode
Корень AST — содержит все объявления верхнего уровня.
```cpp
struct ProgramNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> decls;
};
```

### FuncDecl
```cpp
struct FuncDecl : ASTNode {
    bool   is_pub;      // объявлена как pub
    bool   is_extern;   // объявлена как extern (нет тела)
    bool   has_self;    // первый параметр — self
    std::string name;
    std::vector<std::unique_ptr<ParamDecl>> params;
    std::unique_ptr<TypeNode>   return_type;
    std::unique_ptr<BlockStmt>  body;  // nullptr для extern fnc
};
```

### ParamDecl
```cpp
struct ParamDecl : ASTNode {
    std::string               name;
    std::unique_ptr<TypeNode> type;  // nullptr для self
};
```

### VarDecl
```cpp
struct VarDecl : ASTNode {
    std::string               name;
    std::unique_ptr<TypeNode> type;  // nullptr = вывод типа
    std::unique_ptr<ASTNode>  init;  // nullptr = нет инициализатора
};
```

### StructDecl / ClassDecl
```cpp
struct StructDecl : ASTNode {
    bool   is_pub;
    std::string name;
    std::vector<std::unique_ptr<FieldDecl>> fields;
};

struct ClassDecl : ASTNode {
    bool   is_pub;
    std::string name;
    std::vector<std::unique_ptr<FieldDecl>> fields;
    std::vector<std::unique_ptr<FuncDecl>>  methods;
};
```

### ImplDecl
```cpp
struct ImplDecl : ASTNode {
    std::string                            target;   // имя типа
    std::vector<std::unique_ptr<FuncDecl>> methods;
};
```

### FieldDecl
```cpp
struct FieldDecl : ASTNode {
    bool        is_pub;
    std::string name;
    std::unique_ptr<TypeNode> type;
};
```

---

## Операторы (`include/flux/ast/stmt.h`)

### BlockStmt
```cpp
struct BlockStmt : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> stmts;
};
```

### ReturnStmt
```cpp
struct ReturnStmt : ASTNode {
    std::unique_ptr<ASTNode> value;  // nullptr для bare return
};
```

### IfStmt
```cpp
struct IfStmt : ASTNode {
    std::unique_ptr<ASTNode>   cond;
    std::unique_ptr<BlockStmt> then_block;
    std::unique_ptr<ASTNode>   else_branch;  // IfStmt | BlockStmt | nullptr
};
```

### ForStmt / WhileStmt
```cpp
struct ForStmt : ASTNode {
    std::unique_ptr<ASTNode>   init;  // VarDecl или ExprStmt
    std::unique_ptr<ASTNode>   cond;
    std::unique_ptr<ASTNode>   step;
    std::unique_ptr<BlockStmt> body;
};

struct WhileStmt : ASTNode {
    std::unique_ptr<ASTNode>   cond;
    std::unique_ptr<BlockStmt> body;
};
```

### ExprStmt, BreakStmt, ContinueStmt
Выражение как оператор (`x += 1;`), `break`, `continue`.

---

## Выражения (`include/flux/ast/expr.h`)

### Литералы
```cpp
struct IntLiteral   : ASTNode { int64_t value; };
struct FloatLiteral : ASTNode { double  value; };
struct BoolLiteral  : ASTNode { bool    value; };
struct StrLiteral   : ASTNode { std::string value; };
struct ArrayLiteral : ASTNode { std::vector<std::unique_ptr<ASTNode>> elements; };
```

### Идентификаторы
```cpp
struct IdentExpr : ASTNode { std::string name; };
struct SelfExpr  : ASTNode {};  // self в методах
```

### Операторы
```cpp
struct BinaryExpr : ASTNode {
    std::string op;  // "+", "==", "&&", ...
    std::unique_ptr<ASTNode> lhs, rhs;
};

struct UnaryExpr : ASTNode {
    enum class Op { PreInc, PreDec, PostInc, PostDec,
                    Neg, Not, BitNot, Deref, AddrOf };
    Op op;
    std::unique_ptr<ASTNode> operand;
};

struct AssignExpr : ASTNode {
    std::unique_ptr<ASTNode> target;
    std::string op;  // "=", "+=", ...
    std::unique_ptr<ASTNode> value;
};
```

### Вызовы
```cpp
struct CallExpr : ASTNode {
    std::string callee;
    std::vector<std::unique_ptr<ASTNode>> args;
};

struct MethodCallExpr : ASTNode {
    std::unique_ptr<ASTNode> receiver;
    std::string method;
    std::vector<std::unique_ptr<ASTNode>> args;
};
```

### Доступ к данным
```cpp
struct FieldAccessExpr : ASTNode {
    std::unique_ptr<ASTNode> object;
    std::string field;
};

struct IndexExpr : ASTNode {
    std::unique_ptr<ASTNode> array;
    std::unique_ptr<ASTNode> index;
};
```

### StructInitExpr
```cpp
struct StructInitExpr : ASTNode {
    std::string type_name;  // "Point"
    std::vector<FieldInit> fields;  // { name, value }
};
```

### ImplicitCastExpr
Вставляется семантическим анализатором при неявном приведении типов:
```cpp
struct ImplicitCastExpr : ASTNode {
    std::unique_ptr<ASTNode>  inner;
    std::unique_ptr<TypeNode> target_type;
};
// → в C++: static_cast<target_type>(inner)
```

---

## Типы (`include/flux/ast/type.h`)

```cpp
struct PrimitiveType : TypeNode { std::string name; };  // "int32", "bool"...
struct RefType       : TypeNode { std::unique_ptr<TypeNode> inner; };  // &T
struct UnitType      : TypeNode {};  // ()
struct ArrayType     : TypeNode { std::unique_ptr<TypeNode> element; ... };  // T[]
struct SliceType     : TypeNode { std::unique_ptr<TypeNode> element; };  // []T
struct GenericType   : TypeNode {  // Result<T,E>, Option<T>
    std::string name;
    std::vector<std::unique_ptr<TypeNode>> args;
};
```

---

## Паттерны для match (`include/flux/ast/expr.h`)

```cpp
struct WildcardPattern    : PatternNode {};          // _
struct LiteralPattern     : PatternNode { ... };     // 42, "hello"
struct IdentPattern       : PatternNode { std::string name; };  // x (биндинг)
struct ConstructorPattern : PatternNode {            // Ok(x), Err(e)
    std::string name;
    std::vector<std::unique_ptr<PatternNode>> args;
};

struct MatchArm : ASTNode {
    std::unique_ptr<PatternNode> pattern;
    std::unique_ptr<BlockStmt>   body;
};

struct MatchExpr : ASTNode {
    std::unique_ptr<ASTNode>               subject;
    std::vector<std::unique_ptr<MatchArm>> arms;
};
```

---

- [[Парсер|Как парсер строит AST]]
- [[Семантический анализатор|Как семантический анализатор обходит AST]]
- [[Кодогенератор|Как кодогенератор транслирует AST в C++]]
