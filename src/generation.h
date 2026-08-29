#pragma once

#include "./parser.h"
#include <unordered_map>
#include <cassert>

class Generator {
public:
    inline Generator(NodeProg prog) :
        m_prog(std::move(prog))
        {
            m_scopes.push_back({});
        }

    void gen_term(const NodeTerm* term) {
        struct TermVisitor {
            Generator* gen;
            void operator()(const NodeTermIntLit* term_int_lit) const {
                gen->m_output << "    mov rax, " << term_int_lit->int_lit.value.value() << "\n";
                gen->push("rax");
            }
            void operator()(const NodeTermIdent* term_ident) const {
                const auto name = term_ident->ident.value.value();

                bool found = false;
                Var var;
                for (auto it = gen->m_scopes.rbegin(); it != gen->m_scopes.rend(); ++it) {
                    if (it->contains(name)) {
                        var = it->at(name);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    std::cerr << "Undeclared identifier: " << name << std::endl;
                    exit(EXIT_FAILURE);
                }

                const size_t offset = (gen->m_stack_size - var.stack_location - 1) * 8;
                gen->push("[rsp + " + std::to_string(offset) + "]");
            }
            void operator()(const NodeTermParen* term_paren) const {
                gen->gen_expr(term_paren->expr);
            }
        };
        TermVisitor visitor({.gen = this});
        std::visit(visitor, term->var);
    }

    void gen_expr(const NodeExpr* expr) {
        struct ExprVisitor {
            Generator* gen;

            void operator()(const NodeTerm* term) const
            {
                gen->gen_term(term);
            }

            void operator()(const NodeBinExpr* bin_expr) const {
                struct BinExprVisitor {
                    Generator* gen;

                    void operator()(const NodeBinExprAdd* add_expr) const {
                        gen->gen_expr(add_expr->lhs);
                        gen->gen_expr(add_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    add rax, rbx\n";
                        gen->push("rax");
                    }

                    void operator()(const NodeBinExprMulti* multi_expr) const {
                        gen->gen_expr(multi_expr->lhs);
                        gen->gen_expr(multi_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    mul rbx\n";
                        gen->push("rax");
                    }

                    void operator()(const NodeBinExprDiv* div_expr) const {
                        gen->gen_expr(div_expr->lhs);
                        gen->gen_expr(div_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");

                        // zero out rdx before division
                        gen->m_output << "    xor rdx, rdx\n";

                        gen->m_output << "    div rbx\n";
                        gen->push("rax");
                        // rax holds quotient
                        // rdx holds remainder
                    }

                    void operator()(const NodeBinExprSub* sub_expr) const {
                        gen->gen_expr(sub_expr->lhs);
                        gen->gen_expr(sub_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    sub rax, rbx\n";
                        gen->push("rax");
                    }

                    void operator()(const NodeBinExprEq* eq_expr) const {
                        gen->gen_expr(eq_expr->lhs);
                        gen->gen_expr(eq_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    cmp rax, rbx\n";
                        gen->m_output << "    sete al\n";
                        gen->m_output << "    movzx rax, al\n";
                        gen->push("rax");
                    }
                    void operator()(const NodeBinExprNotEq* neq_expr) const {
                        gen->gen_expr(neq_expr->lhs);
                        gen->gen_expr(neq_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    cmp rax, rbx\n";
                        gen->m_output << "    setne al\n";
                        gen->m_output << "    movzx rax, al\n";
                        gen->push("rax");
                    }
                    void operator()(const NodeBinExprLess* less_expr) const {
                        gen->gen_expr(less_expr->lhs);
                        gen->gen_expr(less_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    cmp rax, rbx\n";
                        gen->m_output << "    setl al\n";
                        gen->m_output << "    movzx rax, al\n";
                        gen->push("rax");
                    }
                    void operator()(const NodeBinExprLessEq* less_eq_expr) const {
                        gen->gen_expr(less_eq_expr->lhs);
                        gen->gen_expr(less_eq_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    cmp rax, rbx\n";
                        gen->m_output << "    setle al\n";
                        gen->m_output << "    movzx rax, al\n";
                        gen->push("rax");
                    }
                    void operator()(const NodeBinExprGreater* greater_expr) const {
                        gen->gen_expr(greater_expr->lhs);
                        gen->gen_expr(greater_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    cmp rax, rbx\n";
                        gen->m_output << "    setg al\n";
                        gen->m_output << "    movzx rax, al\n";
                        gen->push("rax");
                    }
                    void operator()(const NodeBinExprGreaterEq* greater_eq_expr) const {
                        gen->gen_expr(greater_eq_expr->lhs);
                        gen->gen_expr(greater_eq_expr->rhs);
                        gen->pop("rbx");
                        gen->pop("rax");
                        gen->m_output << "    cmp rax, rbx\n";
                        gen->m_output << "    setge al\n";
                        gen->m_output << "    movzx rax, al\n";
                        gen->push("rax");
                    }
                };
                BinExprVisitor visitor {.gen = gen};
                std::visit(visitor, bin_expr->var);
            }
        };

        ExprVisitor visitor {.gen = this};
        std::visit(visitor, expr->var);
    }

    void begin_scope() {
        m_scopes.push_back({});
    }

    void end_scope() {
        const size_t pop_count = m_scopes.back().size();
        m_scopes.pop_back();

        if (pop_count > 0) {
            m_output << "    add rsp, " << pop_count * 8 << "\n";
            m_stack_size -= pop_count;
        }
    }

    void gen_scope(const NodeScope* scope) {
        begin_scope();
        for (const NodeStmt& stmt : scope->stmts) {
            gen_stmt(stmt);
        }
        end_scope();
    }

    void gen_stmt(const NodeStmt& stmt) {
        struct StmtVisitor {
            Generator* gen;
            void operator()(const NodeStmtExit& stmt_exit) const
            {
                gen->gen_expr(stmt_exit.expr);
                gen->m_output << "    mov rax, 0x2000001\n";
                gen->pop("rdi");
                gen->m_output << "    syscall\n";
            }
            void operator()(const NodeStmtLet& stmt_let)
            {
                if(gen->m_scopes.back().contains(stmt_let.ident.value.value())) {
                    std::cerr << "Variable already exists: " << stmt_let.ident.value.value() << std::endl;
                    exit(EXIT_FAILURE);
                }
                gen->m_scopes.back().insert({stmt_let.ident.value.value(), Var {.stack_location = gen->m_stack_size}});
                gen->gen_expr(stmt_let.expr);
            }
            void operator()(const NodeScope* scope) {
                gen->gen_scope(scope);
            }
            void operator()(const NodeStmtIf* stmt_if) {    // if/elif/else chain
                std::string label_end = "label_end_" + std::to_string(gen->m_label_count++);

                gen->gen_expr(stmt_if->expr);
                gen->pop("rax");
                gen->m_output << "    test rax, rax\n";

                std::string label_next = "label_next_" + std::to_string(gen->m_label_count++);

                // jump to the next block
                gen->m_output << "    jz " << label_next << "\n";

                // execute scope and jump to end
                gen->gen_scope(stmt_if->scope);
                gen->m_output << "    jmp " << label_end << "\n";
                gen->m_output << label_next << ":\n";

                for (const auto* elif : stmt_if->elifs) {
                    label_next = "label_next_" + std::to_string(gen->m_label_count++);
                    gen->gen_expr(elif->expr);
                    gen->pop("rax");
                    gen->m_output << "    test rax, rax\n";
                    gen->m_output << "    jz " << label_next << "\n";

                    gen->gen_scope(elif->scope);
                    gen->m_output << "    jmp " << label_end << "\n";
                    gen->m_output << label_next << ":\n";
                }

                if (stmt_if->els.has_value()) {
                    gen->gen_scope(stmt_if->els.value());
                }

                gen->m_output << label_end << ":\n";
            }
            void operator()(const NodeStmtAssign& stmt_assign) {
                const auto name = stmt_assign.ident.value.value();

                bool found = false;
                Var var;
                for (auto it = gen->m_scopes.rbegin(); it != gen->m_scopes.rend(); ++it) {
                    if (it->contains(name)) {
                        var = it->at(name);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    std::cerr << "Undeclared identifier in assignment: " << name << std::endl;
                    exit(EXIT_FAILURE);
                }
                gen->gen_expr(stmt_assign.expr);
                gen->pop("rax");
                const size_t offset = (gen->m_stack_size - var.stack_location - 1) * 8;

                gen->m_output << "    mov [rsp + " << offset << "], rax\n";
            }
            void operator()(const NodeStmtWhile* stmt_while) {
                std::string label_start = "label_start_" + std::to_string(gen->m_label_count++);
                std::string label_end = "label_end_" + std::to_string(gen->m_label_count++);

                gen->m_output << label_start << ":\n";

                gen->gen_expr(stmt_while->expr);
                gen->pop("rax");
                gen->m_output << "    test rax, rax\n";
                gen->m_output << "    jz " << label_end << "\n";

                gen->gen_scope(stmt_while->scope);

                // jump back to re-evaluate the condition
                gen->m_output << "    jmp " << label_start << "\n";

                gen->m_output << label_end << ":\n";
            }

            void operator()(const NodeStmtFor* stmt_for) {
                gen->begin_scope();

                gen->gen_stmt(*(stmt_for->init));

                std::string label_start = "label_start_" + std::to_string(gen->m_label_count++);
                std::string label_end = "label_end_" + std::to_string(gen->m_label_count++);

                gen->m_output << label_start << ":\n";

                gen->gen_expr(stmt_for->cond);
                gen->pop("rax");
                gen->m_output << "    test rax, rax\n";
                gen->m_output << "    jz " << label_end << "\n";

                // run body
                gen->gen_scope(stmt_for->scope);

                // run iteration assignment
                gen->gen_stmt(*(stmt_for->iter));

                gen->m_output << "    jmp " << label_start << "\n";
                gen->m_output << label_end << ":\n";

                gen->end_scope();
            }
        };

        StmtVisitor visitor {.gen = this};
        std::visit(visitor, stmt.var);

    }

    [[nodiscard]] std::string gen_prog()
    {
        std::stringstream output;
        m_output << "global _main\n";
        m_output << "section .text\n";
        m_output << "_main:\n";

        for (const NodeStmt& stmt : m_prog.stmts) {
            gen_stmt(stmt);
        }


        m_output << "    mov rax, 0x2000001\n";
        m_output << "    mov rdi, 0\n";
        m_output << "    syscall\n";
        return m_output.str();
    }

private:
    void push(const std::string& reg) {
        m_output << "    push " << reg << "\n";
        m_stack_size++;
    }
    void pop(const std::string& reg) {
        m_output << "    pop " << reg << "\n";
        m_stack_size--;
    }

    struct Var {
        size_t stack_location;
    };

    const NodeProg m_prog;
    std::stringstream m_output;
    size_t m_stack_size = 0;
    size_t m_label_count = 0;
    std::vector<std::unordered_map<std::string, Var>> m_scopes {};
};