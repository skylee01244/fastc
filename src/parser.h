#pragma once

#include "./tokenisation.h"
#include "./arena.h"
#include <variant>

struct NodeExpr;
struct NodeBinExpr;
struct NodeScope;
struct NodeStmt;

// Expressions

struct NodeBinExprEq { NodeExpr* lhs; NodeExpr* rhs; };
struct NodeBinExprNotEq { NodeExpr* lhs; NodeExpr* rhs; };
struct NodeBinExprLess { NodeExpr* lhs; NodeExpr* rhs; };
struct NodeBinExprLessEq { NodeExpr* lhs; NodeExpr* rhs; };
struct NodeBinExprGreater { NodeExpr* lhs; NodeExpr* rhs; };
struct NodeBinExprGreaterEq { NodeExpr* lhs; NodeExpr* rhs; };

struct NodeTermParen {
    NodeExpr* expr;
};

struct NodeTermIntLit {
    Token int_lit;
};

struct NodeTermIdent {
    Token ident;
};

struct NodeBinExprAdd {
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExprMulti {
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExprDiv {
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExprSub {
    NodeExpr* lhs;
    NodeExpr* rhs;
};

struct NodeBinExpr {
    std::variant<NodeBinExprAdd*, NodeBinExprMulti*, NodeBinExprDiv*, NodeBinExprSub*,
            NodeBinExprEq*, NodeBinExprNotEq*, NodeBinExprLess*, NodeBinExprLessEq*, NodeBinExprGreater*, NodeBinExprGreaterEq*> var;
};

struct NodeTerm {
    std::variant<NodeTermIntLit*, NodeTermIdent*, NodeTermParen*> var;
};

struct NodeExpr {
    std::variant<NodeTerm*, NodeBinExpr*> var;
};

// Statements

struct NodeStmtElif {
    NodeExpr* expr;
    NodeScope* scope;
};

struct NodeStmtIf {
    NodeExpr* expr;
    NodeScope* scope;
    std::vector<NodeStmtElif*> elifs;
    std::optional<NodeScope*> els;
};

struct NodeStmtExit {
    NodeExpr* expr;
};

struct NodeStmtLet {
    Token ident;
    NodeExpr* expr;
};

struct NodeStmtAssign {
    Token ident;
    NodeExpr* expr;
};

struct NodeStmtWhile {
    NodeExpr* expr;
    NodeScope* scope;
};

struct NodeStmtFor {
    NodeStmt* init;
    NodeExpr* cond;
    NodeStmt* iter;
    NodeScope* scope;
};

struct NodeStmt {
    std::variant<NodeStmtExit, NodeStmtLet, NodeScope*, NodeStmtIf*, NodeStmtAssign, NodeStmtWhile*, NodeStmtFor*> var;
};

struct NodeProg {
    std::vector<NodeStmt> stmts;
};

struct NodeExit {
    NodeExpr expr;
};

struct NodeScope {
    std::vector<NodeStmt> stmts;
};

class Parser {
public:
    inline explicit Parser(std::vector<Token> tokens)
        : m_tokens(std::move(tokens)),
          m_allocator(1024 * 1024 * 4) // 4 mb
    {}

    std::optional<int> bin_precedence(TokenType type) {
        switch (type) {
            case TokenType::eq_eq:
            case TokenType::nott_eq:
            case TokenType::less:
            case TokenType::less_eq:
            case TokenType::greater:
            case TokenType::greater_eq:
                return 0;
            case TokenType::plus:
            case TokenType::minus:
                return 1;
            case TokenType::star:
            case TokenType::fslash:
                return 2;
            default:
                return {};
        }
    }

    std::optional<NodeScope*> parse_scope() {
        try_consume(TokenType::open_curly, "Expected '{'");
        auto scope = m_allocator.alloc<NodeScope>();

        while (peek().has_value() && peek().value().type != TokenType::close_curly) {
            if (auto stmt = parse_stmt()) {
                scope->stmts.push_back(stmt.value());
            } else {
                std::cerr << "Invalid statement in scope" << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        try_consume(TokenType::close_curly, "Expected '}'");
        return scope;
    }

    std::optional<NodeTerm*> parse_term() {
        if(auto int_lit = try_consume(TokenType::int_lit)) {
            auto term_int_lit = m_allocator.alloc<NodeTermIntLit>();
            term_int_lit->int_lit = int_lit.value();
            auto term = m_allocator.alloc<NodeTerm>();
            term->var = term_int_lit;
            return term;
        }
        else if(auto ident = try_consume(TokenType::ident)) {
            auto term_ident = m_allocator.alloc<NodeTermIdent>();
            term_ident->ident = ident.value();
            auto term = m_allocator.alloc<NodeTerm>();
            term->var = term_ident;
            return term;
        }
        else if (try_consume(TokenType::open_paren).has_value()) {
            auto expr = parse_expr();
            if (!expr.has_value()) {
                std::cerr << "Expected expression inside parentheses" << std::endl;
                exit(EXIT_FAILURE);
            }
            try_consume(TokenType::close_paren, "Expected ')'");

            auto term_paren = m_allocator.alloc<NodeTermParen>();
            term_paren->expr = expr.value();
            auto term = m_allocator.alloc<NodeTerm>();
            term->var = term_paren;
            return term;
        }
        else {return {};}
    }

    std::optional<NodeExpr*> parse_expr(int min_prec = 0) {
        std::optional<NodeTerm*> term_lhs = parse_term();
        if (!term_lhs.has_value()) return {};

        auto expr_lhs = m_allocator.alloc<NodeExpr>();
        expr_lhs->var = term_lhs.value();

        while (true) {
            std::optional<Token> current_tok = peek();
            std::optional<int> prec;

            if (current_tok.has_value()) {
                prec = bin_precedence(current_tok->type);
                if (!prec.has_value() || prec.value() < min_prec) {
                    break;
                }
            } else {
                break;
            }

            Token op = consume();

            int next_min_prec = prec.value() + 1;

            auto expr_rhs = parse_expr(next_min_prec);
            if (!expr_rhs.has_value()) {
                std::cerr << "Expected expression" << std::endl;
                exit(EXIT_FAILURE);
            }

            auto bin_expr = m_allocator.alloc<NodeBinExpr>();

            if (op.type == TokenType::plus) {
                auto add = m_allocator.alloc<NodeBinExprAdd>();
                add->lhs = expr_lhs;
                add->rhs = expr_rhs.value();
                bin_expr->var = add;
            } else if (op.type == TokenType::minus) {
                auto sub = m_allocator.alloc<NodeBinExprSub>();
                sub->lhs = expr_lhs;
                sub->rhs = expr_rhs.value();
                bin_expr->var = sub;
            } else if (op.type == TokenType::star) {
                auto multi = m_allocator.alloc<NodeBinExprMulti>();
                multi->lhs = expr_lhs;
                multi->rhs = expr_rhs.value();
                bin_expr->var = multi;
            } else if (op.type == TokenType::fslash) {
                auto div = m_allocator.alloc<NodeBinExprDiv>();
                div->lhs = expr_lhs;
                div->rhs = expr_rhs.value();
                bin_expr->var = div;
            }
            else if (op.type == TokenType::eq_eq) {
                auto eq = m_allocator.alloc<NodeBinExprEq>();
                eq->lhs = expr_lhs;
                eq->rhs = expr_rhs.value();
                bin_expr->var = eq;
            } else if (op.type == TokenType::nott_eq) {
                auto neq = m_allocator.alloc<NodeBinExprNotEq>();
                neq->lhs = expr_lhs;
                neq->rhs = expr_rhs.value();
                bin_expr->var = neq;
            } else if (op.type == TokenType::less) {
                auto less = m_allocator.alloc<NodeBinExprLess>();
                less->lhs = expr_lhs;
                less->rhs = expr_rhs.value();
                bin_expr->var = less;
            } else if (op.type == TokenType::less_eq) {
                auto less_eq = m_allocator.alloc<NodeBinExprLessEq>();
                less_eq->lhs = expr_lhs;
                less_eq->rhs = expr_rhs.value();
                bin_expr->var = less_eq;
            } else if (op.type == TokenType::greater) {
                auto greater = m_allocator.alloc<NodeBinExprGreater>();
                greater->lhs = expr_lhs;
                greater->rhs = expr_rhs.value();
                bin_expr->var = greater;
            } else if (op.type == TokenType::greater_eq) {
                auto greater_eq = m_allocator.alloc<NodeBinExprGreaterEq>();
                greater_eq->lhs = expr_lhs;
                greater_eq->rhs = expr_rhs.value();
                bin_expr->var = greater_eq;
            }

            auto new_expr_lhs = m_allocator.alloc<NodeExpr>();
            new_expr_lhs->var = bin_expr;
            expr_lhs = new_expr_lhs;
        }

        return expr_lhs;
    }

    std::optional<NodeStmt> parse_stmt() {
        if(peek().value().type == TokenType::exit && peek(1).has_value() && peek(1).value().type == TokenType::open_paren) {
            consume();
            consume();
            NodeStmtExit stmt_exit;
            if(auto node_expr = parse_expr()) {
                stmt_exit = {.expr = node_expr.value()};
            }
            else {
                std::cerr << "Invalid expression" << std::endl;
                exit(EXIT_FAILURE);
            }
            try_consume(TokenType::close_paren, "Expected ')'");
            try_consume(TokenType::semi, "Expected ';'");

            return NodeStmt{.var = stmt_exit};
        }
        else if(peek().has_value() && peek().value().type == TokenType::let
                && peek(1).has_value() && peek(1).value().type == TokenType::ident
                && peek(2).has_value() && peek(2).value().type == TokenType::eq) {
            consume();
            auto stmt_let = NodeStmtLet {.ident = consume()};
            consume();
            if(auto expr = parse_expr()) {
                stmt_let.expr = expr.value();
            } else {
                std::cerr << "Invalid expression" << std::endl;
                exit(EXIT_FAILURE);
            }
            try_consume(TokenType::semi, "Expected ';'");
            return NodeStmt {.var = stmt_let};
        }
        else if (peek().has_value() && peek().value().type == TokenType::if_) {
            consume();
            try_consume(TokenType::open_paren, "Expected '('");

            auto expr = parse_expr();
            if (!expr.has_value()) {
                std::cerr << "Invalid expression" << std::endl;
                exit(EXIT_FAILURE);
            }

            try_consume(TokenType::close_paren, "Expected ')'");

            auto scope = parse_scope();
            if (!scope.has_value()) {
                std::cerr << "Invalid scope" << std::endl;
                exit(EXIT_FAILURE);
            }

            auto stmt_if = m_allocator.alloc<NodeStmtIf>();
            stmt_if->expr = expr.value();
            stmt_if->scope = scope.value();

            // look for else if / else
            while (peek().has_value() && peek().value().type == TokenType::else_) {
                if (peek(1).has_value() && peek(1).value().type == TokenType::if_) {
                    consume(); // else
                    consume(); // if
                    try_consume(TokenType::open_paren, "Expected '('");

                    auto elif_expr = parse_expr();
                    if (!elif_expr.has_value()) {
                        std::cerr << "Invalid expression in else if" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    try_consume(TokenType::close_paren, "Expected ')'");

                    auto elif_scope = parse_scope();
                    if (!elif_scope.has_value()) {
                        std::cerr << "Invalid scope in else if" << std::endl;
                        exit(EXIT_FAILURE);
                    }

                    auto stmt_elif = m_allocator.alloc<NodeStmtElif>();
                    stmt_elif->expr = elif_expr.value();
                    stmt_elif->scope = elif_scope.value();
                    stmt_if->elifs.push_back(stmt_elif);
                } else {
                    consume(); // Consume 'else'
                    auto els_scope = parse_scope();
                    if (!els_scope.has_value()) {
                        std::cerr << "Invalid scope in else" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    stmt_if->els = els_scope.value();
                    break; // else terminates the chain
                }
            }

            return NodeStmt{.var = stmt_if};
        }
        // while llop
        else if (peek().has_value() && peek().value().type == TokenType::while_) {
            consume();
            try_consume(TokenType::open_paren, "Expected '('");

            auto expr = parse_expr();
            if (!expr.has_value()) {
                std::cerr << "Invalid expression in while loop" << std::endl;
                exit(EXIT_FAILURE);
            }

            try_consume(TokenType::close_paren, "Expected ')'");

            auto scope = parse_scope();
            if (!scope.has_value()) {
                std::cerr << "Invalid scope in while loop" << std::endl;
                exit(EXIT_FAILURE);
            }

            auto stmt_while = m_allocator.alloc<NodeStmtWhile>();
            stmt_while->expr = expr.value();
            stmt_while->scope = scope.value();
            return NodeStmt{.var = stmt_while};
        }
        // for loop
        else if (peek().has_value() && peek().value().type == TokenType::for_) {
            consume();
            try_consume(TokenType::open_paren, "Expected '('");

            auto stmt_for = m_allocator.alloc<NodeStmtFor>();

            auto init_node = parse_stmt();
            if (!init_node.has_value()) {
                std::cerr << "Invalid initialization in for loop" << std::endl;
                exit(EXIT_FAILURE);
            }
            stmt_for->init = m_allocator.alloc<NodeStmt>();
            *(stmt_for->init) = init_node.value();

            auto cond_node = parse_expr();
            if (!cond_node.has_value()) {
                std::cerr << "Invalid condition in for loop" << std::endl;
                exit(EXIT_FAILURE);
            }
            stmt_for->cond = cond_node.value();
            try_consume(TokenType::semi, "Expected ';'");

            if (peek().has_value() && peek().value().type == TokenType::ident && peek(1).has_value() && peek(1).value().type == TokenType::eq) {
                auto stmt_assign = NodeStmtAssign {.ident = consume()};
                consume(); // Consume '='
                if (auto expr = parse_expr()) {
                    stmt_assign.expr = expr.value();
                } else {
                    std::cerr << "Invalid update expression in for loop" << std::endl;
                    exit(EXIT_FAILURE);
                }
                stmt_for->iter = m_allocator.alloc<NodeStmt>();
                *(stmt_for->iter) = NodeStmt{.var = stmt_assign};
            } else {
                std::cerr << "Expected assignment in for loop update" << std::endl;
                exit(EXIT_FAILURE);
            }

            try_consume(TokenType::close_paren, "Expected ')'");

            if (auto scope = parse_scope()) {
                stmt_for->scope = scope.value();
            } else {
                std::cerr << "Invalid scope in for loop" << std::endl;
                exit(EXIT_FAILURE);
            }

            return NodeStmt{.var = stmt_for};
        }
        else if (peek().has_value() && peek().value().type == TokenType::ident
                 && peek(1).has_value() && peek(1).value().type == TokenType::eq) {
            auto stmt_assign = NodeStmtAssign {.ident = consume()};
            consume();

            if (auto expr = parse_expr()) {
                stmt_assign.expr = expr.value();
            } else {
                std::cerr << "Invalid expression in assignment" << std::endl;
                exit(EXIT_FAILURE);
            }

            try_consume(TokenType::semi, "Expected ';'");
            return NodeStmt {.var = stmt_assign};
        }
        else if (peek().has_value() && peek().value().type == TokenType::open_curly) {
            if (auto scope = parse_scope()) {
                return NodeStmt{.var = scope.value()};
            }
            return {};
        }
        else {return {};}
    }

    std::optional<NodeProg> parse_prog() {
        NodeProg prog;
        while(peek().has_value()) {
            if(auto stmt = parse_stmt()) {
                prog.stmts.push_back(stmt.value());
            } else {
                std::cerr << "Invalid statement" << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        return prog;
    }


private:
    [[nodiscard]] inline std::optional<Token> peek(int ahead = 0) const {
        if(m_index + ahead >= m_tokens.size()) return {};
        else    return m_tokens.at(m_index + ahead);
    }

    inline Token consume() {
        return m_tokens.at(m_index++);
    }

    inline Token try_consume(TokenType type, const std::string& err_msg) {
        if(peek().has_value() && peek().value().type == type) {
            return consume();
        }
        else {
            std::cerr << err_msg << std::endl;
            exit(EXIT_FAILURE);
        }
    }
    inline std::optional<Token> try_consume(TokenType type) {
        if(peek().has_value() && peek().value().type == type) {
            return consume();
        }
        else {
            return {};
        }
    }

    const std::vector<Token> m_tokens;
    size_t m_index = 0;
    ArenaAllocator m_allocator;
};