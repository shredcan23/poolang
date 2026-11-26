#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <cctype>
#include <stdexcept>
#include <cmath>

// Forward declarations
struct Value;
struct BlockNode;
class Environment;

using ArrayType = std::vector<Value>;
using DictType = std::unordered_map<std::string, Value>;

struct FuncDef {
    std::vector<std::string> params;
    std::shared_ptr<BlockNode> body;
};

// --- 1. DYNAMIC VALUE SYSTEM ---
struct Value {
    using VariantType = std::variant<
        std::monostate,
        int,
        double,
        std::string,
        bool,
        std::shared_ptr<ArrayType>,
        std::shared_ptr<DictType>
    >;

    VariantType data;

    Value() : data(std::monostate{}) {}
    Value(int v) : data(v) {}
    Value(double v) : data(v) {}
    Value(std::string v) : data(std::move(v)) {}
    Value(const char* v) : data(std::string(v)) {}
    Value(bool v) : data(v) {}
    Value(std::shared_ptr<ArrayType> v) : data(std::move(v)) {}
    Value(std::shared_ptr<DictType> v) : data(std::move(v)) {}

    std::string toString() const {
        if (std::holds_alternative<std::monostate>(data)) return "void";
        if (auto* v = std::get_if<int>(&data)) return std::to_string(*v);
        if (auto* v = std::get_if<double>(&data)) {
            std::ostringstream oss;
            oss << *v;
            return oss.str();
        }
        if (auto* v = std::get_if<std::string>(&data)) return *v;
        if (auto* v = std::get_if<bool>(&data)) return *v ? "true" : "false";
        if (auto* v = std::get_if<std::shared_ptr<ArrayType>>(&data)) {
            std::string out = "[";
            for (size_t i = 0; i < (*v)->size(); ++i) {
                out += (**v)[i].toString() + (i + 1 < (*v)->size() ? ", " : "");
            }
            return out + "]";
        }
        if (auto* v = std::get_if<std::shared_ptr<DictType>>(&data)) {
            std::string out = "{";
            size_t i = 0;
            for (const auto& [k, val] : **v) {
                out += k + ": " + val.toString() + (++i < (*v)->size() ? ", " : "");
            }
            return out + "}";
        }
        return "";
    }

    bool toBool() const {
        if (std::holds_alternative<std::monostate>(data)) return false;
        if (auto* v = std::get_if<bool>(&data)) return *v;
        if (auto* v = std::get_if<int>(&data)) return *v != 0;
        if (auto* v = std::get_if<double>(&data)) return *v != 0.0;
        if (auto* v = std::get_if<std::string>(&data)) return !v->empty() && *v != "false";
        if (auto* v = std::get_if<std::shared_ptr<ArrayType>>(&data)) return !(*v)->empty();
        if (auto* v = std::get_if<std::shared_ptr<DictType>>(&data)) return !(*v)->empty();
        return false;
    }

    double toDouble() const {
        if (auto* v = std::get_if<int>(&data)) return static_cast<double>(*v);
        if (auto* v = std::get_if<double>(&data)) return *v;
        if (auto* v = std::get_if<bool>(&data)) return *v ? 1.0 : 0.0;
        if (auto* v = std::get_if<std::string>(&data)) return std::stod(*v);
        throw std::runtime_error("Cannot convert value to number: " + toString());
    }

    int toInt() const {
        if (auto* v = std::get_if<int>(&data)) return *v;
        if (auto* v = std::get_if<double>(&data)) return static_cast<int>(*v);
        if (auto* v = std::get_if<bool>(&data)) return *v ? 1 : 0;
        if (auto* v = std::get_if<std::string>(&data)) return std::stoi(*v);
        throw std::runtime_error("Cannot convert value to NUM: " + toString());
    }

    bool isNumber() const {
        return std::holds_alternative<int>(data) || std::holds_alternative<double>(data);
    }

    bool equals(const Value& other) const {
        if (isNumber() && other.isNumber()) {
            return std::abs(toDouble() - other.toDouble()) < 1e-9;
        }
        if (std::holds_alternative<bool>(data) && std::holds_alternative<bool>(other.data)) {
            return std::get<bool>(data) == std::get<bool>(other.data);
        }
        return toString() == other.toString();
    }
};

struct ReturnSignal {
    Value value;
};

// --- 2. SCOPED ENVIRONMENT ---
class Environment : public std::enable_shared_from_this<Environment> {
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, Value> variables;
    std::unordered_map<std::string, std::string> types;
    std::unordered_map<std::string, FuncDef> functions;

public:
    Environment(std::shared_ptr<Environment> p = nullptr) : parent(std::move(p)) {}

    void define(const std::string& type, const std::string& name, const Value& val) {
        types[name] = type;
        variables[name] = coerce(type, val);
    }

    void assign(const std::string& name, const Value& val) {
        if (variables.count(name)) {
            variables[name] = coerce(types[name], val);
            return;
        }
        if (parent) {
            parent->assign(name, val);
            return;
        }
        types[name] = "STR";
        variables[name] = val;
    }

    bool exists(const std::string& name) const {
        if (variables.count(name)) return true;
        return parent ? parent->exists(name) : false;
    }

    Value get(const std::string& name) const {
        auto it = variables.find(name);
        if (it != variables.end()) return it->second;
        if (parent) return parent->get(name);
        throw std::runtime_error("Undefined variable: " + name);
    }

    void defineFunc(const std::string& name, FuncDef fn) {
        functions[name] = std::move(fn);
    }

    FuncDef getFunc(const std::string& name) const {
        auto it = functions.find(name);
        if (it != functions.end()) return it->second;
        if (parent) return parent->getFunc(name);
        throw std::runtime_error("Undefined function: " + name);
    }

    static Value coerce(const std::string& type, const Value& val) {
        if (type == "NUM") return Value(val.toInt());
        if (type == "DEC") return Value(val.toDouble());
        if (type == "TOF") return Value(val.toBool());
        if (type == "STR") return Value(val.toString());
        if (type == "ARR") {
            if (std::holds_alternative<std::shared_ptr<ArrayType>>(val.data)) return val;
            throw std::runtime_error("Expected ARR value");
        }
        if (type == "KEY") {
            if (std::holds_alternative<std::shared_ptr<DictType>>(val.data)) return val;
            throw std::runtime_error("Expected KEY value");
        }
        throw std::runtime_error("Unknown type: " + type);
    }
};

// --- 3. LEXER ---
enum class TokenType { WORD, SYMBOL, END };

struct Token {
    TokenType type;
    std::string value;
};

class Lexer {
    std::string src;
    size_t pos = 0;

    static bool isSymbolChar(char c) {
        // Parentheses '(' and ')' removed; '!' is now the block/group delimiter
        return std::string(";<>=,[]{}:+-*/!").find(c) != std::string::npos;
    }

public:
    explicit Lexer(std::string s) : src(std::move(s)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos < src.size()) {
            if (std::isspace(static_cast<unsigned char>(src[pos]))) {
                pos++;
                continue;
            }
            if (src[pos] == '#') {
                while (pos < src.size() && src[pos] != '\n') pos++;
                continue;
            }

            char c = src[pos];
            if (pos + 1 < src.size()) {
                std::string two = src.substr(pos, 2);
                if (two == "<=" || two == ">=" || two == "!=") {
                    tokens.push_back({TokenType::SYMBOL, two});
                    pos += 2;
                    continue;
                }
            }

            if (isSymbolChar(c)) {
                tokens.push_back({TokenType::SYMBOL, std::string(1, c)});
                pos++;
                continue;
            }

            std::string word;
            while (pos < src.size() &&
                   !std::isspace(static_cast<unsigned char>(src[pos])) &&
                   !isSymbolChar(src[pos])) {
                word += src[pos++];
            }
            tokens.push_back({TokenType::WORD, word});
        }
        tokens.push_back({TokenType::END, ""});
        return tokens;
    }
};

// --- 4. AST NODES (EXPRESSIONS & STATEMENTS) ---
struct ExprNode {
    virtual ~ExprNode() = default;
    virtual Value eval(std::shared_ptr<Environment> env) = 0;
};

struct LiteralExpr : ExprNode {
    Value val;
    explicit LiteralExpr(Value v) : val(std::move(v)) {}
    Value eval(std::shared_ptr<Environment>) override { return val; }
};

struct VarOrWordExpr : ExprNode {
    std::string name;
    explicit VarOrWordExpr(std::string n) : name(std::move(n)) {}
    Value eval(std::shared_ptr<Environment> env) override {
        if (env->exists(name)) return env->get(name);
        return Value(name);
    }
};

struct ArrayLiteralExpr : ExprNode {
    std::vector<std::unique_ptr<ExprNode>> elements;
    Value eval(std::shared_ptr<Environment> env) override {
        auto arr = std::make_shared<ArrayType>();
        for (auto& el : elements) arr->push_back(el->eval(env));
        return Value(arr);
    }
};

struct DictLiteralExpr : ExprNode {
    std::vector<std::pair<std::string, std::unique_ptr<ExprNode>>> entries;
    Value eval(std::shared_ptr<Environment> env) override {
        auto dict = std::make_shared<DictType>();
        for (auto& [k, v] : entries) (*dict)[k] = v->eval(env);
        return Value(dict);
    }
};

struct IndexExpr : ExprNode {
    std::unique_ptr<ExprNode> target;
    std::unique_ptr<ExprNode> index;
    IndexExpr(std::unique_ptr<ExprNode> t, std::unique_ptr<ExprNode> i)
        : target(std::move(t)), index(std::move(i)) {}

    Value eval(std::shared_ptr<Environment> env) override {
        Value container = target->eval(env);
        Value idx = index->eval(env);

        if (auto* arr = std::get_if<std::shared_ptr<ArrayType>>(&container.data)) {
            int i = idx.toInt();
            if (i < 0 || static_cast<size_t>(i) >= (*arr)->size()) {
                throw std::runtime_error("ARR index out of bounds: " + std::to_string(i));
            }
            return (**arr)[i];
        }
        if (auto* dict = std::get_if<std::shared_ptr<DictType>>(&container.data)) {
            std::string key = idx.toString();
            if (!(*dict)->count(key)) {
                throw std::runtime_error("KEY not found: " + key);
            }
            return (**dict)[key];
        }
        throw std::runtime_error("Target is not an ARR or KEY");
    }
};

struct CallExpr : ExprNode {
    std::string funcName;
    std::vector<std::unique_ptr<ExprNode>> args;
    CallExpr(std::string name, std::vector<std::unique_ptr<ExprNode>> a)
        : funcName(std::move(name)), args(std::move(a)) {}

    Value eval(std::shared_ptr<Environment> env) override {
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
            if (std::holds_alternative<int>(argVal.data)) inferredType = "NUM";
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
};

struct UnaryExpr : ExprNode {
    std::string op;
    std::unique_ptr<ExprNode> right;
    UnaryExpr(std::string o, std::unique_ptr<ExprNode> r) : op(std::move(o)), right(std::move(r)) {}
    Value eval(std::shared_ptr<Environment> env) override {
        Value v = right->eval(env);
        if (op == "NOT") return Value(!v.toBool());
        if (op == "-") {
            if (std::holds_alternative<int>(v.data)) return Value(-std::get<int>(v.data));
            return Value(-v.toDouble());
        }
        return v;
    }
};

struct BinaryExpr : ExprNode {
    std::string op;
    std::unique_ptr<ExprNode> left, right;
    BinaryExpr(std::string o, std::unique_ptr<ExprNode> l, std::unique_ptr<ExprNode> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}

    Value eval(std::shared_ptr<Environment> env) override {
        Value l = left->eval(env);
        Value r = right->eval(env);

        if (op == "=") return Value(l.equals(r));
        if (op == "!=") return Value(!l.equals(r));
        if (op == "<") return Value(l.toDouble() < r.toDouble());
        if (op == ">") return Value(l.toDouble() > r.toDouble());
        if (op == "<=") return Value(l.toDouble() <= r.toDouble());
        if (op == ">=") return Value(l.toDouble() >= r.toDouble());

        if (op == "+") {
            if (l.isNumber() && r.isNumber()) {
                if (std::holds_alternative<double>(l.data) || std::holds_alternative<double>(r.data))
                    return Value(l.toDouble() + r.toDouble());
                return Value(l.toInt() + r.toInt());
            }
            return Value(l.toString() + " " + r.toString());
        }
        if (op == "-") {
            if (std::holds_alternative<double>(l.data) || std::holds_alternative<double>(r.data))
                return Value(l.toDouble() - r.toDouble());
            return Value(l.toInt() - r.toInt());
        }
        if (op == "*") {
            if (std::holds_alternative<double>(l.data) || std::holds_alternative<double>(r.data))
                return Value(l.toDouble() * r.toDouble());
            return Value(l.toInt() * r.toInt());
        }
        if (op == "/") {
            double denom = r.toDouble();
            if (denom == 0.0) throw std::runtime_error("Division by zero");
            if (std::holds_alternative<int>(l.data) && std::holds_alternative<int>(r.data))
                return Value(l.toInt() / r.toInt());
            return Value(l.toDouble() / denom);
        }
        throw std::runtime_error("Unknown operator: " + op);
    }
};

struct StmtNode {
    virtual ~StmtNode() = default;
    virtual void execute(std::shared_ptr<Environment> env) = 0;
};

struct BlockNode : StmtNode {
    std::vector<std::unique_ptr<StmtNode>> statements;
    void execute(std::shared_ptr<Environment> env) override {
        for (auto& stmt : statements) {
            stmt->execute(env);
        }
    }
};

struct MakeStmt : StmtNode {
    std::string type, name;
    std::unique_ptr<ExprNode> expr;
    void execute(std::shared_ptr<Environment> env) override {
        env->define(type, name, expr->eval(env));
    }
};

struct AssignStmt : StmtNode {
    std::string name;
    std::unique_ptr<ExprNode> indexExpr;
    std::unique_ptr<ExprNode> valueExpr;

    void execute(std::shared_ptr<Environment> env) override {
        Value val = valueExpr->eval(env);
        if (!indexExpr) {
            env->assign(name, val);
            return;
        }
        Value container = env->get(name);
        Value idx = indexExpr->eval(env);
        if (auto* arr = std::get_if<std::shared_ptr<ArrayType>>(&container.data)) {
            int i = idx.toInt();
            if (i < 0 || static_cast<size_t>(i) >= (*arr)->size())
                throw std::runtime_error("ARR index out of bounds");
            (**arr)[i] = val;
        } else if (auto* dict = std::get_if<std::shared_ptr<DictType>>(&container.data)) {
            (**dict)[idx.toString()] = val;
        } else {
            throw std::runtime_error(name + " is not an ARR or KEY");
        }
    }
};

struct PrintStmt : StmtNode {
    std::vector<std::unique_ptr<ExprNode>> items;
    void execute(std::shared_ptr<Environment> env) override {
        for (size_t i = 0; i < items.size(); ++i) {
            std::cout << items[i]->eval(env).toString() << (i + 1 < items.size() ? " " : "");
        }
        std::cout << std::endl;
    }
};

struct InStmt : StmtNode {
    std::string varName;
    std::string prompt;
    void execute(std::shared_ptr<Environment> env) override {
        if (!prompt.empty()) {
            std::cout << prompt << " " << std::flush;
        }
        std::string line;
        std::getline(std::cin, line);
        env->assign(varName, Value(line));
    }
};

struct ConditionBranch {
    std::unique_ptr<ExprNode> cond;
    std::shared_ptr<BlockNode> body;
};

struct ConditionStmt : StmtNode {
    std::vector<ConditionBranch> branches;
    std::shared_ptr<BlockNode> voidBranch;

    void execute(std::shared_ptr<Environment> env) override {
        for (auto& br : branches) {
            if (br.cond->eval(env).toBool()) {
                br.body->execute(env);
                return;
            }
        }
        if (voidBranch) {
            voidBranch->execute(env);
        }
    }
};

struct LoopStmt : StmtNode {
    bool isWhile = false;
    std::string iterVar;
    std::unique_ptr<ExprNode> targetExpr;
    std::shared_ptr<BlockNode> body;

    void execute(std::shared_ptr<Environment> env) override {
        if (isWhile) {
            while (targetExpr->eval(env).toBool()) {
                body->execute(env);
            }
        } else {
            Value target = targetExpr->eval(env);
            if (auto* arr = std::get_if<std::shared_ptr<ArrayType>>(&target.data)) {
                for (const auto& item : **arr) {
                    env->assign(iterVar, item);
                    body->execute(env);
                }
            } else {
                int limit = target.toInt();
                for (int i = 0; i < limit; ++i) {
                    env->define("NUM", iterVar, Value(i));
                    body->execute(env);
                }
            }
        }
    }
};

struct FuncStmt : StmtNode {
    std::string name;
    std::vector<std::string> params;
    std::shared_ptr<BlockNode> body;

    void execute(std::shared_ptr<Environment> env) override {
        env->defineFunc(name, FuncDef{params, body});
    }
};

struct ReturnStmt : StmtNode {
    std::unique_ptr<ExprNode> expr;
    void execute(std::shared_ptr<Environment> env) override {
        Value val = expr ? expr->eval(env) : Value();
        throw ReturnSignal{val};
    }
};

struct ExprStmt : StmtNode {
    std::unique_ptr<ExprNode> expr;
    void execute(std::shared_ptr<Environment> env) override {
        expr->eval(env);
    }
};

// --- 5. RECURSIVE DESCENT PARSER ---
class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;

    const Token& peek(size_t offset = 0) const {
        if (pos + offset >= tokens.size()) return tokens.back();
        return tokens[pos + offset];
    }

    Token advance() {
        Token t = peek();
        if (pos < tokens.size() - 1) pos++;
        return t;
    }

    bool match(const std::string& val) {
        if (peek().value == val) {
            advance();
            return true;
        }
        return false;
    }

    void expect(const std::string& val) {
        if (peek().value != val) {
            throw std::runtime_error("Syntax Error: Expected '" + val + "', got '" + peek().value + "'");
        }
        advance();
    }

    void consumeOptionalSemicolon() {
        while (match(";")) {}
    }

    static bool isInt(const std::string& s) {
        if (s.empty()) return false;
        size_t i = (s[0] == '-') ? 1 : 0;
        if (i == s.size()) return false;
        for (; i < s.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        return true;
    }

    static bool isDouble(const std::string& s) {
        bool dot = false;
        size_t i = (s[0] == '-') ? 1 : 0;
        if (i == s.size()) return false;
        for (; i < s.size(); ++i) {
            if (s[i] == '.') {
                if (dot) return false;
                dot = true;
            } else if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
                return false;
            }
        }
        return dot;
    }

    std::unique_ptr<ExprNode> parseExpression() { return parseComparison(); }

    std::unique_ptr<ExprNode> parseComparison() {
        auto left = parseAdditive();
        while (peek().value == "=" || peek().value == "!=" ||
               peek().value == "<" || peek().value == ">" ||
               peek().value == "<=" || peek().value == ">=") {
            std::string op = advance().value;
            auto right = parseAdditive();
            left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> parseAdditive() {
        auto left = parseMultiplicative();
        while (peek().value == "+" || peek().value == "-") {
            std::string op = advance().value;
            auto right = parseMultiplicative();
            left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> parseMultiplicative() {
        auto left = parseUnary();
        while (peek().value == "*" || peek().value == "/") {
            std::string op = advance().value;
            auto right = parseUnary();
            left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
        }
        return left;
    }

    std::unique_ptr<ExprNode> parseUnary() {
        if (peek().value == "-" || peek().value == "NOT") {
            std::string op = advance().value;
            return std::make_unique<UnaryExpr>(op, parseUnary());
        }
        return parsePostfix();
    }

    std::unique_ptr<ExprNode> parsePostfix() {
        auto expr = parsePrimary();
        while (true) {
            if (match("[")) {
                auto idx = parseExpression();
                expect("]");
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(idx));
            } else if (auto* varNode = dynamic_cast<VarOrWordExpr*>(expr.get());
                       varNode && match("!")) {
                std::string fnName = varNode->name;
                std::vector<std::unique_ptr<ExprNode>> args;
                if (peek().value != "!") {
                    do {
                        args.push_back(parseExpression());
                    } while (match(","));
                }
                expect("!");
                expr = std::make_unique<CallExpr>(fnName, std::move(args));
            } else {
                break;
            }
        }
        return expr;
    }

    std::unique_ptr<ExprNode> parsePrimary() {
        if (match("!")) {
            auto expr = parseExpression();
            expect("!");
            return expr;
        }
        if (match("[")) {
            auto arr = std::make_unique<ArrayLiteralExpr>();
            if (peek().value != "]") {
                do {
                    arr->elements.push_back(parseExpression());
                } while (match(","));
            }
            expect("]");
            return arr;
        }
        if (match("{")) {
            auto dict = std::make_unique<DictLiteralExpr>();
            if (peek().value != "}") {
                do {
                    std::string key = advance().value;
                    expect(":");
                    auto val = parseExpression();
                    dict->entries.emplace_back(key, std::move(val));
                } while (match(","));
            }
            expect("}");
            return dict;
        }

        Token tok = advance();
        if (tok.value == "true") return std::make_unique<LiteralExpr>(Value(true));
        if (tok.value == "false") return std::make_unique<LiteralExpr>(Value(false));
        if (isInt(tok.value)) return std::make_unique<LiteralExpr>(Value(std::stoi(tok.value)));
        if (isDouble(tok.value)) return std::make_unique<LiteralExpr>(Value(std::stod(tok.value)));
        return std::make_unique<VarOrWordExpr>(tok.value);
    }

    std::unique_ptr<StmtNode> parseStatement() {
        if (match("MAKE")) {
            auto node = std::make_unique<MakeStmt>();
            expect("<");
            node->type = advance().value;
            expect(">");
            node->name = advance().value;
            expect("ASSIGN");

            if (node->type == "STR") {
                if (peek(1).value == "!" || peek(1).value == "[") {
                    node->expr = parseExpression();
                } else {
                    std::vector<Token> raw;
                    while (peek().type != TokenType::END && peek().value != ";") {
                        raw.push_back(advance());
                    }
                    if (raw.size() == 1) {
                        node->expr = std::make_unique<VarOrWordExpr>(raw[0].value);
                    } else {
                        std::string joined;
                        for (size_t i = 0; i < raw.size(); ++i) {
                            joined += raw[i].value + (i + 1 < raw.size() ? " " : "");
                        }
                        node->expr = std::make_unique<LiteralExpr>(Value(joined));
                    }
                }
            } else {
                node->expr = parseExpression();
            }
            expect(";");
            return node;
        }

        if (match("PRINT")) {
            auto node = std::make_unique<PrintStmt>();
            bool hasOperator = false;
            for (size_t i = 0; peek(i).type != TokenType::END && peek(i).value != ";"; ++i) {
                const std::string& v = peek(i).value;
                if (v == "+" || v == "-" || v == "*" || v == "/" || v == "NOT" ||
                    v == "=" || v == "!=" || v == "<" || v == ">" || v == "<=" || v == ">=") {
                    hasOperator = true;
                    break;
                }
            }

            if (hasOperator) {
                node->items.push_back(parseExpression());
            } else {
                while (peek().type != TokenType::END && peek().value != ";") {
                    if (peek(1).value == "!" || peek(1).value == "[") {
                        node->items.push_back(parsePostfix());
                    } else {
                        node->items.push_back(parsePrimary());
                    }
                }
            }
            expect(";");
            return node;
        }

        if (match("IN")) {
            auto node = std::make_unique<InStmt>();
            node->varName = advance().value;
            expect("ASSIGN");
            std::string prompt;
            while (peek().type != TokenType::END && peek().value != ";") {
                prompt += (prompt.empty() ? "" : " ") + advance().value;
            }
            node->prompt = prompt;
            expect(";");
            return node;
        }

        if (match("CONDITION")) {
            auto node = std::make_unique<ConditionStmt>();
            ConditionBranch first;
            first.cond = parseExpression();
            expect("IS");
            expect("!");
            first.body = parseBlock(true);
            expect("!");
            node->branches.push_back(std::move(first));

            while (peek().value == "CONDITION" && peek(1).value == "NEXT") {
                advance();
                advance();
                ConditionBranch nextBr;
                nextBr.cond = parseExpression();
                expect("IS");
                expect("!");
                nextBr.body = parseBlock(true);
                expect("!");
                node->branches.push_back(std::move(nextBr));
            }

            if (peek().value == "CONDITION" && peek(1).value == "VOID") {
                advance();
                advance();
                match("IS");
                expect("!");
                node->voidBranch = parseBlock(true);
                expect("!");
            }

            consumeOptionalSemicolon();
            return node;
        }

        if (match("LOOP")) {
            auto node = std::make_unique<LoopStmt>();
            if (match("FOR")) {
                node->isWhile = false;
                node->iterVar = advance().value;
                expect(",");
                node->targetExpr = parseExpression();
                expect("!");
                node->body = parseBlock(true);
                expect("!");
            } else if (match("WHILE")) {
                node->isWhile = true;
                node->targetExpr = parseExpression();
                expect("!");
                node->body = parseBlock(true);
                expect("!");
            } else {
                throw std::runtime_error("Expected FOR or WHILE after LOOP");
            }
            consumeOptionalSemicolon();
            return node;
        }

        if (match("FUNC")) {
            auto node = std::make_unique<FuncStmt>();
            node->name = advance().value;
            if (match("PAR")) {
                expect("!");
                if (peek().value != "!") {
                    do {
                        node->params.push_back(advance().value);
                    } while (match(","));
                }
                expect("!");
            }
            expect("!");
            node->body = parseBlock(true);
            expect("!");
            consumeOptionalSemicolon();
            return node;
        }

        if (match("RETURN")) {
            auto node = std::make_unique<ReturnStmt>();
            if (peek().value != ";") {
                node->expr = parseExpression();
            }
            expect(";");
            return node;
        }

        if (match("CALL")) {
            auto node = std::make_unique<ExprStmt>();
            node->expr = parsePostfix();
            expect(";");
            return node;
        }

        if (peek(1).value == "ASSIGN") {
            auto node = std::make_unique<AssignStmt>();
            node->name = advance().value;
            expect("ASSIGN");
            node->valueExpr = parseExpression();
            expect(";");
            return node;
        }
        if (peek(1).value == "[") {
            auto node = std::make_unique<AssignStmt>();
            node->name = advance().value;
            expect("[");
            node->indexExpr = parseExpression();
            expect("]");
            expect("ASSIGN");
            node->valueExpr = parseExpression();
            expect(";");
            return node;
        }

        auto node = std::make_unique<ExprStmt>();
        node->expr = parseExpression();
        expect(";");
        return node;
    }

public:
    explicit Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

    std::shared_ptr<BlockNode> parseBlock(bool stopAtBang = false) {
        auto block = std::make_shared<BlockNode>();
        while (peek().type != TokenType::END) {
            if (stopAtBang && peek().value == "!") break;
            if (match(";")) continue;
            block->statements.push_back(parseStatement());
        }
        return block;
    }
};

// --- 6. FILE-LOADING ENTRY POINT ---
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./mylang <filename>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << argv[1] << std::endl;
        return 1;
    }

    std::ostringstream codeBuffer;
    codeBuffer << file.rdbuf();

    try {
        Lexer lexer(codeBuffer.str());
        Parser parser(lexer.tokenize());
        auto ast = parser.parseBlock(false);

        auto globalEnv = std::make_shared<Environment>();
        ast->execute(globalEnv);
    } catch (const ReturnSignal&) {
        std::cerr << "Runtime Error: RETURN used outside of a FUNC." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
