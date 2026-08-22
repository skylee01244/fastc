#pragma once

#include "./parser.h"

class Generator {
public:
    inline Generator(NodeExit root) :
        m_root(std::move(root)) {}

    [[nodiscard]] std::string generate() const
    {
        std::stringstream output;
        output << "global _main\n";
        output << "section .text\n";
        output << "_main:\n";
        output << "    mov rax, 0x2000001\n";
        output << "    mov rdi, " << m_root.expr.int_lit.value.value() << "\n";
        output << "    syscall";
        return output.str();
    }

private:
    const NodeExit m_root;


};