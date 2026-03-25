#include <iostream>
#include <string>
#include "MapToWad.h"

void printHelp() {
    std::cout << "Doom Map Converter\n";
    std::cout << "===================\n";
    std::cout << "Usage:\n";
    std::cout << "  map_converter --create <textfile> <wadfile> [mapname]\n";
    std::cout << "  map_converter --add <textfile> <wadfile> [mapname]\n";
    std::cout << "  map_converter --new-wad <wadfile>\n";
    std::cout << "  map_converter --help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  map_converter --create mymap.txt mymaps.wad MAP01\n";
    std::cout << "  map_converter --add mymap.txt mymaps.wad MAP02\n";
    std::cout << "  map_converter --new-wad mymaps.wad\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h") {
        printHelp();
        return 0;
    }

    if (command == "--new-wad") {
        if (argc < 3) {
            std::cerr << "Error: Missing WAD filename\n";
            return 1;
        }

        std::string wadFile = argv[2];
        if (MapToWad::createEmptyWad(wadFile)) {
            std::cout << "Empty WAD file created: " << wadFile << std::endl;
            return 0;
        } else {
            std::cerr << "Failed to create WAD file\n";
            return 1;
        }
    }

    if (command == "--create" || command == "--add") {
        if (argc < 4) {
            std::cerr << "Error: Missing parameters\n";
            printHelp();
            return 1;
        }

        std::string textFile = argv[2];
        std::string wadFile = argv[3];
        std::string mapName = (argc > 4) ? argv[4] : "MAP01";

        bool result;
        if (command == "--create") {
            result = MapToWad::convertTextMapToWad(textFile, wadFile, mapName);
        } else {
            result = MapToWad::addMapToWad(wadFile, textFile, mapName);
        }

        if (result) {
            std::cout << "Successfully " << (command == "--create" ? "created" : "added to")
                      << " WAD file\n";
            return 0;
        } else {
            std::cerr << "Failed to process map\n";
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << std::endl;
    printHelp();
    return 1;
}