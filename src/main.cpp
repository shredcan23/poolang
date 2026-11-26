#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// ============================================================================
// 1. TOKENS & LEXER
// ============================================================================

enum class TokenType {
    // Keywords
    KW_MAKE,
    KW_PRINT,
    KW_IN,
    KW_ASSIGN,
    KW_CONDITION,
    KW_IS,
    KW_NEXT,
    KW_VOID,
    KW_LOOP,
    KW_FOR,
    KW_WHILE,
    KW_FUNC,
    KW_PAR,
    KW_RETURN,
    KW_CALL,
    KW_NOT,
    KW_TRUE,
    KW_FALSE,

    // Literals & Identifiers
    IDENT,
    INT_LIT,
    DEC_LIT,

    // Symbols & Operators
    BANG,       // !
    SEMICOLON,  // ;
    COMMA,      // ,
    COLON,      // :
    LBRACKET,   // [
    RBRACKET,   // ]
    LBRACE,     // {
    RBRACE,     // }
    LANGLE,     // <
    RANGLE,     // >
    LEQ,        // <=
    GEQ,        // >=
    EQ,         // =
    NEQ,        // !=
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    SYMBOL,     // Other punctuation inside unquoted text (e.g., '.', '?')

    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string text;
    int line;
    size_t startPos;
    size_t endPos;
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : source(src), pos(0), line(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;

        while (pos < source.size()) {
            char c = source[pos];

            // Whitespace
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (c == '\n') line++;
                pos++;
                continue;
            }

            // Comments (# to end of line)
            if (c == '#') {
                while (pos < source.size() && source[pos] != '\n') {
                    pos++;
                }
                continue;
            }

            size_t start = pos;

            // Numbers (INT_LIT or DEC_LIT)
            if (std::isdigit(static_cast<unsigned char>(c))) {
                bool isDec = false;
                while (pos < source.size() && std::isdigit(static_cast<unsigned char>(source[pos]))) {
                    pos++;
                }
                if (pos + 1 < source.size() && source[pos] == '.' &&
                    std::isdigit(static_cast<unsigned char>(source[pos + 1]))) {
                    isDec = true;
                    pos++; // consume '.'
                    while (pos < source.size() && std::isdigit(static_cast<unsigned char>(source[pos]))) {
                        pos++;
                    }
                }
                std::string numStr = source.substr(start, pos - start);
                tokens.push_back({isDec ? TokenType::DEC_LIT : TokenType::INT_LIT, numStr, line, start, pos});
                continue;
            }

            // Identifiers & Keywords
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                while (pos < source.size() &&
                       (std::isalnum(static_cast<unsigned char>(source[pos])) || source[pos] == '_')) {
                    pos++;
                }
                std::string word = source.substr(start, pos - start);
                tokens.push_back({classifyWord(word), word, line, start, pos});
                continue;
            }

            // Two-character operators
            if (pos + 1 < source.size()) {
                std::string two = source.substr(pos, 2);
                if (two == "!=") {
                    pos += 2;
                    tokens.push_back({TokenType::NEQ, two, line, start, pos});
                    continue;
                }
                if (two == "<=") {
                    pos += 2;
                    tokens.push_back({TokenType::LEQ, two, line, start, pos});
                    continue;
                }
                if (two == ">=") {
                    pos += 2;
                    tokens.push_back({TokenType::GEQ, two, line, start, pos});
                    continue;
                }
            }

            // Single-character tokens
            pos++;
            switch (c) {
                case '!': tokens.push_back({TokenType::BANG, "!", line, start, pos}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", line, start, pos}); break;
                case ',': tokens.push_back({TokenType::COMMA, ",", line, start, pos}); break;
                case ':': tokens.push_back({TokenType::COLON, ":", line, start, pos}); break;
                case '[': tokens.push_back({TokenType::LBRACKET, "[", line, start, pos}); break;
                case ']': tokens.push_back({TokenType::RBRACKET, "]", line, start, pos}); break;
                case '{': tokens.push_back({TokenType::LBRACE, "{", line, start, pos}); break;
                case '}': tokens.push_back({TokenType::RBRACE, "}", line, start, pos}); break;
                case '<': tokens.push_back({TokenType::LANGLE, "<", line, start, pos}); break;
                case '>': tokens.push_back({TokenType::RANGLE, ">", line, start, pos}); break;
                case '=': tokens.push_back({TokenType::EQ, "=", line, start, pos}); break;
                case '+': tokens.push_back({TokenType::PLUS, "+", line, start, pos}); break;
                case '-': tokens.push_back({TokenType::MINUS, "-", line, start, pos}); break;
                case '*': tokens.push_back({TokenType::STAR, "*", line, start, pos}); break;
                case '/': tokens.push_back({TokenType::SLASH, "/", line, start, pos}); break;
                default:
                    tokens.push_back({TokenType::SYMBOL, std::string(1, c), line, start, pos});
                    break;
            }
        }

        tokens.push_back({TokenType::END_OF_FILE, "", line, pos, pos});
        return tokens;
    }

private:
    const std::string& source;
    size_t pos;
    int line;

    static TokenType classifyWord(const std::string& w) {
        if (w == "MAKE") return TokenType::KW_MAKE;
        if (w == "PRINT") return TokenType::KW_PRINT;
        if (w == "IN") return TokenType::KW_IN;
        if (w == "ASSIGN") return TokenType::KW_ASSIGN;
        if (w == "CONDITION") return TokenType::KW_CONDITION;
        if (w == "IS") return TokenType::KW_IS;
        if (w == "NEXT") return TokenType::KW_NEXT;
        if (w == "VOID") return TokenType::KW_VOID;
        if (w == "LOOP") return TokenType::KW_LOOP;
        if (w == "FOR") return TokenType::KW_FOR;
        if (w == "WHILE") return TokenType::KW_WHILE;
        if (w == "FUNC") return TokenType::KW_FUNC;
        if (w == "PAR") return TokenType::KW_PAR;
        if (w == "RETURN") return TokenType::KW_RETURN;
        if (w == "CALL") return TokenType::KW_CALL;
        if (w == "NOT") return TokenType::KW_NOT;
        if (w == "true") return TokenType::KW_TRUE;
        if (w == "false") return TokenType::KW_FALSE;
        return TokenType::IDENT;
    }
};

// ============================================================================
// 2. RUNTIME VALUES & ENVIRONMENT
// ============================================================================

struct Value;
struct BlockNode;

using ArrayType = std::vector<Value>;
using DictType = std::unordered_map<std::string, Value>;

struct Value {
    std::variant<
        std::monostate,
        long long,
        double,
        std::string,
        bool,
        std::shared_ptr<ArrayType>,
        std::shared_ptr<DictType>
    > data;

    Value() : data(std::monostate{}) {}
    Value(long long v) : data(v) {}
    Value(int v) : data(static_cast<long long>(v)) {}
    Value(double v) : data(v) {}
    Value(const std::string& v) : data(v) {}
    Value(const char* v) : data(std::string(v)) {}
    Value(bool v) : data(v) {}
    Value(std::shared_ptr<ArrayType> v) : data(std::move(v)) {}
    Value(std::shared_ptr<DictType> v) : data(std::move(v)) {}

    bool isNumber() const {
        return std::holds_alternative<long long>(data) || std::holds_alternative<double>(data);
    }

    double asDouble() const {
        if (std::holds_alternative<long long>(data)) return static_cast<double>(std::get<long long>(data));
        if (std::holds_alternative<double>(data)) return std::get<double>(data);
        if (std::holds_alternative<bool>(data)) return std::get<bool>(data) ? 1.0 : 0.0;
        if (std::holds_alternative<std::string>(data)) {
            try {
                return std::stod(std::get<std::string>(data));
            } catch (...) {
                throw std::runtime_error("Cannot convert text '" + std::get<std::string>(data) + "' to number");
            }
        }
        throw std::runtime_error("Value is not numeric");
    }

    long long asInt() const {
        if (std::holds_alternative<long long>(data)) return std::get<long long>(data);
        if (std::holds_alternative<double>(data)) return static_cast<long long>(std::get<double>(data));
        if (std::holds_alternative<bool>(data)) return std::get<bool>(data) ? 1 : 0;
        if (std::holds_alternative<std::string>(data)) {
            try {
                return std::stoll(std::get<std::string>(data));
            } catch (...) {
                throw std::runtime_error("Cannot convert text '" + std::get<std::string>(data) + "' to NUM");
            }
        }
        throw std::runtime_error("Value cannot be converted to NUM");
    }

    bool isTruthy() const {
        if (std::holds_alternative<std::monostate>(data)) return false;
        if (std::holds_alternative<bool>(data)) return std::get<bool>(data);
        if (std::holds_alternative<long long>(data)) return std::get<long long>(data) != 0;
        if (std::holds_alternative<double>(data)) return std::get<double>(data) != 0.0;
        if (std::holds_alternative<std::string>(data)) {
            const auto& s = std::get<std::string>(data);
            return !s.empty() && s != "false";
        }
        if (std::holds_alternative<std::shared_ptr<ArrayType>>(data)) {
            return !std::get<std::shared_ptr<ArrayType>>(data)->empty();
        }
        if (std::holds_alternative<std::shared_ptr<DictType>>(data)) {
            return !std::get<std::shared_ptr<DictType>>(data)->empty();
        }
        return false;
    }

    std::string toString() const {
        if (std::holds_alternative<std::monostate>(data)) return "";
        if (std::holds_alternative<long long>(data)) return std::to_string(std::get<long long>(data));
        if (std::holds_alternative<double>(data)) {
            std::ostringstream oss;
            oss << std::get<double>(data);
            return oss.str();
        }
        if (std::holds_alternative<std::string>(data)) return std::get<std::string>(data);
        if (std::holds_alternative<bool>(data)) return std::get<bool>(data) ? "true" : "false";
        if (std::holds_alternative<std::shared_ptr<ArrayType>>(data)) {
            const auto& arr = *std::get<std::shared_ptr<ArrayType>>(data);
            std::string out = "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) out += ", ";
                out += arr[i].toString();
            }
            out += "]";
            return out;
        }
        if (std::holds_alternative<std::shared_ptr<DictType>>(data)) {
            const auto& dict = *std::get<std::shared_ptr<DictType>>(data);
            std::string out = "{";
            bool first = true;
            for (const auto& kv : dict) {
                if (!first) out += ", ";
                first = false;
                out += kv.first + ": " + kv.second.toString();
            }
            out += "}";
            return out;
        }
        return "";
    }

    bool equals(const Value& other) const {
        if (isNumber() && other.isNumber()) {
            return std::fabs(asDouble() - other.asDouble()) < 1e-9;
        }
        if (std::holds_alternative<bool>(data) && std::holds_alternative<bool>(other.data)) {
            return std::get<bool>(data) == std::get<bool>(other.data);
        }
        if (std::holds_alternative<std::string>(data) && std::holds_alternative<std::string>(other.data)) {
            return std::get<std::string>(data) == std::get<std::string>(other.data);
        }
        return toString() == other.toString();
    }
};

struct FuncDef {
    std::vector<std::string> params;
    BlockNode* body = nullptr; // Owned by AST
};

struct ReturnSignal {
    Value value;
};

class Environment : public std::enable_shared_from_this<Environment> {
public:
    explicit Environment(std::shared_ptr<Environment> parentEnv = nullptr)
        : parent(std::move(parentEnv)) {}

    static Value coerceToType(const std::string& typeTag, const Value& val) {
        if (typeTag == "NUM") return Value(val.asInt());
        if (typeTag == "DEC") return Value(val.asDouble());
        if (typeTag == "STR") return Value(val.toString());
        if (typeTag == "TOF") return Value(val.isTruthy());
        if (typeTag == "ARR") {
            if (!std::holds_alternative<std::shared_ptr<ArrayType>>(val.data)) {
                throw std::runtime_error("Expected an array [...] for <ARR>");
            }
            return val;
        }
        if (typeTag == "KEY") {
            if (!std::holds_alternative<std::shared_ptr<DictType>>(val.data)) {
                throw std::runtime_error("Expected a dictionary {...} for <KEY>");
            }
            return val;
        }
        throw std::runtime_error("Unknown type <" + typeTag + ">");
    }

    void define(const std::string& typeTag, const std::string& name, const Value& val) {
        vars[name] = {typeTag, coerceToType(typeTag, val)};
    }

    bool hasVar(const std::string& name) const {
        if (vars.count(name)) return true;
        if (parent) return parent->hasVar(name);
        return false;
    }

    Value getVar(const std::string& name) const {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second.value;
        if (parent) return parent->getVar(name);
        throw std::runtime_error("Undefined variable '" + name + "'");
    }

    std::string getVarType(const std::string& name) const {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second.typeTag;
        if (parent) return parent->getVarType(name);
        throw std::runtime_error("Undefined variable '" + name + "'");
    }

    void assign(const std::string& name, const Value& val) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            it->second.value = coerceToType(it->second.typeTag, val);
            return;
        }
        if (parent) {
            parent->assign(name, val);
            return;
        }
        throw std::runtime_error("Cannot assign to undeclared variable '" + name + "'");
    }

    void defineFunc(const std::string& name, FuncDef fn) {
        funcs[name] = std::move(fn);
    }

    FuncDef getFunc(const std::string& name) const {
        auto it = funcs.find(name);
        if (it != funcs.end()) return it->second;
        if (parent) return parent->getFunc(name);
        throw std::runtime_error("Undefined function '" + name + "'");
    }

private:
    struct VarEntry {
        std::string typeTag;
        Value value;
    };

    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, VarEntry> vars;
    std::unordered_map<std::string, FuncDef> funcs;
};

// ============================================================================
// 3. AST NODE DECLARATIONS
// ============================================================================

struct ExprNode {
    virtual ~ExprNode() = default;
    virtual Value eval(std::shared_ptr<Environment> env) = 0;
};

struct StmtNode {
    virtual ~StmtNode() = default;
    virtual void execute(std::shared_ptr<Environment> env) = 0;
};

struct BlockNode {
    std::vector<std::unique_ptr<StmtNode>> statements;
    void execute(std::shared_ptr<Environment> env);
};

// --- Expression Declarations ---

struct LiteralExpr : ExprNode {
    Value val;
    explicit LiteralExpr(Value v) : val(std::move(v)) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

struct VarExpr : ExprNode {
    std::string name;
    bool allowUnquotedWord;
    VarExpr(std::string n, bool unquotedFallback = true)
        : name(std::move(n)), allowUnquotedWord(unquotedFallback) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

struct ArrayLiteralExpr : ExprNode {
    std::vector<std::unique_ptr<ExprNode>> elements;
    explicit ArrayLiteralExpr(std::vector<std::unique_ptr<ExprNode>> elems)
        : elements(std::move(elems)) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

struct DictLiteralExpr : ExprNode {
    std::vector<std::pair<std::string, std::unique_ptr<ExprNode>>> entries;
    explicit DictLiteralExpr(std::vector<std::pair<std::string, std::unique_ptr<ExprNode>>> ents)
        : entries(std::move(ents)) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

struct IndexExpr : ExprNode {
    std::unique_ptr<ExprNode> target;
    std::unique_ptr<ExprNode> index;
    IndexExpr(std::unique_ptr<ExprNode> t, std::unique_ptr<ExprNode> idx)
        : target(std::move(t)), index(std::move(idx)) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

struct UnaryExpr : ExprNode {
    std::string op;
    std::unique_ptr<ExprNode> right;
    UnaryExpr(std::string o, std::unique_ptr<ExprNode> r)
        : op(std::move(o)), right(std::move(r)) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

struct BinaryExpr : ExprNode {
    std::unique_ptr<ExprNode> left;
    std::string op;
    std::unique_ptr<ExprNode> right;
    BinaryExpr(std::unique_ptr<ExprNode> l, std::string o, std::unique_ptr<ExprNode> r)
        : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

struct CallExpr : ExprNode {
    std::string funcName;
    std::vector<std::unique_ptr<ExprNode>> args;
    CallExpr(std::string name, std::vector<std::unique_ptr<ExprNode>> a)
        : funcName(std::move(name)), args(std::move(a)) {}
    Value eval(std::shared_ptr<Environment> env) override;
};

// --- Statement Declarations ---

struct MakeStmt : StmtNode {
    std::string typeTag;
    std::string varName;
    std::unique_ptr<ExprNode> initExpr;
    MakeStmt(std::string t, std::string n, std::unique_ptr<ExprNode> expr)
        : typeTag(std::move(t)), varName(std::move(n)), initExpr(std::move(expr)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct AssignStmt : StmtNode {
    std::string varName;
    std::unique_ptr<ExprNode> valueExpr;
    AssignStmt(std::string n, std::unique_ptr<ExprNode> expr)
        : varName(std::move(n)), valueExpr(std::move(expr)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct IndexAssignStmt : StmtNode {
    std::unique_ptr<ExprNode> targetExpr;
    std::unique_ptr<ExprNode> indexExpr;
    std::unique_ptr<ExprNode> valueExpr;
    IndexAssignStmt(std::unique_ptr<ExprNode> t, std::unique_ptr<ExprNode> idx, std::unique_ptr<ExprNode> val)
        : targetExpr(std::move(t)), indexExpr(std::move(idx)), valueExpr(std::move(val)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct PrintStmt : StmtNode {
    std::vector<std::unique_ptr<ExprNode>> items;
    explicit PrintStmt(std::vector<std::unique_ptr<ExprNode>> it) : items(std::move(it)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct InStmt : StmtNode {
    std::string varName;
    std::string prompt;
    bool hasPrompt;
    InStmt(std::string v, std::string p, bool hp)
        : varName(std::move(v)), prompt(std::move(p)), hasPrompt(hp) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct ConditionBranch {
    std::unique_ptr<ExprNode> condition; // nullptr for CONDITION VOID (else)
    std::unique_ptr<BlockNode> body;
};

struct ConditionStmt : StmtNode {
    std::vector<ConditionBranch> branches;
    void execute(std::shared_ptr<Environment> env) override;
};

struct LoopForStmt : StmtNode {
    std::string iterVar;
    std::unique_ptr<ExprNode> iterableExpr;
    std::unique_ptr<BlockNode> body;
    LoopForStmt(std::string v, std::unique_ptr<ExprNode> it, std::unique_ptr<BlockNode> b)
        : iterVar(std::move(v)), iterableExpr(std::move(it)), body(std::move(b)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct LoopWhileStmt : StmtNode {
    std::unique_ptr<ExprNode> condition;
    std::unique_ptr<BlockNode> body;
    LoopWhileStmt(std::unique_ptr<ExprNode> cond, std::unique_ptr<BlockNode> b)
        : condition(std::move(cond)), body(std::move(b)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct FuncDeclStmt : StmtNode {
    std::string funcName;
    std::vector<std::string> params;
    std::unique_ptr<BlockNode> body;
    FuncDeclStmt(std::string n, std::vector<std::string> p, std::unique_ptr<BlockNode> b)
        : funcName(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct ReturnStmt : StmtNode {
    std::unique_ptr<ExprNode> expr;
    explicit ReturnStmt(std::unique_ptr<ExprNode> e) : expr(std::move(e)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

struct CallStmt : StmtNode {
    std::unique_ptr<CallExpr> call;
    explicit CallStmt(std::unique_ptr<CallExpr> c) : call(std::move(c)) {}
    void execute(std::shared_ptr<Environment> env) override;
};

// ============================================================================
// 4. AST METHOD IMPLEMENTATIONS (All types fully defined above)
// ============================================================================

void BlockNode::execute(std::shared_ptr<Environment> env) {
    for (auto& stmt : statements) {
        stmt->execute(env);
    }
}

Value LiteralExpr::eval(std::shared_ptr<Environment>) {
    return val;
}

Value VarExpr::eval(std::shared_ptr<Environment> env) {
    if (env->hasVar(name)) {
        return env->getVar(name);
    }
    if (allowUnquotedWord) {
        return Value(name);
    }
    throw std::runtime_error("Undefined variable '" + name + "'");
}

Value ArrayLiteralExpr::eval(std::shared_ptr<Environment> env) {
    auto arr = std::make_shared<ArrayType>();
    arr->reserve(elements.size());
    for (auto& el : elements) {
        arr->push_back(el->eval(env));
    }
    return Value(arr);
}

Value DictLiteralExpr::eval(std::shared_ptr<Environment> env) {
    auto dict = std::make_shared<DictType>();
    for (auto& kv : entries) {
        (*dict)[kv.first] = kv.second->eval(env);
    }
    return Value(dict);
}

Value IndexExpr::eval(std::shared_ptr<Environment> env) {
    Value container = target->eval(env);

    if (std::holds_alternative<std::shared_ptr<ArrayType>>(container.data)) {
        auto arr = std::get<std::shared_ptr<ArrayType>>(container.data);
        long long idx = index->eval(env).asInt();
        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
            throw std::runtime_error("Array index " + std::to_string(idx) +
                                     " out of bounds (size " + std::to_string(arr->size()) + ")");
        }
        return (*arr)[static_cast<size_t>(idx)];
    }

    if (std::holds_alternative<std::shared_ptr<DictType>>(container.data)) {
        auto dict = std::get<std::shared_ptr<DictType>>(container.data);
        std::string key;
        if (auto* varIdx = dynamic_cast<VarExpr*>(index.get())) {
            if (dict->count(varIdx->name)) {
                key = varIdx->name;
            } else {
                key = index->eval(env).toString();
            }
        } else {
            key = index->eval(env).toString();
        }
        auto it = dict->find(key);
        if (it == dict->end()) {
            throw std::runtime_error("Key '" + key + "' not found in dictionary");
        }
        return it->second;
    }

    if (std::holds_alternative<std::string>(container.data)) {
        const auto& str = std::get<std::string>(container.data);
        long long idx = index->eval(env).asInt();
        if (idx < 0 || static_cast<size_t>(idx) >= str.size()) {
            throw std::runtime_error("String index out of bounds");
        }
        return Value(std::string(1, str[static_cast<size_t>(idx)]));
    }

    throw std::runtime_error("Cannot index into non-collection value");
}

Value UnaryExpr::eval(std::shared_ptr<Environment> env) {
    Value val = right->eval(env);
    if (op == "NOT") {
        return Value(!val.isTruthy());
    }
    if (op == "-") {
        if (std::holds_alternative<long long>(val.data)) {
            return Value(-std::get<long long>(val.data));
        }
        return Value(-val.asDouble());
    }
    throw std::runtime_error("Unknown unary operator '" + op + "'");
}

Value BinaryExpr::eval(std::shared_ptr<Environment> env) {
    Value lval = left->eval(env);
    Value rval = right->eval(env);

    // String concatenation with space when using '+' on strings
    if (op == "+" && (std::holds_alternative<std::string>(lval.data) ||
                      std::holds_alternative<std::string>(rval.data))) {
        std::string ls = lval.toString();
        std::string rs = rval.toString();
        if (ls.empty()) return Value(rs);
        if (rs.empty()) return Value(ls);
        return Value(ls + " " + rs);
    }

    // Arithmetic
    if (op == "+" || op == "-" || op == "*" || op == "/") {
        bool useDouble = std::holds_alternative<double>(lval.data) ||
                         std::holds_alternative<double>(rval.data);
        if (op == "/") {
            double denom = rval.asDouble();
            if (denom == 0.0) throw std::runtime_error("Division by zero");
            if (!useDouble && (lval.asInt() % rval.asInt() == 0)) {
                return Value(lval.asInt() / rval.asInt());
            }
            return Value(lval.asDouble() / denom);
        }
        if (useDouble) {
            double a = lval.asDouble();
            double b = rval.asDouble();
            if (op == "+") return Value(a + b);
            if (op == "-") return Value(a - b);
            if (op == "*") return Value(a * b);
        } else {
            long long a = lval.asInt();
            long long b = rval.asInt();
            if (op == "+") return Value(a + b);
            if (op == "-") return Value(a - b);
            if (op == "*") return Value(a * b);
        }
    }

    // Equality & Relational
    if (op == "=") return Value(lval.equals(rval));
    if (op == "!=") return Value(!lval.equals(rval));
    if (op == "<") return Value(lval.asDouble() < rval.asDouble());
    if (op == ">") return Value(lval.asDouble() > rval.asDouble());
    if (op == "<=") return Value(lval.asDouble() <= rval.asDouble());
    if (op == ">=") return Value(lval.asDouble() >= rval.asDouble());

    throw std::runtime_error("Unknown binary operator '" + op + "'");
}

Value CallExpr::eval(std::shared_ptr<Environment> env) {
    FuncDef fn = env->getFunc(funcName);
    if (fn.params.size() != args.size()) {
        throw std::runtime_error("Function '" + funcName + "' expected " +
                                 std::to_string(fn.params.size()) + " args, got " +
                                 std::to_string(args.size()));
    }

    auto localEnv = std::make_shared<Environment>(env);
    for (size_t i = 0; i < fn.params.size(); ++i) {
        Value argVal = args[i]->eval(env);
        std::string inferredType = "STR";
        if (std::holds_alternative<long long>(argVal.data)) inferredType = "NUM";
        else if (std::holds_alternative<double>(argVal.data)) inferredType = "DEC";
        else if (std::holds_alternative<bool>(argVal.data)) inferredType = "TOF";
        else if (std::holds_alternative<std::shared_ptr<ArrayType>>(argVal.data)) inferredType = "ARR";
        else if (std::holds_alternative<std::shared_ptr<DictType>>(argVal.data)) inferredType = "KEY";
        localEnv->define(inferredType, fn.params[i], argVal);
    }

    try {
        fn.body->execute(localEnv);
    } catch (const ReturnSignal& ret) {
        return ret.value;
    }
    return Value();
}

void MakeStmt::execute(std::shared_ptr<Environment> env) {
    if (!initExpr) {
        if (typeTag == "NUM") env->define(typeTag, varName, Value(0LL));
        else if (typeTag == "DEC") env->define(typeTag, varName, Value(0.0));
        else if (typeTag == "STR") env->define(typeTag, varName, Value(""));
        else if (typeTag == "TOF") env->define(typeTag, varName, Value(false));
        else if (typeTag == "ARR") env->define(typeTag, varName, Value(std::make_shared<ArrayType>()));
        else if (typeTag == "KEY") env->define(typeTag, varName, Value(std::make_shared<DictType>()));
        return;
    }
    Value v = initExpr->eval(env);
    env->define(typeTag, varName, v);
}

void AssignStmt::execute(std::shared_ptr<Environment> env) {
    Value v = valueExpr->eval(env);
    env->assign(varName, v);
}

void IndexAssignStmt::execute(std::shared_ptr<Environment> env) {
    Value container = targetExpr->eval(env);
    Value newVal = valueExpr->eval(env);

    if (std::holds_alternative<std::shared_ptr<ArrayType>>(container.data)) {
        auto arr = std::get<std::shared_ptr<ArrayType>>(container.data);
        long long idx = indexExpr->eval(env).asInt();
        if (idx < 0 || static_cast<size_t>(idx) >= arr->size()) {
            throw std::runtime_error("Array index out of bounds during assignment");
        }
        (*arr)[static_cast<size_t>(idx)] = newVal;
        return;
    }

    if (std::holds_alternative<std::shared_ptr<DictType>>(container.data)) {
        auto dict = std::get<std::shared_ptr<DictType>>(container.data);
        std::string key;
        if (auto* varIdx = dynamic_cast<VarExpr*>(indexExpr.get())) {
            if (dict->count(varIdx->name)) {
                key = varIdx->name;
            } else if (env->hasVar(varIdx->name)) {
                key = env->getVar(varIdx->name).toString();
            } else {
                key = varIdx->name;
            }
        } else {
            key = indexExpr->eval(env).toString();
        }
        (*dict)[key] = newVal;
        return;
    }

    throw std::runtime_error("Cannot index-assign into non-collection value");
}

void PrintStmt::execute(std::shared_ptr<Environment> env) {
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) std::cout << " ";
        std::cout << items[i]->eval(env).toString();
    }
    std::cout << std::endl;
}

void InStmt::execute(std::shared_ptr<Environment> env) {
    if (hasPrompt && !prompt.empty()) {
        std::cout << prompt;
        if (prompt.back() != ' ') std::cout << " ";
        std::cout.flush();
    }
    std::string line;
    if (!std::getline(std::cin, line)) {
        line = "";
    }
    if (env->hasVar(varName)) {
        env->assign(varName, Value(line));
    } else {
        env->define("STR", varName, Value(line));
    }
}

void ConditionStmt::execute(std::shared_ptr<Environment> env) {
    for (auto& branch : branches) {
        if (!branch.condition || branch.condition->eval(env).isTruthy()) {
            auto branchEnv = std::make_shared<Environment>(env);
            branch.body->execute(branchEnv);
            return;
        }
    }
}

void LoopForStmt::execute(std::shared_ptr<Environment> env) {
    Value target = iterableExpr->eval(env);

    if (target.isNumber()) {
        long long limit = target.asInt();
        for (long long i = 0; i < limit; ++i) {
            auto loopEnv = std::make_shared<Environment>(env);
            loopEnv->define("NUM", iterVar, Value(i));
            body->execute(loopEnv);
        }
        return;
    }

    if (std::holds_alternative<std::shared_ptr<ArrayType>>(target.data)) {
        auto arr = std::get<std::shared_ptr<ArrayType>>(target.data);
        for (const auto& elem : *arr) {
            auto loopEnv = std::make_shared<Environment>(env);
            std::string t = "STR";
            if (std::holds_alternative<long long>(elem.data)) t = "NUM";
            else if (std::holds_alternative<double>(elem.data)) t = "DEC";
            else if (std::holds_alternative<bool>(elem.data)) t = "TOF";
            else if (std::holds_alternative<std::shared_ptr<ArrayType>>(elem.data)) t = "ARR";
            else if (std::holds_alternative<std::shared_ptr<DictType>>(elem.data)) t = "KEY";
            loopEnv->define(t, iterVar, elem);
            body->execute(loopEnv);
        }
        return;
    }

    if (std::holds_alternative<std::string>(target.data)) {
        const auto& str = std::get<std::string>(target.data);
        for (char ch : str) {
            auto loopEnv = std::make_shared<Environment>(env);
            loopEnv->define("STR", iterVar, Value(std::string(1, ch)));
            body->execute(loopEnv);
        }
        return;
    }

    throw std::runtime_error("LOOP FOR requires a NUM, ARR, or STR");
}

void LoopWhileStmt::execute(std::shared_ptr<Environment> env) {
    while (condition->eval(env).isTruthy()) {
        auto loopEnv = std::make_shared<Environment>(env);
        body->execute(loopEnv);
    }
}

void FuncDeclStmt::execute(std::shared_ptr<Environment> env) {
    FuncDef def;
    def.params = params;
    def.body = body.get();
    env->defineFunc(funcName, def);
}

void ReturnStmt::execute(std::shared_ptr<Environment> env) {
    Value v = expr ? expr->eval(env) : Value();
    throw ReturnSignal{v};
}

void CallStmt::execute(std::shared_ptr<Environment> env) {
    call->eval(env);
}

// ============================================================================
// 5. RECURSIVE DESCENT PARSER
// ============================================================================

class Parser {
public:
    Parser(const std::vector<Token>& toks, const std::string& src)
        : tokens(toks), source(src), pos(0) {}

    std::unique_ptr<BlockNode> parseProgram() {
        auto root = std::make_unique<BlockNode>();
        while (!check(TokenType::END_OF_FILE)) {
            if (match(TokenType::SEMICOLON)) continue; // Ignore stray semicolons
            root->statements.push_back(parseStatement());
        }
        return root;
    }

private:
    const std::vector<Token>& tokens;
    const std::string& source;
    size_t pos;

    const Token& peek(size_t offset = 0) const {
        if (pos + offset >= tokens.size()) return tokens.back();
        return tokens[pos + offset];
    }

    bool check(TokenType t) const {
        return peek().type == t;
    }

    const Token& advance() {
        if (pos < tokens.size()) pos++;
        return tokens[pos - 1];
    }

    bool match(TokenType t) {
        if (check(t)) {
            advance();
            return true;
        }
        return false;
    }

    const Token& expect(TokenType t, const std::string& msg) {
        if (!check(t)) {
            throw std::runtime_error("Syntax Error on line " + std::to_string(peek().line) +
                                     ": " + msg + " (got '" + peek().text + "')");
        }
        return advance();
    }

    static std::string trim(const std::string& s) {
        size_t first = 0;
        while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) first++;
        size_t last = s.size();
        while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) last--;
        return s.substr(first, last - first);
    }

    // Extract raw text from the source up to the next SEMICOLON
    std::string sliceRawUntilSemicolon() {
        if (check(TokenType::SEMICOLON) || check(TokenType::END_OF_FILE)) {
            expect(TokenType::SEMICOLON, "Expected ';'");
            return "";
        }
        size_t startIdx = peek().startPos;
        size_t endIdx = peek().endPos;
        while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
            endIdx = peek().endPos;
            advance();
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        return trim(source.substr(startIdx, endIdx - startIdx));
    }

    std::unique_ptr<BlockNode> parseBlock() {
        expect(TokenType::BANG, "Expected '!' to open block");
        auto block = std::make_unique<BlockNode>();
        while (!check(TokenType::BANG) && !check(TokenType::END_OF_FILE)) {
            if (match(TokenType::SEMICOLON)) continue;
            block->statements.push_back(parseStatement());
        }
        expect(TokenType::BANG, "Expected '!' to close block");
        match(TokenType::SEMICOLON); // Optional ';' after closing '!'
        return block;
    }

    std::unique_ptr<StmtNode> parseStatement() {
        if (match(TokenType::KW_MAKE)) return parseMake();
        if (match(TokenType::KW_PRINT)) return parsePrint();
        if (match(TokenType::KW_IN)) return parseIn();
        if (match(TokenType::KW_CONDITION)) return parseCondition();
        if (match(TokenType::KW_LOOP)) return parseLoop();
        if (match(TokenType::KW_FUNC)) return parseFunc();
        if (match(TokenType::KW_RETURN)) return parseReturn();
        if (match(TokenType::KW_CALL)) return parseCallStmt();
        if (check(TokenType::IDENT)) return parseIdentStatement();

        throw std::runtime_error("Syntax Error on line " + std::to_string(peek().line) +
                                 ": Unexpected token '" + peek().text + "'");
    }

    // MAKE <TYPE> varName ASSIGN value;
    std::unique_ptr<StmtNode> parseMake() {
        expect(TokenType::LANGLE, "Expected '<' before type in MAKE");
        Token typeTok = expect(TokenType::IDENT, "Expected type name (NUM, DEC, STR, TOF, ARR, KEY)");
        expect(TokenType::RANGLE, "Expected '>' after type in MAKE");
        Token nameTok = expect(TokenType::IDENT, "Expected variable name after type");

        if (match(TokenType::SEMICOLON)) {
            return std::make_unique<MakeStmt>(typeTok.text, nameTok.text, nullptr);
        }

        expect(TokenType::KW_ASSIGN, "Expected 'ASSIGN' or ';' in MAKE statement");

        if (typeTok.text == "STR") {
            // Check if this is an expression (contains '+', '[', or function call '!') vs unquoted text
            bool isComplexExpr = false;
            for (size_t i = 0; peek(i).type != TokenType::SEMICOLON &&
                               peek(i).type != TokenType::END_OF_FILE; ++i) {
                TokenType tt = peek(i).type;
                if (tt == TokenType::PLUS || tt == TokenType::BANG || tt == TokenType::LBRACKET) {
                    isComplexExpr = true;
                    break;
                }
            }
            if (!isComplexExpr) {
                // Single identifier might be copying an existing variable or a literal word
                if (peek(0).type == TokenType::IDENT && peek(1).type == TokenType::SEMICOLON) {
                    std::string word = advance().text;
                    expect(TokenType::SEMICOLON, "Expected ';'");
                    return std::make_unique<MakeStmt>(
                        "STR", nameTok.text, std::make_unique<VarExpr>(word, true));
                }
                std::string rawText = sliceRawUntilSemicolon();
                return std::make_unique<MakeStmt>(
                    "STR", nameTok.text, std::make_unique<LiteralExpr>(Value(rawText)));
            }
        }

        auto expr = parseExpression(false);
        expect(TokenType::SEMICOLON, "Expected ';' after MAKE statement");
        return std::make_unique<MakeStmt>(typeTok.text, nameTok.text, std::move(expr));
    }

    // PRINT item1 item2 ...;
    std::unique_ptr<StmtNode> parsePrint() {
        std::vector<std::unique_ptr<ExprNode>> items;
        while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
            items.push_back(parseExpression(false));
        }
        expect(TokenType::SEMICOLON, "Expected ';' after PRINT statement");
        return std::make_unique<PrintStmt>(std::move(items));
    }

    // IN varName ASSIGN prompt text;
    std::unique_ptr<StmtNode> parseIn() {
        Token varTok = expect(TokenType::IDENT, "Expected variable name after IN");
        if (match(TokenType::SEMICOLON)) {
            return std::make_unique<InStmt>(varTok.text, "", false);
        }
        expect(TokenType::KW_ASSIGN, "Expected 'ASSIGN' or ';' in IN statement");
        std::string prompt = sliceRawUntilSemicolon();
        return std::make_unique<InStmt>(varTok.text, prompt, true);
    }

    // CONDITION cond IS ! ... ! CONDITION NEXT cond IS ! ... ! CONDITION VOID ! ... !
    std::unique_ptr<StmtNode> parseCondition() {
        auto stmt = std::make_unique<ConditionStmt>();

        // First IF branch
        auto firstCond = parseExpression(false);
        expect(TokenType::KW_IS, "Expected 'IS' after condition");
        auto firstBody = parseBlock();
        stmt->branches.push_back({std::move(firstCond), std::move(firstBody)});

        // Chained CONDITION NEXT / CONDITION VOID branches
        while (check(TokenType::KW_CONDITION)) {
            if (peek(1).type == TokenType::KW_NEXT) {
                advance(); // CONDITION
                advance(); // NEXT
                auto nextCond = parseExpression(false);
                expect(TokenType::KW_IS, "Expected 'IS' after CONDITION NEXT expression");
                auto nextBody = parseBlock();
                stmt->branches.push_back({std::move(nextCond), std::move(nextBody)});
            } else if (peek(1).type == TokenType::KW_VOID) {
                advance(); // CONDITION
                advance(); // VOID
                auto elseBody = parseBlock();
                stmt->branches.push_back({nullptr, std::move(elseBody)});
                break;
            } else {
                // Separate standalone CONDITION statement
                break;
            }
        }

        return stmt;
    }

    // LOOP FOR i, 5 ! ... !  OR  LOOP WHILE cond ! ... !
    std::unique_ptr<StmtNode> parseLoop() {
        if (match(TokenType::KW_FOR)) {
            Token iterTok = expect(TokenType::IDENT, "Expected loop variable after LOOP FOR");
            expect(TokenType::COMMA, "Expected ',' after loop variable in LOOP FOR");
            auto iterExpr = parseExpression(true); // Stop before top-level '!' block opener
            auto body = parseBlock();
            return std::make_unique<LoopForStmt>(iterTok.text, std::move(iterExpr), std::move(body));
        }
        if (match(TokenType::KW_WHILE)) {
            auto condExpr = parseExpression(true); // Stop before top-level '!' block opener
            auto body = parseBlock();
            return std::make_unique<LoopWhileStmt>(std::move(condExpr), std::move(body));
        }
        throw std::runtime_error("Syntax Error on line " + std::to_string(peek().line) +
                                 ": Expected 'FOR' or 'WHILE' after LOOP");
    }

    // FUNC name PAR !a, b! ! ... !  OR  FUNC name ! ... !
    std::unique_ptr<StmtNode> parseFunc() {
        Token nameTok = expect(TokenType::IDENT, "Expected function name after FUNC");
        std::vector<std::string> params;

        if (match(TokenType::KW_PAR)) {
            expect(TokenType::BANG, "Expected '!' after PAR");
            if (!check(TokenType::BANG)) {
                params.push_back(expect(TokenType::IDENT, "Expected parameter name").text);
                while (match(TokenType::COMMA)) {
                    params.push_back(expect(TokenType::IDENT, "Expected parameter name after ','").text);
                }
            }
            expect(TokenType::BANG, "Expected '!' after parameter list");
        }

        auto body = parseBlock();
        return std::make_unique<FuncDeclStmt>(nameTok.text, std::move(params), std::move(body));
    }

    // RETURN expr;  OR  RETURN;
    std::unique_ptr<StmtNode> parseReturn() {
        if (match(TokenType::SEMICOLON)) {
            return std::make_unique<ReturnStmt>(nullptr);
        }
        auto expr = parseExpression(false);
        expect(TokenType::SEMICOLON, "Expected ';' after RETURN");
        return std::make_unique<ReturnStmt>(std::move(expr));
    }

    // CALL funcName!arg1, arg2!;  OR  CALL funcName;
    std::unique_ptr<StmtNode> parseCallStmt() {
        Token nameTok = expect(TokenType::IDENT, "Expected function name after CALL");
        std::vector<std::unique_ptr<ExprNode>> args;
        if (match(TokenType::BANG)) {
            if (!check(TokenType::BANG)) {
                args.push_back(parseExpression(false));
                while (match(TokenType::COMMA)) {
                    args.push_back(parseExpression(false));
                }
            }
            expect(TokenType::BANG, "Expected '!' after function call arguments");
        }
        expect(TokenType::SEMICOLON, "Expected ';' after CALL statement");
        return std::make_unique<CallStmt>(
            std::make_unique<CallExpr>(nameTok.text, std::move(args)));
    }

    // Variable assignment, indexed assignment, or bare function call
    std::unique_ptr<StmtNode> parseIdentStatement() {
        Token identTok = advance();

        // Bare function call without CALL keyword: funcName!arg1, arg2!;
        if (match(TokenType::BANG)) {
            std::vector<std::unique_ptr<ExprNode>> args;
            if (!check(TokenType::BANG)) {
                args.push_back(parseExpression(false));
                while (match(TokenType::COMMA)) {
                    args.push_back(parseExpression(false));
                }
            }
            expect(TokenType::BANG, "Expected '!' after function arguments");
            expect(TokenType::SEMICOLON, "Expected ';' after function call");
            return std::make_unique<CallStmt>(
                std::make_unique<CallExpr>(identTok.text, std::move(args)));
        }

        // Indexed assignment: arr[0] ASSIGN val; or dict[key] ASSIGN val;
        if (check(TokenType::LBRACKET)) {
            std::unique_ptr<ExprNode> target = std::make_unique<VarExpr>(identTok.text, false);
            std::unique_ptr<ExprNode> lastIdx = nullptr;
            while (match(TokenType::LBRACKET)) {
                if (lastIdx) {
                    target = std::make_unique<IndexExpr>(std::move(target), std::move(lastIdx));
                }
                lastIdx = parseExpression(false);
                expect(TokenType::RBRACKET, "Expected ']'");
            }
            expect(TokenType::KW_ASSIGN, "Expected 'ASSIGN' for indexed assignment");
            auto valExpr = parseExpression(false);
            expect(TokenType::SEMICOLON, "Expected ';' after assignment");
            return std::make_unique<IndexAssignStmt>(
                std::move(target), std::move(lastIdx), std::move(valExpr));
        }

        // Standard assignment: varName ASSIGN expr;
        expect(TokenType::KW_ASSIGN, "Expected 'ASSIGN' after '" + identTok.text + "'");

        // Detect unquoted multi-word string reassignment (e.g., msg ASSIGN hello world;)
        bool hasConsecutiveWords = false;
        bool hasOps = false;
        for (size_t i = 0; peek(i).type != TokenType::SEMICOLON &&
                           peek(i).type != TokenType::END_OF_FILE; ++i) {
            TokenType t1 = peek(i).type;
            TokenType t2 = peek(i + 1).type;
            if (t1 == TokenType::PLUS || t1 == TokenType::MINUS || t1 == TokenType::STAR ||
                t1 == TokenType::SLASH || t1 == TokenType::BANG || t1 == TokenType::LBRACKET) {
                hasOps = true;
            }
            bool isVal1 = (t1 == TokenType::IDENT || t1 == TokenType::INT_LIT ||
                           t1 == TokenType::DEC_LIT || t1 == TokenType::SYMBOL);
            bool isVal2 = (t2 == TokenType::IDENT || t2 == TokenType::INT_LIT ||
                           t2 == TokenType::DEC_LIT || t2 == TokenType::SYMBOL);
            if (isVal1 && isVal2) {
                hasConsecutiveWords = true;
                break;
            }
        }

        if (hasConsecutiveWords && !hasOps) {
            std::string rawText = sliceRawUntilSemicolon();
            return std::make_unique<AssignStmt>(
                identTok.text, std::make_unique<LiteralExpr>(Value(rawText)));
        }

        auto expr = parseExpression(false);
        expect(TokenType::SEMICOLON, "Expected ';' after assignment");
        return std::make_unique<AssignStmt>(identTok.text, std::move(expr));
    }

    // --- Expression Precedence Climbing ---

    std::unique_ptr<ExprNode> parseExpression(bool inLoopHeader) {
        return parseEquality(inLoopHeader);
    }

    std::unique_ptr<ExprNode> parseEquality(bool inLoopHeader) {
        auto left = parseComparison(inLoopHeader);
        while (check(TokenType::EQ) || check(TokenType::NEQ)) {
            std::string op = advance().text;
            auto right = parseComparison(inLoopHeader);
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> parseComparison(bool inLoopHeader) {
        auto left = parseTerm(inLoopHeader);
        while (check(TokenType::LANGLE) || check(TokenType::RANGLE) ||
               check(TokenType::LEQ) || check(TokenType::GEQ)) {
            std::string op = advance().text;
            auto right = parseTerm(inLoopHeader);
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> parseTerm(bool inLoopHeader) {
        auto left = parseFactor(inLoopHeader);
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            std::string op = advance().text;
            auto right = parseFactor(inLoopHeader);
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> parseFactor(bool inLoopHeader) {
        auto left = parseUnary(inLoopHeader);
        while (check(TokenType::STAR) || check(TokenType::SLASH)) {
            std::string op = advance().text;
            auto right = parseUnary(inLoopHeader);
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> parseUnary(bool inLoopHeader) {
        if (match(TokenType::KW_NOT)) {
            auto right = parseUnary(inLoopHeader);
            return std::make_unique<UnaryExpr>("NOT", std::move(right));
        }
        if (match(TokenType::MINUS)) {
            auto right = parseUnary(inLoopHeader);
            return std::make_unique<UnaryExpr>("-", std::move(right));
        }
        return parsePostfix(inLoopHeader);
    }

    std::unique_ptr<ExprNode> parsePostfix(bool inLoopHeader) {
        auto expr = parsePrimary(inLoopHeader);
        while (match(TokenType::LBRACKET)) {
            auto idxExpr = parseExpression(false);
            expect(TokenType::RBRACKET, "Expected ']' after index");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(idxExpr));
        }
        return expr;
    }

    std::unique_ptr<ExprNode> parsePrimary(bool inLoopHeader) {
        if (check(TokenType::INT_LIT)) {
            long long val = std::stoll(advance().text);
            return std::make_unique<LiteralExpr>(Value(val));
        }
        if (check(TokenType::DEC_LIT)) {
            double val = std::stod(advance().text);
            return std::make_unique<LiteralExpr>(Value(val));
        }
        if (match(TokenType::KW_TRUE)) {
            return std::make_unique<LiteralExpr>(Value(true));
        }
        if (match(TokenType::KW_FALSE)) {
            return std::make_unique<LiteralExpr>(Value(false));
        }

        // Grouped expression using ! ... !
        if (match(TokenType::BANG)) {
            auto inner = parseExpression(false); // Inside ! ... !, calls are unambiguous
            expect(TokenType::BANG, "Expected closing '!' for grouped expression");
            return inner;
        }

        // Array literal [...]
        if (match(TokenType::LBRACKET)) {
            std::vector<std::unique_ptr<ExprNode>> elems;
            if (!check(TokenType::RBRACKET)) {
                elems.push_back(parseExpression(false));
                while (match(TokenType::COMMA)) {
                    elems.push_back(parseExpression(false));
                }
            }
            expect(TokenType::RBRACKET, "Expected ']' to close array literal");
            return std::make_unique<ArrayLiteralExpr>(std::move(elems));
        }

        // Dictionary literal {key: val, ...}
        if (match(TokenType::LBRACE)) {
            std::vector<std::pair<std::string, std::unique_ptr<ExprNode>>> entries;
            if (!check(TokenType::RBRACE)) {
                do {
                    std::string key;
                    if (check(TokenType::IDENT) || check(TokenType::INT_LIT) || check(TokenType::DEC_LIT)) {
                        key = advance().text;
                    } else {
                        throw std::runtime_error("Syntax Error on line " + std::to_string(peek().line) +
                                                 ": Expected dictionary key");
                    }
                    expect(TokenType::COLON, "Expected ':' after dictionary key '" + key + "'");
                    auto valExpr = parseExpression(false);
                    entries.emplace_back(key, std::move(valExpr));
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RBRACE, "Expected '}' to close dictionary literal");
            return std::make_unique<DictLiteralExpr>(std::move(entries));
        }

        // Identifier or Function Call
        if (check(TokenType::IDENT)) {
            std::string name = advance().text;
            // Function call: funcName!arg1, arg2! (not triggered at top-level of LOOP header)
            if (!inLoopHeader && match(TokenType::BANG)) {
                std::vector<std::unique_ptr<ExprNode>> args;
                if (!check(TokenType::BANG)) {
                    args.push_back(parseExpression(false));
                    while (match(TokenType::COMMA)) {
                        args.push_back(parseExpression(false));
                    }
                }
                expect(TokenType::BANG, "Expected '!' after function call arguments");
                return std::make_unique<CallExpr>(name, std::move(args));
            }
            return std::make_unique<VarExpr>(name, true);
        }

        // Punctuation tokens inside unquoted PRINT text
        if (check(TokenType::SYMBOL) || check(TokenType::COLON) || check(TokenType::COMMA)) {
            return std::make_unique<LiteralExpr>(Value(advance().text));
        }

        throw std::runtime_error("Syntax Error on line " + std::to_string(peek().line) +
                                 ": Unexpected token '" + peek().text + "' in expression");
    }
};

// ============================================================================
// 6. MAIN ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <script_file>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << argv[1] << "'" << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        Parser parser(tokens, source);
        auto program = parser.parseProgram();

        auto globalEnv = std::make_shared<Environment>();
        program->execute(globalEnv);
    } catch (const std::exception& ex) {
        std::cerr << "poolang Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
