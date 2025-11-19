#include <iostream>
#include <lyra/lyra.hpp>
#include <string>

int main(int argc, char** argv) {
  // Command line parameters
  bool show_help = false;
  int width = 80;
  std::string name;

  // Define command line interface
  auto cli =
      lyra::cli() | lyra::help(show_help) |
      lyra::opt(width, "width")["-w"]["--width"]("How wide should it be?") |
      lyra::arg(name, "name")("Name to use").required();

  // Parse command line
  auto result = cli.parse({argc, argv});

  // Check for errors
  if (!result) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    std::cerr << cli << std::endl;
    return 1;
  }

  // Show help if requested
  if (show_help) {
    std::cout << cli << std::endl;
    return 0;
  }

  // Use the parsed parameters
  std::cout << "Name: " << name << std::endl;
  std::cout << "Width: " << width << std::endl;

  return 0;
}
