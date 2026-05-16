#include "app/application.h"

#include "util/common.h"

#include <iostream>
#include <stdexcept>

namespace jarvis {

Application::Application(std::filesystem::path config_path)
    : runtime_(std::move(config_path)) {}

Application::~Application() = default;

int Application::run() {
    printBanner();
    std::cout << '\n' << runtime_.helpText() << '\n';

    for (std::string line; std::cout << "\njarvis> " && std::getline(std::cin, line); ) {
        line = util::trim(line);
        if (line == "/quit" || line == "/exit") {
            return 0;
        }
        try {
            const RuntimeResult result = runtime_.processInput(line);
            if (!result.text.empty()) {
                std::cout << result.text << '\n';
            }
        } catch (const std::exception& error) {
            std::cerr << "[ERR] " << error.what() << '\n';
        }
    }

    return 0;
}

void Application::printBanner() const {
    std::cout
        << "JARVIS C++\n"
        << "Provider: " << runtime_.activeProviderName() << '\n'
        << "Platform: " << runtime_.platformName() << '\n'
        << runtime_.assets().summaryText() << '\n';
}

}  // namespace jarvis
