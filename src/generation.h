#pragma once

#include "./parser.h"
#include <unordered_map>

class Generator {
public:
    inline Generator(NodeProg prog) :
        m_prog(std::move(prog)) {}

    void gen_expr(const NodeExpr& expr) {
        struct ExprVisitor {
            Generator* gen;
            void operator()(const NodeExprIntLit& expr_int_lit) const
            {
                gen->m_output << "    mov rax, " << expr_int_lit.int_lit.value.value() << "\n";
                gen->push("rax");
            }
            void operator()(const NodeExprIdent& expr_ident) const
            {
                const auto name = expr_ident.ident.value.value();

                if (!gen->m_vars.contains(name)) {
                    std::cerr << "Undeclared identifier: " << name << std::endl;
                    exit(EXIT_FAILURE);
                }

                const auto& var = gen->m_vars.at(name);
                const size_t offset = (gen->m_stack_size - var.stack_location - 1) * 8;
                gen->push("[rsp + " + std::to_string(offset) + "]");
            }

        };

        ExprVisitor visitor {.gen = this};
        std::visit(visitor, expr.var);
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
                if(gen->m_vars.contains(stmt_let.ident.value.value())) {
                    std::cerr << "Variable already exists: " << stmt_let.ident.value.value() << std::endl;
                    exit(EXIT_FAILURE);
                }
                gen->m_vars.insert({stmt_let.ident.value.value(), Var {.stack_location = gen->m_stack_size}});
                gen->gen_expr(stmt_let.expr);
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
    std::unordered_map<std::string, Var> m_vars {};
};