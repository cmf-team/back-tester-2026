#include <fstream>
#include <iostream>
#include <string>

#include "../common/JsonLineParser.h"
#include "../common/Summary.h"

int main(int argc, char** argv) {
    std::string file_path;

    if (argc > 1) {
        file_path = argv[1];
    } else {
        std::cout << "Using default folder: data/extracted\n";
        file_path = "data/extracted/xeur-eobi-20260309.mbo.json";
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "ERROR: cannot open file\n";
        return 1;
    }

    Summary summary;

    std::string line;

    while (std::getline(file, line)) {
        try {
            auto event = JsonLineParser::parse(line);

            // ❌ НИЧЕГО НЕ ПЕЧАТАЕМ
            // processMarketDataEvent(event); ← УДАЛЕНО

            summary.update(event);
        } catch (...) {
            // игнор плохих строк
        }
    }

    file.close();

    // ✔ ТОЛЬКО SUMMARY
    summary.print();

    return 0;
}