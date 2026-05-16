#include "app/application.h"
#include "config/app_config.h"

#ifdef JARVIS_HAS_QT
#include "gui/main_window.h"

#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>
#endif

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#if defined(_WIN32) && defined(JARVIS_HAS_QT)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#endif

namespace {

bool hasCliFlag(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--cli") {
            return true;
        }
    }
    return false;
}

#if defined(_WIN32) && defined(JARVIS_HAS_QT)
void attachConsoleForCli() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }

    FILE* stream = nullptr;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

void showFatalErrorDialog(const std::string& message) {
    std::wstring wide(message.begin(), message.end());
    MessageBoxW(nullptr,
                wide.c_str(),
                L"JARVIS Fatal Error",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}
#endif

}  // namespace

int main(int argc, char** argv) {
    const bool force_cli = hasCliFlag(argc, argv);

#if defined(_WIN32) && defined(JARVIS_HAS_QT)
    if (force_cli) {
        attachConsoleForCli();
    }
#endif

    try {
        const std::filesystem::path config_path =
            jarvis::defaultConfigPath(argc > 0 ? std::filesystem::path(argv[0]) : std::filesystem::path{});
#ifdef JARVIS_HAS_QT
        if (!force_cli) {
            QApplication app(argc, argv);
            app.setStyle(QStyleFactory::create("Fusion"));
            jarvis::Runtime runtime(config_path);
            jarvis::MainWindow window(runtime);
            window.show();
            return app.exec();
        }
#endif
        jarvis::Application app(config_path);
        return app.run();
    } catch (const std::exception& error) {
#if defined(_WIN32) && defined(JARVIS_HAS_QT)
        if (!force_cli) {
            showFatalErrorDialog(error.what());
            return 1;
        }
#endif
        std::cerr << "[FATAL] " << error.what() << '\n';
        return 1;
    }
}
