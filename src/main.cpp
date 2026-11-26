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
#include <unordered_set>
#include <vector>

#include "poographic.hpp"
#include "poographicl.hpp"

// ============================================================================
// 1. VALUE REPRESENTATION
// ============================================================================

enum class ValType { NUM, STR, TOF, VOID };

struct Value {
    ValType type = ValType::VOID;
    double num = 0.0;
    std::string str;
    bool tof = false;

    static Value makeNum(double v) {
        Value val;
        val.type = ValType::NUM;
        val.num = v;
        return val;
    }

    static Value makeStr(const std::string& s) {
        Value val;
        val.type = ValType::STR;
        val.str = s;
        return val;
    }

    static Value makeTof(bool b) {
        Value val;
        val.type = ValType::TOF;
        val.tof = b;
        return val;
    }

    static Value makeVoid() {
        return Value{};
    }

    bool isTruthy() const {
        if (type == ValType::TOF) return tof;
        if (type == ValType::NUM) return num != 0.0;
        if (type == ValType::STR) return !str.empty() && str != "false";
        return false;
    }

    double asNumber() const {
        if (type == ValType::NUM) return num;
        if (type == ValType::TOF) return tof ? 1.0 : 0.0;
        if (type == ValType::STR) {
            try {
                return std::stod(str);
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    }

    std::string asString() const {
        if (type == ValType::STR) return str;
        if (type == ValType::TOF) return tof ? "true" : "false";
        if (type == ValType::NUM) {
            if (std::floor(num) == num && std::abs(num) < 1e15) {
                return std::to_string(static_cast<long long>(num));
            }
            std::ostringstream oss;
            oss << num;
            return oss.str();
        }
        return "";
    }
};

// ============================================================================
// 2. LEXER
// ============================================================================

enum class TokenType {
    IDENT,
    NUMBER,
    STRING,
    BANG,       // !
    SEMI,       // ;
    COMMA,      // ,
    LPAREN,     // (
    RPAREN,     // )
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    MOD,        // %
    LT,         // <
    GT,         // >
    LE,         // <=
    GE,         // >=
    EQ,         // ==
    NEQ,        // !=
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string text;
    int line;
};

class Lexer {
public:
    explicit Lexer(const std::string& src) : src_(src) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < src_.size()) {
            char c = src_[pos_];

            if (c == '\n') {
                ++line_;
                ++pos_;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++pos_;
                continue;
            }

            // Comments: // or #
            if ((c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') || c == '#') {
                while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
                continue;
            }

            // Strings
            if (c == '"' || c == '\'') {
                char quote = c;
                ++pos_;
                std::string strVal;
                while (pos_ < src_.size() && src_[pos_] != quote) {
                    if (src_[pos_] == '\\' && pos_ + 1 < src_.size()) {
                        ++pos_;
                        char esc = src_[pos_];
                        if (esc == 'n') strVal += '\n';
                        else if (esc == 't') strVal += '\t';
                        else strVal += esc;
                    } else {
                        if (src_[pos_] == '\n') ++line_;
                        strVal += src_[pos_];
                    }
                    ++pos_;
                }
                if (pos_ < src_.size()) ++pos_;
                tokens.push_back({TokenType::STRING, strVal, line_});
                continue;
            }

            // Numbers
            if (std::isdigit(static_cast<unsigned char>(c))) {
                size_t start = pos_;
                while (pos_ < src_.size() &&
                       (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.')) {
                    ++pos_;
                }
                tokens.push_back({TokenType::NUMBER, src_.substr(start, pos_ - start), line_});
                continue;
            }

            // Identifiers (supports dots for POOGRAPHIC.KEY.HOLD, etc.)
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                size_t start = pos_;
                while (pos_ < src_.size() &&
                       (std::isalnum(static_cast<unsigned char>(src_[pos_])) ||
                        src_[pos_] == '_' || src_[pos_] == '.')) {
                    ++pos_;
                }
                tokens.push_back({TokenType::IDENT, src_.substr(start, pos_ - start), line_});
                continue;
            }

            // Two-char operators
            if (pos_ + 1 < src_.size()) {
                std::string two = src_.substr(pos_, 2);
                if (two == "<=") { tokens.push_back({TokenType::LE, two, line_}); pos_ += 2; continue; }
                if (two == ">=") { tokens.push_back({TokenType::GE, two, line_}); pos_ += 2; continue; }
                if (two == "==") { tokens.push_back({TokenType::EQ, two, line_}); pos_ += 2; continue; }
                if (two == "!=") { tokens.push_back({TokenType::NEQ, two, line_}); pos_ += 2; continue; }
            }

            // Single-char symbols
            ++pos_;
            switch (c) {
                case '!': tokens.push_back({TokenType::BANG, "!", line_}); break;
                case ';': tokens.push_back({TokenType::SEMI, ";", line_}); break;
                case ',': tokens.push_back({TokenType::COMMA, ",", line_}); break;
                case '(': tokens.push_back({TokenType::LPAREN, "(", line_}); break;
                case ')': tokens.push_back({TokenType::RPAREN, ")", line_}); break;
                case '+': tokens.push_back({TokenType::PLUS, "+", line_}); break;
                case '-': tokens.push_back({TokenType::MINUS, "-", line_}); break;
                case '*': tokens.push_back({TokenType::STAR, "*", line_}); break;
                case '/': tokens.push_back({TokenType::SLASH, "/", line_}); break;
                case '%': tokens.push_back({TokenType::MOD, "%", line_}); break;
                case '<': tokens.push_back({TokenType::LT, "<", line_}); break;
                case '>': tokens.push_back({TokenType::GT, ">", line_}); break;
                default:
                    throw std::runtime_error("Line " + std::to_string(line_) +
                                             ": Unexpected character '" + std::string(1, c) + "'");
            }
        }

        tokens.push_back({TokenType::END_OF_FILE, "", line_});
        return tokens;
    }

private:
    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
};

// ============================================================================
// 3. AST DEFINITIONS
// ============================================================================

struct Expr {
    virtual ~Expr() = default;
};
using ExprPtr = std::shared_ptr<Expr>;

struct Stmt {
    virtual ~Stmt() = default;
};
using StmtPtr = std::shared_ptr<Stmt>;

struct LiteralExpr : Expr {
    Value val;
    explicit LiteralExpr(Value v) : val(std::move(v)) {}
};

struct IdentExpr : Expr {
    std::string name;
    explicit IdentExpr(std::string n) : name(std::move(n)) {}
};

struct UnaryExpr : Expr {
    std::string op;
    ExprPtr right;
    UnaryExpr(std::string o, ExprPtr r) : op(std::move(o)), right(std::move(r)) {}
};

struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;
    BinaryExpr(std::string o, ExprPtr l, ExprPtr r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    CallExpr(std::string c, std::vector<ExprPtr> a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
};

struct ImportStmt : Stmt {
    std::string moduleName;
    explicit ImportStmt(std::string m) : moduleName(std::move(m)) {}
};

struct MakeStmt : Stmt {
    ValType declType;
    std::string varName;
    ExprPtr initExpr;
    MakeStmt(ValType t, std::string n, ExprPtr e)
        : declType(t), varName(std::move(n)), initExpr(std::move(e)) {}
};

struct AssignStmt : Stmt {
    std::string varName;
    ExprPtr expr;
    AssignStmt(std::string n, ExprPtr e) : varName(std::move(n)), expr(std::move(e)) {}
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct PrintStmt : Stmt {
    std::vector<ExprPtr> args;
    explicit PrintStmt(std::vector<ExprPtr> a) : args(std::move(a)) {}
};

struct ConditionStmt : Stmt {
    ExprPtr cond;
    std::shared_ptr<BlockStmt> thenBlock;
    std::shared_ptr<BlockStmt> elseBlock;
};

struct LoopStmt : Stmt {
    ExprPtr cond;
    std::shared_ptr<BlockStmt> body;
};

struct FuncParam {
    ValType type;
    std::string name;
};

struct FuncDeclStmt : Stmt {
    ValType returnType;
    std::string name;
    std::vector<FuncParam> params;
    std::shared_ptr<BlockStmt> body;
};

struct ReturnStmt : Stmt {
    ExprPtr expr;
    explicit ReturnStmt(ExprPtr e) : expr(std::move(e)) {}
};

// ============================================================================
// 4. PARSER
// ============================================================================

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

    std::vector<StmtPtr> parseProgram() {
        std::vector<StmtPtr> stmts;
        while (!isAtEnd()) {
            if (match(TokenType::SEMI)) continue;
            stmts.push_back(parseStatement());
        }
        return stmts;
    }

private:
    const std::vector<Token>& tokens_;
    size_t pos_ = 0;

    const Token& peek(int offset = 0) const {
        size_t idx = pos_ + static_cast<size_t>(offset);
        if (idx >= tokens_.size()) return tokens_.back();
        return tokens_[idx];
    }

    bool isAtEnd() const {
        return peek().type == TokenType::END_OF_FILE;
    }

    const Token& advance() {
        if (!isAtEnd()) ++pos_;
        return tokens_[pos_ - 1];
    }

    bool check(TokenType t) const {
        return peek().type == t;
    }

    bool checkKeyword(const std::string& kw) const {
        return peek().type == TokenType::IDENT && peek().text == kw;
    }

    bool match(TokenType t) {
        if (check(t)) {
            advance();
            return true;
        }
        return false;
    }

    bool matchKeyword(const std::string& kw) {
        if (checkKeyword(kw)) {
            advance();
            return true;
        }
        return false;
    }

    void expect(TokenType t, const std::string& msg) {
        if (!match(t)) {
            throw std::runtime_error("Line " + std::to_string(peek().line) + ": " + msg +
                                     " (got '" + peek().text + "')");
        }
    }

    ValType parseTypeAnnotation() {
        expect(TokenType::LT, "Expected '<' before type name");
        if (!check(TokenType::IDENT)) {
            throw std::runtime_error("Line " + std::to_string(peek().line) + ": Expected type name");
        }
        std::string tName = advance().text;
        expect(TokenType::GT, "Expected '>' after type name");

        if (tName == "NUM") return ValType::NUM;
        if (tName == "STR") return ValType::STR;
        if (tName == "TOF") return ValType::TOF;
        if (tName == "VOID") return ValType::VOID;
        throw std::runtime_error("Unknown type: <" + tName + ">");
    }

    std::shared_ptr<BlockStmt> parseBangBlock(bool alreadyOpened = false) {
        if (!alreadyOpened) {
            expect(TokenType::BANG, "Expected '!' to open block");
        }
        auto block = std::make_shared<BlockStmt>();
        while (!isAtEnd() && !check(TokenType::BANG)) {
            if (match(TokenType::SEMI)) continue;
            block->statements.push_back(parseStatement());
        }
        expect(TokenType::BANG, "Expected '!' to close block");
        return block;
    }

    StmtPtr parseStatement() {
        if (matchKeyword("IMPORT")) {
            if (!check(TokenType::IDENT)) {
                throw std::runtime_error("Line " + std::to_string(peek().line) +
                                         ": Expected library name after IMPORT");
            }
            std::string mod = advance().text;
            match(TokenType::SEMI);
            return std::make_shared<ImportStmt>(mod);
        }

        if (matchKeyword("MAKE")) {
            ValType vType = parseTypeAnnotation();
            if (!check(TokenType::IDENT)) {
                throw std::runtime_error("Line " + std::to_string(peek().line) +
                                         ": Expected variable name after type");
            }
            std::string varName = advance().text;
            if (!matchKeyword("ASSIGN")) {
                throw std::runtime_error("Line " + std::to_string(peek().line) +
                                         ": Expected ASSIGN in MAKE statement");
            }
            ExprPtr init = parseExpression();
            match(TokenType::SEMI);
            return std::make_shared<MakeStmt>(vType, varName, init);
        }

        if (matchKeyword("CONDITION")) {
            auto stmt = std::make_shared<ConditionStmt>();
            stmt->cond = parseExpression();
            if (!matchKeyword("IS")) {
                throw std::runtime_error("Line " + std::to_string(peek().line) +
                                         ": Expected IS after CONDITION expression");
            }
            stmt->thenBlock = parseBangBlock(false);
            if (matchKeyword("OTHERWISE") || matchKeyword("ELSE")) {
                stmt->elseBlock = parseBangBlock(false);
            }
            match(TokenType::SEMI);
            return stmt;
        }

        if (matchKeyword("LOOP")) {
            if (!matchKeyword("WHILE")) {
                throw std::runtime_error("Line " + std::to_string(peek().line) +
                                         ": Expected WHILE after LOOP");
            }
            auto stmt = std::make_shared<LoopStmt>();
            stmt->cond = parseExpression();
            // Support both "LOOP WHILE ALIVE!! ! ... !" and "LOOP WHILE ALIVE! ! ... !"
            bool openedByCall = !check(TokenType::BANG);
            stmt->body = parseBangBlock(openedByCall);
            match(TokenType::SEMI);
            return stmt;
        }

        if (matchKeyword("FUNC")) {
            ValType retType = ValType::VOID;
            if (check(TokenType::LT)) {
                retType = parseTypeAnnotation();
            }
            if (!check(TokenType::IDENT)) {
                throw std::runtime_error("Line " + std::to_string(peek().line) +
                                         ": Expected function name");
            }
            std::string funcName = advance().text;
            std::vector<FuncParam> params;

            if (match(TokenType::BANG)) {
                if (!match(TokenType::BANG)) {
                    while (!isAtEnd() && !check(TokenType::BANG)) {
                        ValType pType = ValType::NUM;
                        if (check(TokenType::LT)) {
                            pType = parseTypeAnnotation();
                        }
                        if (!check(TokenType::IDENT)) {
                            throw std::runtime_error("Line " + std::to_string(peek().line) +
                                                     ": Expected parameter name");
                        }
                        std::string pName = advance().text;
                        params.push_back({pType, pName});
                        if (!match(TokenType::COMMA)) break;
                    }
                    expect(TokenType::BANG, "Expected '!' after function parameters");
                }
            }

            auto decl = std::make_shared<FuncDeclStmt>();
            decl->returnType = retType;
            decl->name = funcName;
            decl->params = std::move(params);
            decl->body = parseBangBlock(false);
            match(TokenType::SEMI);
            return decl;
        }

        if (matchKeyword("RETURN")) {
            ExprPtr val = nullptr;
            if (!check(TokenType::SEMI) && !check(TokenType::BANG)) {
                val = parseExpression();
            }
            match(TokenType::SEMI);
            return std::make_shared<ReturnStmt>(val);
        }

        if (matchKeyword("PRINT") || matchKeyword("SAY")) {
            std::vector<ExprPtr> args;
            if (match(TokenType::BANG)) {
                if (!match(TokenType::BANG) && !check(TokenType::SEMI)) {
                    while (!isAtEnd() && !check(TokenType::BANG)) {
                        args.push_back(parseExpression());
                        if (!match(TokenType::COMMA)) break;
                    }
                    match(TokenType::BANG);
                }
            } else {
                args.push_back(parseExpression());
            }
            match(TokenType::SEMI);
            return std::make_shared<PrintStmt>(std::move(args));
        }

        // Variable assignment: x ASSIGN expr;
        if (check(TokenType::IDENT) && peek(1).type == TokenType::IDENT && peek(1).text == "ASSIGN") {
            std::string varName = advance().text;
            advance(); // ASSIGN
            ExprPtr expr = parseExpression();
            match(TokenType::SEMI);
            return std::make_shared<AssignStmt>(varName, expr);
        }

        // Expression statement (e.g., POOGRAPHIC.MAKEWINDOW!800, 600!;)
        ExprPtr expr = parseExpression();
        match(TokenType::SEMI);
        return std::make_shared<ExprStmt>(expr);
    }

    ExprPtr parseExpression() {
        return parseOr();
    }

    ExprPtr parseOr() {
        ExprPtr left = parseAnd();
        while (matchKeyword("OR")) {
            ExprPtr right = parseAnd();
            left = std::make_shared<BinaryExpr>("OR", left, right);
        }
        return left;
    }

    ExprPtr parseAnd() {
        ExprPtr left = parseEquality();
        while (matchKeyword("AND")) {
            ExprPtr right = parseEquality();
            left = std::make_shared<BinaryExpr>("AND", left, right);
        }
        return left;
    }

    ExprPtr parseEquality() {
        ExprPtr left = parseComparison();
        while (check(TokenType::EQ) || check(TokenType::NEQ)) {
            std::string op = advance().text;
            ExprPtr right = parseComparison();
            left = std::make_shared<BinaryExpr>(op, left, right);
        }
        return left;
    }

    ExprPtr parseComparison() {
        ExprPtr left = parseAdditive();
        while (check(TokenType::LT) || check(TokenType::GT) ||
               check(TokenType::LE) || check(TokenType::GE)) {
            std::string op = advance().text;
            ExprPtr right = parseAdditive();
            left = std::make_shared<BinaryExpr>(op, left, right);
        }
        return left;
    }

    ExprPtr parseAdditive() {
        ExprPtr left = parseMultiplicative();
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            std::string op = advance().text;
            ExprPtr right = parseMultiplicative();
            left = std::make_shared<BinaryExpr>(op, left, right);
        }
        return left;
    }

    ExprPtr parseMultiplicative() {
        ExprPtr left = parseUnary();
        while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::MOD)) {
            std::string op = advance().text;
            ExprPtr right = parseUnary();
            left = std::make_shared<BinaryExpr>(op, left, right);
        }
        return left;
    }

    ExprPtr parseUnary() {
        if (check(TokenType::MINUS) || checkKeyword("NOT")) {
            std::string op = advance().text;
            return std::make_shared<UnaryExpr>(op, parseUnary());
        }
        return parsePrimary();
    }

    ExprPtr parsePrimary() {
        if (check(TokenType::NUMBER)) {
            double v = std::stod(advance().text);
            return std::make_shared<LiteralExpr>(Value::makeNum(v));
        }

        if (check(TokenType::STRING)) {
            return std::make_shared<LiteralExpr>(Value::makeStr(advance().text));
        }

        if (matchKeyword("true") || matchKeyword("TRUE")) {
            return std::make_shared<LiteralExpr>(Value::makeTof(true));
        }
        if (matchKeyword("false") || matchKeyword("FALSE")) {
            return std::make_shared<LiteralExpr>(Value::makeTof(false));
        }

        if (match(TokenType::LPAREN)) {
            ExprPtr inner = parseExpression();
            expect(TokenType::RPAREN, "Expected ')'");
            return inner;
        }

        if (check(TokenType::IDENT)) {
            std::string name = advance().text;

            // Function / library call: NAME!arg1, arg2! or NAME!! or NAME!;
            if (match(TokenType::BANG)) {
                std::vector<ExprPtr> args;
                // Check for immediate close (NAME!!), statement end (NAME!;), or keyword (CONDITION CLICK! IS)
                if (match(TokenType::BANG)) {
                    return std::make_shared<CallExpr>(name, std::move(args));
                }
                if (check(TokenType::SEMI) || check(TokenType::RPAREN) || check(TokenType::COMMA) ||
                    checkKeyword("IS") || checkKeyword("AND") || checkKeyword("OR")) {
                    return std::make_shared<CallExpr>(name, std::move(args));
                }

                while (!isAtEnd() && !check(TokenType::BANG)) {
                    args.push_back(parseExpression());
                    if (!match(TokenType::COMMA)) break;
                }
                expect(TokenType::BANG, "Expected closing '!' for call to " + name);
                return std::make_shared<CallExpr>(name, std::move(args));
            }

            return std::make_shared<IdentExpr>(name);
        }

        throw std::runtime_error("Line " + std::to_string(peek().line) +
                                 ": Unexpected token '" + peek().text + "'");
    }
};

// ============================================================================
// 5. INTERPRETER / EVALUATOR
// ============================================================================

struct ReturnSignal {
    Value val;
};

struct Environment {
    std::unordered_map<std::string, Value> vars;
    std::shared_ptr<Environment> parent;

    explicit Environment(std::shared_ptr<Environment> p = nullptr) : parent(std::move(p)) {}

    bool has(const std::string& name) const {
        if (vars.count(name)) return true;
        return parent ? parent->has(name) : false;
    }

    Value get(const std::string& name) const {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        if (parent) return parent->get(name);
        return Value::makeVoid();
    }

    void define(const std::string& name, const Value& v) {
        vars[name] = v;
    }

    void assign(const std::string& name, const Value& v) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            it->second = v;
            return;
        }
        if (parent) {
            parent->assign(name, v);
            return;
        }
        vars[name] = v;
    }
};

struct AllTouchWatcher {
    std::vector<ExprPtr> coords; // 8 expressions: x1, y1, w1, h1, x2, y2, w2, h2
    std::string boundVar;        // Optional variable updated automatically each DRAW!
    std::shared_ptr<Environment> env;
};

class Interpreter {
public:
    Interpreter() : globalEnv_(std::make_shared<Environment>()) {}

    void run(const std::vector<StmtPtr>& program) {
        for (const auto& stmt : program) {
            execStmt(stmt, globalEnv_);
        }
    }

private:
    std::shared_ptr<Environment> globalEnv_;
    std::unordered_map<std::string, std::shared_ptr<FuncDeclStmt>> functions_;

    bool hasPoographic_ = false;
    bool hasPoographicl_ = false;
    std::vector<AllTouchWatcher> allTouchWatchers_;
    bool lastAllTouchHit_ = false;

    static std::string toLower(std::string s) {
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }

    void updateAllTouchWatchers() {
        bool anyHit = false;
        for (const auto& w : allTouchWatchers_) {
            if (w.coords.size() < 8) continue;
            double x1 = evalExpr(w.coords[0], w.env).asNumber();
            double y1 = evalExpr(w.coords[1], w.env).asNumber();
            double w1 = evalExpr(w.coords[2], w.env).asNumber();
            double h1 = evalExpr(w.coords[3], w.env).asNumber();
            double x2 = evalExpr(w.coords[4], w.env).asNumber();
            double y2 = evalExpr(w.coords[5], w.env).asNumber();
            double w2 = evalExpr(w.coords[6], w.env).asNumber();
            double h2 = evalExpr(w.coords[7], w.env).asNumber();

            bool hit = poographic::touch(x1, y1, w1, h1, x2, y2, w2, h2);
            if (hit) anyHit = true;
            if (!w.boundVar.empty() && w.env) {
                w.env->assign(w.boundVar, Value::makeTof(hit));
            }
        }
        lastAllTouchHit_ = anyHit;
    }

    void execBlock(const std::shared_ptr<BlockStmt>& block, const std::shared_ptr<Environment>& env) {
        for (const auto& s : block->statements) {
            execStmt(s, env);
        }
    }

    void execStmt(const StmtPtr& stmt, const std::shared_ptr<Environment>& env) {
        if (auto imp = std::dynamic_pointer_cast<ImportStmt>(stmt)) {
            std::string mod = toLower(imp->moduleName);
            if (mod == "poographic") {
                hasPoographic_ = true;
            } else if (mod == "poographicl") {
                hasPoographicl_ = true;
            } else {
                throw std::runtime_error("Unknown library in IMPORT: " + imp->moduleName);
            }
            return;
        }

        if (auto mk = std::dynamic_pointer_cast<MakeStmt>(stmt)) {
            Value val = evalExpr(mk->initExpr, env);
            env->define(mk->varName, val);

            // If initialized with ALLTOUCH, bind the variable so it stays live for the runtime
            if (auto call = std::dynamic_pointer_cast<CallExpr>(mk->initExpr)) {
                if ((call->callee == "POOGRAPHIC.ALLTOUCH" || call->callee == "POOGRAPHICL.ALLTOUCH") &&
                    !allTouchWatchers_.empty()) {
                    allTouchWatchers_.back().boundVar = mk->varName;
                }
            }
            return;
        }

        if (auto asg = std::dynamic_pointer_cast<AssignStmt>(stmt)) {
            Value val = evalExpr(asg->expr, env);
            env->assign(asg->varName, val);

            if (auto call = std::dynamic_pointer_cast<CallExpr>(asg->expr)) {
                if ((call->callee == "POOGRAPHIC.ALLTOUCH" || call->callee == "POOGRAPHICL.ALLTOUCH") &&
                    !allTouchWatchers_.empty()) {
                    allTouchWatchers_.back().boundVar = asg->varName;
                }
            }
            return;
        }

        if (auto pr = std::dynamic_pointer_cast<PrintStmt>(stmt)) {
            for (size_t i = 0; i < pr->args.size(); ++i) {
                if (i > 0) std::cout << " ";
                std::cout << evalExpr(pr->args[i], env).asString();
            }
            std::cout << "\n";
            return;
        }

        if (auto cond = std::dynamic_pointer_cast<ConditionStmt>(stmt)) {
            if (evalExpr(cond->cond, env).isTruthy()) {
                execBlock(cond->thenBlock, env);
            } else if (cond->elseBlock) {
                execBlock(cond->elseBlock, env);
            }
            return;
        }

        if (auto lp = std::dynamic_pointer_cast<LoopStmt>(stmt)) {
            while (evalExpr(lp->cond, env).isTruthy()) {
                execBlock(lp->body, env);
            }
            return;
        }

        if (auto fn = std::dynamic_pointer_cast<FuncDeclStmt>(stmt)) {
            functions_[fn->name] = fn;
            return;
        }

        if (auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt)) {
            Value v = ret->expr ? evalExpr(ret->expr, env) : Value::makeVoid();
            throw ReturnSignal{v};
        }

        if (auto es = std::dynamic_pointer_cast<ExprStmt>(stmt)) {
            evalExpr(es->expr, env);
            return;
        }
    }

    Value evalExpr(const ExprPtr& expr, const std::shared_ptr<Environment>& env) {
        if (auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr)) {
            return lit->val;
        }

        if (auto id = std::dynamic_pointer_cast<IdentExpr>(expr)) {
            if (env->has(id->name)) {
                return env->get(id->name);
            }
            // Allow unquoted words (e.g. black, red, w, space, player.png) as string arguments
            return Value::makeStr(id->name);
        }

        if (auto un = std::dynamic_pointer_cast<UnaryExpr>(expr)) {
            Value r = evalExpr(un->right, env);
            if (un->op == "-") return Value::makeNum(-r.asNumber());
            if (un->op == "NOT") return Value::makeTof(!r.isTruthy());
            return r;
        }

        if (auto bin = std::dynamic_pointer_cast<BinaryExpr>(expr)) {
            if (bin->op == "AND") {
                return Value::makeTof(evalExpr(bin->left, env).isTruthy() &&
                                      evalExpr(bin->right, env).isTruthy());
            }
            if (bin->op == "OR") {
                return Value::makeTof(evalExpr(bin->left, env).isTruthy() ||
                                      evalExpr(bin->right, env).isTruthy());
            }

            Value l = evalExpr(bin->left, env);
            Value r = evalExpr(bin->right, env);

            if (bin->op == "+") {
                if (l.type == ValType::STR || r.type == ValType::STR) {
                    return Value::makeStr(l.asString() + r.asString());
                }
                return Value::makeNum(l.asNumber() + r.asNumber());
            }
            if (bin->op == "-") return Value::makeNum(l.asNumber() - r.asNumber());
            if (bin->op == "*") return Value::makeNum(l.asNumber() * r.asNumber());
            if (bin->op == "/") {
                double denom = r.asNumber();
                if (denom == 0.0) throw std::runtime_error("Division by zero");
                return Value::makeNum(l.asNumber() / denom);
            }
            if (bin->op == "%") {
                long long a = static_cast<long long>(l.asNumber());
                long long b = static_cast<long long>(r.asNumber());
                if (b == 0) throw std::runtime_error("Modulo by zero");
                return Value::makeNum(static_cast<double>(a % b));
            }
            if (bin->op == "<") return Value::makeTof(l.asNumber() < r.asNumber());
            if (bin->op == ">") return Value::makeTof(l.asNumber() > r.asNumber());
            if (bin->op == "<=") return Value::makeTof(l.asNumber() <= r.asNumber());
            if (bin->op == ">=") return Value::makeTof(l.asNumber() >= r.asNumber());
            if (bin->op == "==") {
                if (l.type == ValType::STR || r.type == ValType::STR) {
                    return Value::makeTof(l.asString() == r.asString());
                }
                if (l.type == ValType::TOF && r.type == ValType::TOF) {
                    return Value::makeTof(l.tof == r.tof);
                }
                return Value::makeTof(l.asNumber() == r.asNumber());
            }
            if (bin->op == "!=") {
                if (l.type == ValType::STR || r.type == ValType::STR) {
                    return Value::makeTof(l.asString() != r.asString());
                }
                if (l.type == ValType::TOF && r.type == ValType::TOF) {
                    return Value::makeTof(l.tof != r.tof);
                }
                return Value::makeTof(l.asNumber() != r.asNumber());
            }
        }

        if (auto call = std::dynamic_pointer_cast<CallExpr>(expr)) {
            return evalCall(call, env);
        }

        return Value::makeVoid();
    }

    void requireArgs(const std::string& fn, const std::vector<Value>& args, size_t expected) {
        if (args.size() < expected) {
            throw std::runtime_error(fn + " expects at least " + std::to_string(expected) +
                                     " arguments, got " + std::to_string(args.size()));
        }
    }

    Value evalCall(const std::shared_ptr<CallExpr>& call, const std::shared_ptr<Environment>& env) {
        const std::string& name = call->callee;

        // Route POOGRAPHIC.*
        if (name.rfind("POOGRAPHIC.", 0) == 0) {
            if (!hasPoographic_) {
                throw std::runtime_error("Library 'poographic' must be imported first (IMPORT poographic;)");
            }
            return evalGraphicsCommand(false, name.substr(11), call->args, env);
        }

        // Route POOGRAPHICL.*
        if (name.rfind("POOGRAPHICL.", 0) == 0) {
            if (!hasPoographicl_) {
                throw std::runtime_error("Library 'poographicl' must be imported first (IMPORT poographicl;)");
            }
            return evalGraphicsCommand(true, name.substr(12), call->args, env);
        }

        // User-defined FUNC
        auto fnIt = functions_.find(name);
        if (fnIt == functions_.end()) {
            throw std::runtime_error("Undefined function: " + name);
        }

        const auto& fn = fnIt->second;
        auto localEnv = std::make_shared<Environment>(globalEnv_);
        for (size_t i = 0; i < fn->params.size(); ++i) {
            Value argVal = (i < call->args.size()) ? evalExpr(call->args[i], env) : Value::makeVoid();
            localEnv->define(fn->params[i].name, argVal);
        }

        try {
            execBlock(fn->body, localEnv);
        } catch (const ReturnSignal& sig) {
            return sig.val;
        }
        return Value::makeVoid();
    }

    Value evalGraphicsCommand(bool isCli,
                              const std::string& cmd,
                              const std::vector<ExprPtr>& rawArgs,
                              const std::shared_ptr<Environment>& env) {
        // ALLTOUCH registers persistent AST expressions for runtime tracking
        if (cmd == "ALLTOUCH") {
            if (rawArgs.size() < 8) {
                throw std::runtime_error("ALLTOUCH expects 8 arguments: x1, y1, w1, h1, x2, y2, w2, h2");
            }
            allTouchWatchers_.push_back(AllTouchWatcher{rawArgs, "", env});
            double x1 = evalExpr(rawArgs[0], env).asNumber();
            double y1 = evalExpr(rawArgs[1], env).asNumber();
            double w1 = evalExpr(rawArgs[2], env).asNumber();
            double h1 = evalExpr(rawArgs[3], env).asNumber();
            double x2 = evalExpr(rawArgs[4], env).asNumber();
            double y2 = evalExpr(rawArgs[5], env).asNumber();
            double w2 = evalExpr(rawArgs[6], env).asNumber();
            double h2 = evalExpr(rawArgs[7], env).asNumber();
            bool hit = poographic::touch(x1, y1, w1, h1, x2, y2, w2, h2);
            lastAllTouchHit_ = hit;
            return Value::makeTof(hit);
        }

        if (cmd == "ALLTOUCHED") {
            return Value::makeTof(lastAllTouchHit_);
        }

        std::vector<Value> args;
        args.reserve(rawArgs.size());
        for (const auto& a : rawArgs) {
            args.push_back(evalExpr(a, env));
        }

        if (cmd == "MAKEWINDOW") {
            requireArgs(cmd, args, 2);
            int w = static_cast<int>(args[0].asNumber());
            int h = static_cast<int>(args[1].asNumber());
            if (isCli) poographicl::makeWindow(w, h);
            else poographic::makeWindow(w, h);
            return Value::makeVoid();
        }

        if (cmd == "ALIVE") {
            return Value::makeTof(isCli ? poographicl::isAlive() : poographic::isAlive());
        }

        if (cmd == "BUCKET") {
            requireArgs(cmd, args, 1);
            if (isCli) poographicl::bucket(args[0].asString());
            else poographic::bucket(args[0].asString());
            return Value::makeVoid();
        }

        if (cmd == "BRUSH") {
            requireArgs(cmd, args, 1);
            if (isCli) poographicl::brush(args[0].asString());
            else poographic::brush(args[0].asString());
            return Value::makeVoid();
        }

        if (cmd == "DRAW") {
            updateAllTouchWatchers();
            if (isCli) poographicl::draw();
            else poographic::draw();
            return Value::makeVoid();
        }

        if (cmd == "BOX") {
            requireArgs(cmd, args, 4);
            int x = static_cast<int>(args[0].asNumber());
            int y = static_cast<int>(args[1].asNumber());
            int w = static_cast<int>(args[2].asNumber());
            int h = static_cast<int>(args[3].asNumber());
            if (isCli) poographicl::box(x, y, w, h);
            else poographic::box(x, y, w, h);
            return Value::makeVoid();
        }

        if (cmd == "CIRCLE") {
            requireArgs(cmd, args, 3);
            int x = static_cast<int>(args[0].asNumber());
            int y = static_cast<int>(args[1].asNumber());
            int r = static_cast<int>(args[2].asNumber());
            if (isCli) poographicl::circle(x, y, r);
            else poographic::circle(x, y, r);
            return Value::makeVoid();
        }

        if (cmd == "LINE") {
            requireArgs(cmd, args, 4);
            int x1 = static_cast<int>(args[0].asNumber());
            int y1 = static_cast<int>(args[1].asNumber());
            int x2 = static_cast<int>(args[2].asNumber());
            int y2 = static_cast<int>(args[3].asNumber());
            if (isCli) poographicl::line(x1, y1, x2, y2);
            else poographic::line(x1, y1, x2, y2);
            return Value::makeVoid();
        }

        if (cmd == "WRITE") {
            requireArgs(cmd, args, 3);
            int x = static_cast<int>(args[0].asNumber());
            int y = static_cast<int>(args[1].asNumber());
            if (isCli) poographicl::writeText(x, y, args[2].asString());
            else poographic::writeText(x, y, args[2].asString());
            return Value::makeVoid();
        }

        if (cmd == "PIC") {
            requireArgs(cmd, args, 3);
            int x = static_cast<int>(args[0].asNumber());
            int y = static_cast<int>(args[1].asNumber());
            if (isCli) poographicl::pic(x, y, args[2].asString());
            else poographic::pic(x, y, args[2].asString());
            return Value::makeVoid();
        }

        if (cmd == "KEY.HOLD") {
            requireArgs(cmd, args, 1);
            std::string k = args[0].asString();
            return Value::makeTof(isCli ? poographicl::keyHold(k) : poographic::keyHold(k));
        }

        if (cmd == "KEY.PRESS") {
            requireArgs(cmd, args, 1);
            std::string k = args[0].asString();
            return Value::makeTof(isCli ? poographicl::keyPress(k) : poographic::keyPress(k));
        }

        if (cmd == "MOUSEX") {
            return Value::makeNum(isCli ? poographicl::mouseX() : poographic::mouseX());
        }

        if (cmd == "MOUSEY") {
            return Value::makeNum(isCli ? poographicl::mouseY() : poographic::mouseY());
        }

        if (cmd == "CLICK") {
            return Value::makeTof(isCli ? poographicl::click() : poographic::click());
        }

        if (cmd == "TOUCH") {
            requireArgs(cmd, args, 8);
            bool hit = poographic::touch(
                args[0].asNumber(), args[1].asNumber(), args[2].asNumber(), args[3].asNumber(),
                args[4].asNumber(), args[5].asNumber(), args[6].asNumber(), args[7].asNumber()
            );
            return Value::makeTof(hit);
        }

        if (cmd == "RANDOM") {
            requireArgs(cmd, args, 2);
            int minV = static_cast<int>(args[0].asNumber());
            int maxV = static_cast<int>(args[1].asNumber());
            return Value::makeNum(isCli ? poographicl::randomInt(minV, maxV)
                                        : poographic::randomInt(minV, maxV));
        }

        throw std::runtime_error("Unknown graphics command: " + cmd);
    }
};

// ============================================================================
// 6. ENTRY POINT
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: poolang <script.poo>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: Could not open file '" << argv[1] << "'\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    try {
        Lexer lexer(buffer.str());
        std::vector<Token> tokens = lexer.tokenize();

        Parser parser(tokens);
        std::vector<StmtPtr> program = parser.parseProgram();

        Interpreter interpreter;
        interpreter.run(program);
    } catch (const std::exception& ex) {
        poographic::shutdown();
        poographicl::shutdown();
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    poographic::shutdown();
    poographicl::shutdown();
    return 0;
}
