#include "args.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char *argv[]) {
    ParsedArgs args = parse_arguments(argc, argv);

#ifdef DEBUG
    std::cout << "Režim: " << (args.decompress ? "dekomprese" : "komprese") << "\n";
    std::cout << "Vstupní soubor: " << args.infile << "\n";
    std::cout << "Výstupní soubor: " << args.outfile << "\n";

    if (!args.decompress) {
        std::cout << "Šířka obrazu: " << args.width << "\n";
        std::cout << "Model: " << (args.use_model ? "aktivní" : "neaktivní") << "\n";
        std::cout << "Skenování: " << (args.adaptive_scan ? "adaptivní" : "sekvenční") << "\n";
    }
#endif


	std::ifstream in(args.infile, std::ios::binary);
    if (!in) {
        std::cerr << "Nelze otevřít vstupní soubor: " << args.infile << "\n";
        return 1;
    }

    std::ofstream out(args.outfile, std::ios::binary);
    if (!out) {
        std::cerr << "Nelze otevřít výstupní soubor: " << args.outfile << "\n";
        return 1;
    }

    out << in.rdbuf();

    return 0;
}
