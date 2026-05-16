#pragma once

#include "gui/orb_widget.h"
#include "runtime/runtime.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>

namespace jarvis {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(Runtime& runtime, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    enum class VoiceMode {
        Text,
        Voice,
        HandsFree,
    };

    void buildUi();
    void applyFonts();
    void refreshProviderUi();
    void refreshStaticUi();
    void refreshToolUi();
    void refreshVoiceUi();
    void setBusy(bool busy);
    void setVisualState(OrbWidget::VisualState state, const QString& label);
    void appendEntry(const QString& label, const QString& text, const QString& color);
    void submitInput();
    void submitVoiceCapture();
    void startSpeechOutputIfNeeded(const RuntimeResult& result);
    void playUiCue(std::string cue);
    VoiceMode selectedVoiceMode() const;
    RuntimeResult executeSafely(const std::string& input) const;
    std::string captureSpeechSafely() const;
    std::string speakReplySafely(const std::string& input) const;

    Runtime& runtime_;
    OrbWidget* orb_ = nullptr;
    OrbWidget::VisualState current_visual_state_ = OrbWidget::VisualState::Initialising;
    QLabel* provider_badge_ = nullptr;
    QLabel* status_badge_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* platform_value_ = nullptr;
    QLabel* asset_value_ = nullptr;
    QLabel* model_value_ = nullptr;
    QLabel* tools_value_ = nullptr;
    QLabel* icon_strip_ = nullptr;
    QTextEdit* conversation_ = nullptr;
    QLineEdit* input_ = nullptr;
    QPushButton* send_button_ = nullptr;
    QPushButton* mic_button_ = nullptr;
    QPushButton* reload_button_ = nullptr;
    QComboBox* provider_combo_ = nullptr;
    QComboBox* voice_mode_combo_ = nullptr;
    QCheckBox* auto_speak_checkbox_ = nullptr;
    QCheckBox* secret_protection_checkbox_ = nullptr;
    QLabel* voice_status_value_ = nullptr;
    QFutureWatcher<RuntimeResult> watcher_;
    QFutureWatcher<std::string> mic_watcher_;
    QFutureWatcher<std::string> tts_watcher_;
};

}  // namespace jarvis
