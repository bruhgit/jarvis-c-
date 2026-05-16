#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace jarvis {

class PlatformServices {
public:
    std::string platformName() const;
    bool supportsSpeechRecognition() const;
    std::string speakText(std::string_view text, std::string_view voice_name = {}) const;
    std::string recognizeSpeech(std::string_view locale, int timeout_seconds = 8) const;
    std::string playAudioFile(const std::filesystem::path& path) const;
    std::string runShellCommand(std::string_view command) const;
    std::string systemInfo(std::string_view query) const;
    std::string browserControl(std::string_view action, std::string_view url, std::string_view query) const;
    std::string openTarget(std::string_view target) const;
};

}  // namespace jarvis
