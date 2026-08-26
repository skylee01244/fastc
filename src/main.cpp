#include <iostream>
#include <fstream>
#include <sstream>
#include <optional>
#include <vector>
#include <cctype>

#include "./tokenisation.h"
#include "./parser.h"
#include "./generation.h"


int main(int argc, char* argv[]) {
    if(argc != 2) {
        std::cerr << "Error: please input..." << std::endl;
        std::cerr << "fastc <input.fc>" << std::endl;
        return EXIT_FAILURE;
    }

    std::fstream input(argv[1], std::ios::in);
    std::stringstream contents_stream;
    contents_stream << input.rdbuf();
    std::string contents = contents_stream.str();
    input.close();


    Tokeniser tokeniser(std::move(contents));
    std::vector<Token> tokens = tokeniser.tokenise();

    Parser parser(std::move(tokens));
    std::optional<NodeProg> prog = parser.parse_prog();

    if(!prog.has_value()) {
        std::cerr << "Invalid program" << std::endl;
        exit(EXIT_FAILURE);
    }

    Generator generator(prog.value());

    {
        std::fstream file("out.asm",std::ios::out);
        file << generator.gen_prog();
    }

    system("nasm -fmacho64 out.asm");
    system("clang -arch x86_64 out.o -o out");

    return EXIT_SUCCESS;
}