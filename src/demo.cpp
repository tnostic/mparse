#include "mparse.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // 1. Create parser instance, with a description.
    mparse::parser cli("Demo Instance -- scientists reveal the hi.");

    // 2. Define your configuration variables with sensible defaults. The parser supports floats!
    bool custom_flag = false;
    int intensity = 0;
    double scale = 1.0;
    std::string output = "default.out"; // a starting value
    std::string input_data;

    // 3. Register flags and options
    cli.add_flag("-b", "--bool", custom_flag, "This is a bool flag!");
    cli.add_option("-i", "--intensity", intensity, "This is an int option!");
    cli.add_option("-s", "--scale", scale, "This is a float option");
    cli.add_option("-o", "--output", output, "TODO");

    // 4. Register positional argument (like raw text or strings)
    cli.add_positional("input", input_data, "TODO");

    // 5. Parse command line
    if (!cli.parse(argc, argv)) {
        if (cli.help_requested()) {
            return 0; // user just requested the help menu, exit cleanly
        }
        std::cerr << "Error: " << cli.error() << "\n";
        std::cerr << "Run with -h or --help for usage information.\n";
        return 1;
    }

    // 6. Use variables directly
    std::cout << "--- Configuration ---\n";
    std::cout << "Input Data : " << input_data << "\n";
    std::cout << "Output File: " << output << "\n";
    std::cout << "Intensity  : " << intensity << "\n";
    std::cout << "Scale      : " << scale << "\n";
    std::cout << "Bool Flag  : " << (custom_flag ? "True" : "False") << "\n";

    return 0;
}

