#pragma once

#include "runtime/runtime.h"

#include <filesystem>

namespace jarvis {

class Application {
public:
    explicit Application(std::filesystem::path config_path);
    ~Application();
    int run();

private:
    void printBanner() const;
    Runtime runtime_;
};

}  // namespace jarvis
