#include "platform/platform_services.h"

#include "util/common.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

namespace jarvis {

namespace {

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') {
            out += "\\\"";
        } else {
            out.push_back(ch);
        }
    }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'') {
            out += "'\"'\"'";
        } else {
            out.push_back(ch);
        }
    }
    out += "'";
    return out;
#endif
}

std::string runCapture(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return {};
    }

    std::array<char, 256> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return util::trim(output);
}

bool isBlockedCommand(std::string_view command) {
    static const std::array<std::string_view, 13> blocked = {
        "rm -rf /",
        "sudo rm -rf",
        "mkfs",
        "dd if=",
        ":(){:|:&};:",
        "shutdown",
        "reboot",
        "halt",
        "diskutil erase",
        "format ",
        " del /f",
        " remove-item ",
        "chmod ",
    };

    const std::string lower = util::toLower(command);
    for (const auto& item : blocked) {
        if (lower.find(item) != std::string::npos) {
            return true;
        }
    }

    return false;
}

std::string formatNow(const char* pattern) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    std::ostringstream out;
    out << std::put_time(&localTime, pattern);
    return out.str();
}

std::string diskInfo() {
    std::error_code error;
    const auto root = std::filesystem::current_path().root_path();
    const auto space = std::filesystem::space(root.empty() ? std::filesystem::current_path() : root, error);
    if (error) {
        return "Disk bilgisi alinamadi.";
    }

    const auto toGb = [](std::uintmax_t bytes) {
        return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    };

    std::ostringstream out;
    out << "Disk: " << std::fixed << std::setprecision(1)
        << toGb(space.capacity - space.available) << "GB kullanildi, "
        << toGb(space.available) << "GB bos";
    return out.str();
}

std::string cpuInfo() {
    const unsigned int count = std::thread::hardware_concurrency();
    if (count == 0U) {
        return "CPU: cekirdek bilgisi alinamadi.";
    }
    return "CPU: " + std::to_string(count) + " mantiksal cekirdek";
}

std::string ramInfo() {
#ifdef _WIN32
    const std::string json = runCapture(
        "powershell -NoProfile -Command "
        "\"$os = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue; "
        "if ($os) { "
        "[math]::Round(($os.TotalVisibleMemorySize - $os.FreePhysicalMemory) / 1MB,1).ToString() + '|' + "
        "[math]::Round($os.TotalVisibleMemorySize / 1MB,1).ToString() "
        "}\"");
    const auto parts = util::split(json, '|');
    if (parts.size() == 2) {
        return "RAM: " + util::trim(parts[0]) + "GB / " + util::trim(parts[1]) + "GB";
    }
#elif defined(__linux__)
    const std::string meminfo = runCapture("cat /proc/meminfo");
    long long totalKb = 0;
    long long availableKb = 0;
    std::istringstream input(meminfo);
    std::string label;
    long long value = 0;
    std::string unit;
    while (input >> label >> value >> unit) {
        if (label == "MemTotal:") {
            totalKb = value;
        } else if (label == "MemAvailable:") {
            availableKb = value;
        }
    }
    if (totalKb > 0) {
        const double totalGb = static_cast<double>(totalKb) / (1024.0 * 1024.0);
        const double usedGb = static_cast<double>(totalKb - availableKb) / (1024.0 * 1024.0);
        std::ostringstream out;
        out << "RAM: " << std::fixed << std::setprecision(1) << usedGb << "GB / " << totalGb << "GB";
        return out.str();
    }
#elif defined(__APPLE__)
    const std::string bytes = runCapture("sysctl -n hw.memsize");
    try {
        const double total = static_cast<double>(std::stoll(bytes)) / (1024.0 * 1024.0 * 1024.0);
        std::ostringstream out;
        out << "RAM: toplam " << std::fixed << std::setprecision(1) << total << "GB";
        return out.str();
    } catch (...) {
    }
#endif
    return "RAM bilgisi alinamadi.";
}

std::string batteryInfo() {
#ifdef _WIN32
    const std::string out = runCapture(
        "powershell -NoProfile -Command "
        "\"$b = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue | "
        "Select-Object -First 1 -ExpandProperty EstimatedChargeRemaining; "
        "if ($null -ne $b) { $b }\"");
    if (!out.empty()) {
        return "Pil: %" + util::trim(out);
    }
#elif defined(__APPLE__)
    const std::string out = runCapture("pmset -g batt");
    if (!out.empty()) {
        return "Pil: " + util::trim(out);
    }
#elif defined(__linux__)
    const std::string capacity = runCapture("cat /sys/class/power_supply/BAT0/capacity 2>/dev/null");
    if (!capacity.empty()) {
        return "Pil: %" + util::trim(capacity);
    }
#endif
    return "Pil bilgisi alinamadi.";
}

std::string networkInfo() {
#ifdef _WIN32
    const std::string hostname = runCapture("hostname");
    if (!hostname.empty()) {
        return "Ag: host " + hostname;
    }
#else
    const std::string hostname = runCapture("hostname");
    if (!hostname.empty()) {
        return "Ag: host " + hostname;
    }
#endif
    return "Ag bilgisi alinamadi.";
}

std::string psQuote(std::string_view text) {
    return util::replaceAll(std::string(text), "'", "''");
}

std::filesystem::path tempRoot() {
    return std::filesystem::path(".jarvis-tmp");
}

std::string uniqueSuffix() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

}  // namespace

std::string PlatformServices::platformName() const {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

bool PlatformServices::supportsSpeechRecognition() const {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

std::string PlatformServices::speakText(std::string_view text, std::string_view voice_name) const {
    std::string safe = util::trim(text);
    if (safe.empty()) {
        return "Metin bos.";
    }
    if (safe.size() > 500) {
        safe.resize(500);
        safe += "...";
    }

#ifdef _WIN32
    const std::string command =
        "powershell -NoProfile -Command "
        "\"Add-Type -AssemblyName System.Speech; "
        "$speaker = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
        "if ('" + psQuote(util::trim(voice_name)) + "' -ne '') { "
        "  try { $speaker.SelectVoice('" + psQuote(util::trim(voice_name)) + "') } catch {} "
        "}; "
        "$speaker.Speak('" + psQuote(safe) + "')\"";
    const int exitCode = std::system(command.c_str());
    return exitCode == 0 ? "TTS tamamlandi." : "Windows TTS calistirilamadi.";
#elif defined(__APPLE__)
    const std::string voiceArg = util::trim(voice_name).empty()
        ? std::string()
        : ("-v " + shellQuote(util::trim(voice_name)) + " ");
    const std::string command = "say " + voiceArg + shellQuote(safe);
    const int exitCode = std::system(command.c_str());
    return exitCode == 0 ? "TTS tamamlandi." : "macOS TTS calistirilamadi.";
#else
    const std::string command = "espeak " + shellQuote(safe);
    const int exitCode = std::system(command.c_str());
    return exitCode == 0 ? "TTS tamamlandi." : "Linux TTS icin espeak gerekli.";
#endif
}

std::string PlatformServices::recognizeSpeech(std::string_view locale, int timeout_seconds) const {
#ifdef _WIN32
    std::filesystem::create_directories(tempRoot());
    const std::string suffix = uniqueSuffix();
    const std::filesystem::path scriptPath = tempRoot() / ("speech-" + suffix + ".ps1");
    const std::filesystem::path outputPath = tempRoot() / ("speech-" + suffix + ".txt");
    const std::filesystem::path errorPath = tempRoot() / ("speech-" + suffix + ".err.txt");

    const std::string script =
        "param(\n"
        "  [string]$Locale,\n"
        "  [int]$TimeoutSeconds,\n"
        "  [string]$OutFile,\n"
        "  [string]$ErrFile\n"
        ")\n"
        "$ErrorActionPreference = 'Stop'\n"
        "[Console]::OutputEncoding = [System.Text.UTF8Encoding]::UTF8\n"
        "Add-Type -AssemblyName System.Speech\n"
        "function New-Recognizer([string]$CultureName) {\n"
        "  try {\n"
        "    if ([string]::IsNullOrWhiteSpace($CultureName)) {\n"
        "      return New-Object System.Speech.Recognition.SpeechRecognitionEngine\n"
        "    }\n"
        "    $culture = New-Object System.Globalization.CultureInfo($CultureName)\n"
        "    return New-Object System.Speech.Recognition.SpeechRecognitionEngine($culture)\n"
        "  } catch {\n"
        "    return $null\n"
        "  }\n"
        "}\n"
        "$engine = New-Recognizer $Locale\n"
        "if (-not $engine) { $engine = New-Recognizer '' }\n"
        "if (-not $engine) {\n"
        "  Set-Content -Path $ErrFile -Value 'Speech recognizer olusturulamadi.' -Encoding UTF8\n"
        "  exit 1\n"
        "}\n"
        "try {\n"
        "  $engine.LoadGrammar((New-Object System.Speech.Recognition.DictationGrammar))\n"
        "  $engine.SetInputToDefaultAudioDevice()\n"
        "  $result = $engine.Recognize([TimeSpan]::FromSeconds($TimeoutSeconds))\n"
        "  if ($result -and $result.Text) {\n"
        "    Set-Content -Path $OutFile -Value $result.Text -Encoding UTF8\n"
        "    exit 0\n"
        "  }\n"
        "  Set-Content -Path $ErrFile -Value 'Ses algilanamadi veya zaman asimi oldu.' -Encoding UTF8\n"
        "  exit 2\n"
        "} catch {\n"
        "  Set-Content -Path $ErrFile -Value $_.Exception.Message -Encoding UTF8\n"
        "  exit 1\n"
        "}\n";

    util::writeTextFile(scriptPath, script);

    const std::string command =
        "powershell -NoProfile -ExecutionPolicy Bypass -File "
        + shellQuote(scriptPath.string())
        + " -Locale " + shellQuote(util::trim(locale))
        + " -TimeoutSeconds " + std::to_string(std::max(1, timeout_seconds))
        + " -OutFile " + shellQuote(outputPath.string())
        + " -ErrFile " + shellQuote(errorPath.string());

    const int exitCode = std::system(command.c_str());

    std::string transcript;
    std::string error;
    if (std::filesystem::exists(outputPath)) {
        transcript = util::trim(util::readTextFile(outputPath));
    }
    if (std::filesystem::exists(errorPath)) {
        error = util::trim(util::readTextFile(errorPath));
    }

    std::error_code ignored;
    std::filesystem::remove(scriptPath, ignored);
    std::filesystem::remove(outputPath, ignored);
    std::filesystem::remove(errorPath, ignored);

    if (exitCode == 0 && !transcript.empty()) {
        return transcript;
    }
    if (!error.empty()) {
        return "Hata: " + error;
    }
    return "Hata: Ses tanima basarisiz oldu.";
#else
    (void)locale;
    (void)timeout_seconds;
    return "Ses tanima bu platformda desteklenmiyor.";
#endif
}

std::string PlatformServices::playAudioFile(const std::filesystem::path& path) const {
    if (path.empty() || !std::filesystem::exists(path)) {
        return "Ses dosyasi bulunamadi.";
    }

#ifdef _WIN32
    const std::string command =
        "powershell -NoProfile -WindowStyle Hidden -Command "
        "\"Add-Type -AssemblyName presentationCore; "
        "$player = New-Object System.Windows.Media.MediaPlayer; "
        "$player.Open([Uri]'" + psQuote(path.lexically_normal().string()) + "'); "
        "while (-not $player.NaturalDuration.HasTimeSpan) { Start-Sleep -Milliseconds 50 }; "
        "$player.Play(); "
        "Start-Sleep -Milliseconds ([Math]::Max(350, [int]$player.NaturalDuration.TimeSpan.TotalMilliseconds)); "
        "$player.Stop(); $player.Close();\"";
    const int exitCode = std::system(command.c_str());
    return exitCode == 0 ? "Ses oynatildi." : "Windows ses oynatma basarisiz.";
#elif defined(__APPLE__)
    const std::string command = "afplay " + shellQuote(path.string()) + " >/dev/null 2>&1";
    const int exitCode = std::system(command.c_str());
    return exitCode == 0 ? "Ses oynatildi." : "macOS ses oynatma basarisiz.";
#else
    const std::string primary = "ffplay -nodisp -autoexit -loglevel quiet " + shellQuote(path.string()) + " >/dev/null 2>&1";
    const int primaryExitCode = std::system(primary.c_str());
    if (primaryExitCode == 0) {
        return "Ses oynatildi.";
    }
    const std::string fallback = "mpg123 -q " + shellQuote(path.string()) + " >/dev/null 2>&1";
    const int fallbackExitCode = std::system(fallback.c_str());
    return fallbackExitCode == 0 ? "Ses oynatildi." : "Linux ses oynatma icin ffplay veya mpg123 gerekli.";
#endif
}

std::string PlatformServices::runShellCommand(std::string_view command) const {
    const std::string safe = util::trim(command);
    if (safe.empty()) {
        return "Komut belirtilmedi.";
    }
    if (isBlockedCommand(safe)) {
        return "Guvenlik: bu komut engellendi.";
    }

    const std::string output = runCapture(safe + " 2>&1");
    if (output.empty()) {
        return "Komut basariyla calisti (cikti yok).";
    }

    if (output.size() > 1200) {
        return output.substr(0, 1200) + "\n... (cikti kisaltildi)";
    }
    return output;
}

std::string PlatformServices::systemInfo(std::string_view query) const {
    const std::string mode = util::toLower(util::trim(query));
    std::vector<std::string> lines;

    if (mode == "battery" || mode == "pil" || mode == "all") {
        lines.push_back(batteryInfo());
    }
    if (mode == "cpu" || mode == "islemci" || mode == "all") {
        lines.push_back(cpuInfo());
    }
    if (mode == "ram" || mode == "memory" || mode == "bellek" || mode == "all") {
        lines.push_back(ramInfo());
    }
    if (mode == "disk" || mode == "depolama" || mode == "all") {
        lines.push_back(diskInfo());
    }
    if (mode == "time" || mode == "saat" || mode == "zaman" || mode == "all") {
        lines.push_back("Saat: " + formatNow("%H:%M:%S"));
    }
    if (mode == "date" || mode == "tarih" || mode == "all") {
        lines.push_back("Tarih: " + formatNow("%d %B %Y"));
    }
    if (mode == "network" || mode == "ag" || mode == "wifi" || mode == "all") {
        lines.push_back(networkInfo());
    }
    if (mode == "all") {
        lines.insert(lines.begin(), "Platform: " + platformName());
    }

    if (lines.empty()) {
        return "Bilinmeyen sorgu. battery/cpu/ram/disk/time/date/network/all kullan.";
    }
    return util::join(lines, "\n");
}

std::string PlatformServices::browserControl(std::string_view action, std::string_view url, std::string_view query) const {
    const std::string mode = util::toLower(util::trim(action));
    if (mode == "open_url") {
        return openTarget(url);
    }
    if (mode == "search") {
        const std::string encoded = util::replaceAll(util::trim(query), " ", "+");
        return openTarget("https://www.google.com/search?q=" + encoded);
    }
    if (mode == "play_youtube") {
        const std::string encoded = util::replaceAll(util::trim(query), " ", "+");
        return openTarget("https://www.youtube.com/results?search_query=" + encoded);
    }
    return "Bilinmeyen browser action.";
}

std::string PlatformServices::openTarget(std::string_view target) const {
    const std::string value = util::trim(target);
    if (value.empty()) {
        return "Hedef bos.";
    }

#ifdef _WIN32
    const std::string command = "cmd /C start \"\" " + shellQuote(value);
#elif defined(__APPLE__)
    const std::string command = "open " + shellQuote(value);
#else
    const std::string command = "xdg-open " + shellQuote(value) + " >/dev/null 2>&1";
#endif

    const int exitCode = std::system(command.c_str());
    return exitCode == 0 ? "Hedef acildi." : "Hedef acilamadi.";
}

}  // namespace jarvis
