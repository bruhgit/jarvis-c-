#include "gui/main_window.h"

#include <QtConcurrent/QtConcurrentRun>

#include <QApplication>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QVBoxLayout>

namespace jarvis {

namespace {

QFrame* makeCard(const QString& title, QWidget* content, QWidget* parent = nullptr) {
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("infoCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto* heading = new QLabel(title, card);
    heading->setObjectName(QStringLiteral("cardHeading"));
    layout->addWidget(heading);
    layout->addWidget(content);
    layout->addStretch(1);
    return card;
}

QString toHtml(const QString& text) {
    QString escaped = text.toHtmlEscaped();
    escaped.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
    return escaped;
}

QString pickFontFamily(const std::vector<int>& font_ids, QStringView preferred_substring) {
    QString fallback;
    for (const int id : font_ids) {
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        for (const QString& family : families) {
            if (fallback.isEmpty()) {
                fallback = family;
            }
            if (family.contains(preferred_substring, Qt::CaseInsensitive)) {
                return family;
            }
        }
    }
    return fallback;
}

}  // namespace

MainWindow::MainWindow(Runtime& runtime, QWidget* parent)
    : QMainWindow(parent), runtime_(runtime) {
    buildUi();
    applyFonts();
    refreshStaticUi();
    refreshProviderUi();
    refreshToolUi();
    {
        const QSignalBlocker blocker1(voice_mode_combo_);
        const QSignalBlocker blocker2(auto_speak_checkbox_);
        const QSignalBlocker blocker3(secret_protection_checkbox_);
        voice_mode_combo_->setCurrentText(QString::fromStdString(runtime_.config().voice_mode).trimmed().toUpper());
        auto_speak_checkbox_->setChecked(runtime_.config().auto_speak_replies);
        secret_protection_checkbox_->setChecked(runtime_.localSecretProtectionEnabled());
    }
    refreshVoiceUi();
    setVisualState(OrbWidget::VisualState::Listening, QStringLiteral("LISTENING"));
    appendEntry(QStringLiteral("SYSTEM"),
                QStringLiteral("Qt arayuzu hazir. Komut veya dogal dil girerek JARVIS ile calisabilirsiniz."),
                QStringLiteral("#ffcc00"));
    playUiCue("Start");

    connect(&watcher_, &QFutureWatcher<RuntimeResult>::finished, this, [this]() {
        const RuntimeResult result = watcher_.result();
        const QString color = result.role == "error" ? "#ff5d6c"
                            : result.role == "assistant" ? "#00d4c0"
                            : "#ffcc00";
        const QString label = result.role == "assistant" ? "JARVIS"
                             : result.role == "error" ? "ERROR"
                             : "SYSTEM";
        appendEntry(label, QString::fromStdString(result.text), color);

        if (!result.provider.empty()) {
            provider_badge_->setText(QString::fromStdString(result.provider));
        }
        if (!result.model.empty()) {
            model_value_->setText(QString::fromStdString(result.model));
        }

        if (result.role == "error" || !result.success) {
            setVisualState(OrbWidget::VisualState::Error, QStringLiteral("ERROR"));
        } else {
            playUiCue("Done");
            startSpeechOutputIfNeeded(result);
            if (!tts_watcher_.isRunning()) {
                setVisualState(OrbWidget::VisualState::Listening, QStringLiteral("LISTENING"));
            }
        }
        setBusy(false);
        refreshVoiceUi();
    });

    connect(&mic_watcher_, &QFutureWatcher<std::string>::finished, this, [this]() {
        const std::string transcript = mic_watcher_.result();
        const QString recognized = QString::fromStdString(transcript).trimmed();
        if (recognized.isEmpty() || recognized.startsWith(QStringLiteral("Hata:"))) {
            appendEntry(QStringLiteral("ERROR"),
                        recognized.isEmpty() ? QStringLiteral("Ses algilanamadi.") : recognized,
                        QStringLiteral("#ff5d6c"));
            setBusy(false);
            setVisualState(OrbWidget::VisualState::Error, QStringLiteral("ERROR"));
            refreshVoiceUi();
            return;
        }

        input_->setText(recognized);
        appendEntry(QStringLiteral("YOU"), recognized, QStringLiteral("#ffffff"));
        input_->clear();
        setVisualState(OrbWidget::VisualState::Thinking, QStringLiteral("THINKING"));
        watcher_.setFuture(QtConcurrent::run([this, value = recognized.toStdString()]() {
            return executeSafely(value);
        }));
    });

    connect(&tts_watcher_, &QFutureWatcher<std::string>::finished, this, [this]() {
        const std::string status = tts_watcher_.result();
        if (!status.empty() && status.rfind("Hata:", 0) == 0) {
            appendEntry(QStringLiteral("ERROR"), QString::fromStdString(status), QStringLiteral("#ff5d6c"));
            setVisualState(OrbWidget::VisualState::Error, QStringLiteral("ERROR"));
        } else if (!watcher_.isRunning() && !mic_watcher_.isRunning()) {
            setVisualState(OrbWidget::VisualState::Listening, QStringLiteral("LISTENING"));
        }
        refreshVoiceUi();
    });
}

MainWindow::~MainWindow() {
    if (watcher_.isRunning()) {
        watcher_.waitForFinished();
    }
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("JARVIS C++"));
    resize(1680, 980);
    setMinimumSize(1280, 760);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    setStyleSheet(
        "QMainWindow, QWidget {"
        "  background: #041011;"
        "  color: #d9fffb;"
        "}"
        "QFrame#infoCard {"
        "  background: rgba(5, 19, 20, 0.92);"
        "  border: 1px solid #113b39;"
        "  border-radius: 16px;"
        "}"
        "QLabel#titleLabel {"
        "  color: #00d4c0;"
        "  font-size: 28px;"
        "  font-weight: 800;"
        "}"
        "QLabel#subTitleLabel {"
        "  color: #4e7b79;"
        "  font-size: 12px;"
        "  letter-spacing: 1px;"
        "}"
        "QLabel#cardHeading {"
        "  color: #7dfff6;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "  text-transform: uppercase;"
        "  letter-spacing: 1px;"
        "}"
        "QLabel#badgeLabel {"
        "  background: #0a1f20;"
        "  border: 1px solid #155250;"
        "  border-radius: 12px;"
        "  padding: 8px 12px;"
        "  color: #9ff6ef;"
        "  font-weight: 700;"
        "}"
        "QTextEdit {"
        "  background: #030b0c;"
        "  border: 1px solid #0d3937;"
        "  border-radius: 16px;"
        "  padding: 10px;"
        "}"
        "QLineEdit {"
        "  background: #061516;"
        "  border: 1px solid #14514f;"
        "  border-radius: 12px;"
        "  padding: 12px 14px;"
        "  color: #e8fffc;"
        "  font-size: 14px;"
        "}"
        "QPushButton {"
        "  background: #0e2728;"
        "  border: 1px solid #1b6662;"
        "  border-radius: 12px;"
        "  padding: 11px 16px;"
        "  color: #d7fffb;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "  background: #133334;"
        "}"
        "QPushButton:disabled, QLineEdit:disabled, QComboBox:disabled {"
        "  color: #6a8d8a;"
        "}"
        "QComboBox {"
        "  background: #061516;"
        "  border: 1px solid #14514f;"
        "  border-radius: 12px;"
        "  padding: 9px 12px;"
        "}"
    );

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(18);

    auto* header = new QHBoxLayout();
    auto* titleColumn = new QVBoxLayout();
    title_label_ = new QLabel(QStringLiteral("J.A.R.V.I.S"), central);
    title_label_->setObjectName(QStringLiteral("titleLabel"));
    subtitle_label_ = new QLabel(QStringLiteral("Qt Widgets Port · Cross Platform Control Surface"), central);
    subtitle_label_->setObjectName(QStringLiteral("subTitleLabel"));
    titleColumn->addWidget(title_label_);
    titleColumn->addWidget(subtitle_label_);
    header->addLayout(titleColumn);
    header->addStretch(1);

    provider_combo_ = new QComboBox(central);
    provider_combo_->setMinimumWidth(190);
    header->addWidget(provider_combo_);

    secret_protection_checkbox_ = new QCheckBox(QStringLiteral("LOCK KEYS"), central);
    header->addWidget(secret_protection_checkbox_);

    reload_button_ = new QPushButton(QStringLiteral("RELOAD CONFIG"), central);
    header->addWidget(reload_button_);

    provider_badge_ = new QLabel(QStringLiteral("provider"), central);
    provider_badge_->setObjectName(QStringLiteral("badgeLabel"));
    header->addWidget(provider_badge_);

    status_badge_ = new QLabel(QStringLiteral("INITIALISING"), central);
    status_badge_->setObjectName(QStringLiteral("badgeLabel"));
    header->addWidget(status_badge_);

    root->addLayout(header);

    auto* content = new QHBoxLayout();
    content->setSpacing(18);

    auto* leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(16);
    leftColumn->setContentsMargins(0, 0, 0, 0);

    platform_value_ = new QLabel();
    platform_value_->setWordWrap(true);
    leftColumn->addWidget(makeCard(QStringLiteral("Platform"), platform_value_, central));

    voice_status_value_ = new QLabel();
    voice_status_value_->setWordWrap(true);
    leftColumn->addWidget(makeCard(QStringLiteral("Voice"), voice_status_value_, central));

    asset_value_ = new QLabel();
    asset_value_->setWordWrap(true);
    leftColumn->addWidget(makeCard(QStringLiteral("Assets"), asset_value_, central));

    model_value_ = new QLabel();
    model_value_->setWordWrap(true);
    leftColumn->addWidget(makeCard(QStringLiteral("Model"), model_value_, central));

    tools_value_ = new QLabel();
    tools_value_->setWordWrap(true);
    leftColumn->addWidget(makeCard(QStringLiteral("Tools"), tools_value_, central));

    icon_strip_ = new QLabel();
    icon_strip_->setMinimumHeight(52);
    leftColumn->addWidget(makeCard(QStringLiteral("Visual Assets"), icon_strip_, central));
    leftColumn->addStretch(1);

    content->addLayout(leftColumn, 1);

    auto* centerColumn = new QVBoxLayout();
    centerColumn->setSpacing(12);
    centerColumn->setContentsMargins(0, 0, 0, 0);

    orb_ = new OrbWidget(central);
    orb_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    centerColumn->addWidget(orb_, 1);

    auto* centerNote = new QLabel(QStringLiteral(
        "Python Tkinter UI'nin ilk Qt taslagi. Orb, provider secimi ve komut akisı bu katmanda toplandi."),
        central);
    centerNote->setWordWrap(true);
    centerNote->setStyleSheet(QStringLiteral("color:#5e8c89; padding:4px 10px;"));
    centerColumn->addWidget(centerNote);

    content->addLayout(centerColumn, 2);

    conversation_ = new QTextEdit(central);
    conversation_->setReadOnly(true);
    conversation_->setAcceptRichText(true);
    conversation_->setMinimumWidth(460);
    content->addWidget(conversation_, 2);

    root->addLayout(content, 1);

    auto* inputRow = new QHBoxLayout();
    inputRow->setSpacing(12);
    voice_mode_combo_ = new QComboBox(central);
    voice_mode_combo_->setMinimumWidth(120);
    voice_mode_combo_->addItem(QStringLiteral("TEXT"));
    voice_mode_combo_->addItem(QStringLiteral("VOICE"));
    voice_mode_combo_->addItem(QStringLiteral("HANDS-FREE"));
    inputRow->addWidget(voice_mode_combo_);

    auto_speak_checkbox_ = new QCheckBox(QStringLiteral("AUTO SPEAK"), central);
    inputRow->addWidget(auto_speak_checkbox_);

    mic_button_ = new QPushButton(QStringLiteral("MIC"), central);
    mic_button_->setMinimumWidth(92);
    inputRow->addWidget(mic_button_);

    input_ = new QLineEdit(central);
    input_->setPlaceholderText(QStringLiteral("Bir komut ya da mesaj yazin. Ornek: /sys all veya 'bugun hava nasil?'"));
    inputRow->addWidget(input_, 1);

    send_button_ = new QPushButton(QStringLiteral("SEND"), central);
    send_button_->setMinimumWidth(120);
    inputRow->addWidget(send_button_);
    root->addLayout(inputRow);

    connect(send_button_, &QPushButton::clicked, this, [this]() { submitInput(); });
    connect(mic_button_, &QPushButton::clicked, this, [this]() { submitVoiceCapture(); });
    connect(input_, &QLineEdit::returnPressed, this, [this]() { submitInput(); });
    connect(reload_button_, &QPushButton::clicked, this, [this]() {
        try {
            runtime_.reloadConfig();
            {
                const QSignalBlocker blocker1(voice_mode_combo_);
                const QSignalBlocker blocker2(auto_speak_checkbox_);
                const QSignalBlocker blocker3(secret_protection_checkbox_);
                voice_mode_combo_->setCurrentText(QString::fromStdString(runtime_.config().voice_mode).trimmed().toUpper());
                auto_speak_checkbox_->setChecked(runtime_.config().auto_speak_replies);
                secret_protection_checkbox_->setChecked(runtime_.localSecretProtectionEnabled());
            }
            refreshStaticUi();
            refreshProviderUi();
            refreshToolUi();
            refreshVoiceUi();
            appendEntry(QStringLiteral("SYSTEM"), QStringLiteral("Config yenilendi."), QStringLiteral("#ffcc00"));
        } catch (const std::exception& error) {
            appendEntry(QStringLiteral("ERROR"), QString::fromUtf8(error.what()), QStringLiteral("#ff5d6c"));
            setVisualState(OrbWidget::VisualState::Error, QStringLiteral("ERROR"));
        }
    });
    connect(provider_combo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (text.isEmpty()) {
            return;
        }
        try {
            runtime_.setActiveProvider(text.toStdString());
            refreshProviderUi();
            appendEntry(QStringLiteral("SYSTEM"),
                        QStringLiteral("Aktif provider degisti: %1").arg(text),
                        QStringLiteral("#ffcc00"));
        } catch (const std::exception& error) {
            appendEntry(QStringLiteral("ERROR"), QString::fromUtf8(error.what()), QStringLiteral("#ff5d6c"));
            setVisualState(OrbWidget::VisualState::Error, QStringLiteral("ERROR"));
        }
    });
    connect(voice_mode_combo_, &QComboBox::currentTextChanged, this, [this](const QString&) {
        refreshVoiceUi();
    });
    connect(auto_speak_checkbox_, &QCheckBox::stateChanged, this, [this](int) {
        refreshVoiceUi();
    });
    connect(secret_protection_checkbox_, &QCheckBox::stateChanged, this, [this](int state) {
        try {
            runtime_.setLocalSecretProtection(state == static_cast<int>(Qt::Checked));
            appendEntry(QStringLiteral("SYSTEM"),
                        state == static_cast<int>(Qt::Checked)
                            ? QStringLiteral("Local secret protection etkinlestirildi.")
                            : QStringLiteral("Local secret protection kapatildi."),
                        QStringLiteral("#ffcc00"));
            refreshStaticUi();
            refreshProviderUi();
            refreshVoiceUi();
        } catch (const std::exception& error) {
            appendEntry(QStringLiteral("ERROR"), QString::fromUtf8(error.what()), QStringLiteral("#ff5d6c"));
            setVisualState(OrbWidget::VisualState::Error, QStringLiteral("ERROR"));
        }
    });
}

void MainWindow::applyFonts() {
    std::vector<int> fontIds;
    for (const auto& fontPath : runtime_.assets().fontFiles()) {
        const int id = QFontDatabase::addApplicationFont(QString::fromStdString(fontPath.string()));
        if (id != -1) {
            fontIds.push_back(id);
        }
    }

    QString family = pickFontFamily(fontIds, QStringLiteral("Grift"));
    if (family.isEmpty()) {
        family = QStringLiteral("Segoe UI");
    }
    QFont body(family, 11);
    QApplication::setFont(body);

    if (title_label_ != nullptr) {
        QFont titleFont(body);
        titleFont.setPointSize(28);
        titleFont.setWeight(QFont::Black);
        title_label_->setFont(titleFont);
    }
    if (subtitle_label_ != nullptr) {
        QFont subtitleFont(body);
        subtitleFont.setPointSize(11);
        subtitleFont.setWeight(QFont::Medium);
        subtitle_label_->setFont(subtitleFont);
    }
}

void MainWindow::refreshProviderUi() {
    const auto providers = runtime_.providerSummaries();

    {
        const QSignalBlocker blocker(provider_combo_);
        provider_combo_->clear();
        for (const auto& provider : providers) {
            provider_combo_->addItem(QString::fromStdString(provider.name));
            if (provider.active) {
                provider_combo_->setCurrentText(QString::fromStdString(provider.name));
            }
        }
    }

    for (const auto& provider : providers) {
        if (!provider.active) {
            continue;
        }
        provider_badge_->setText(QString::fromStdString(provider.name));
        model_value_->setText(QString("Model: %1\nProtocol: %2\nKey: %3")
                                  .arg(QString::fromStdString(provider.model),
                                       QString::fromStdString(provider.protocol),
                                       provider.has_api_key ? QStringLiteral("configured")
                                                            : QStringLiteral("missing")));
        break;
    }
}

void MainWindow::refreshStaticUi() {
    platform_value_->setText(QString("Target platform: %1\nSource root: %2")
                                 .arg(QString::fromStdString(runtime_.platformName()),
                                      QString::fromStdString(runtime_.config().python_source_root)));

    const AssetSummary summary = runtime_.assets().scan();
    asset_value_->setText(QString("Fonts: %1\nIcons: %2\nSounds: %3\nRoot: %4")
                              .arg(summary.fonts)
                              .arg(summary.icons)
                              .arg(summary.sounds)
                              .arg(QString::fromStdString(runtime_.assets().root().string())));

    QPixmap strip(180, 34);
    strip.fill(Qt::transparent);
    QPainter painter(&strip);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    int x = 0;
    for (const auto& iconPath : runtime_.assets().iconFiles()) {
        QPixmap icon(QString::fromStdString(iconPath.string()));
        if (icon.isNull()) {
            continue;
        }
        const QPixmap scaled = icon.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(x, 1, scaled);
        x += 42;
        if (x > strip.width() - 36) {
            break;
        }
    }
    icon_strip_->setPixmap(strip);
}

void MainWindow::refreshVoiceUi() {
    if (!runtime_.voiceInputSupported()) {
        mic_button_->setDisabled(true);
        voice_status_value_->setText(QStringLiteral(
            "Input: unsupported\nOutput: local TTS available\nSecrets: %1\nNote: speech capture su an yalnizca Windows'ta acik.")
            .arg(runtime_.localSecretProtectionEnabled() ? QStringLiteral("locked") : QStringLiteral("plain")));
        return;
    }

    const VoiceMode modeValue = selectedVoiceMode();
    const bool busy = watcher_.isRunning() || mic_watcher_.isRunning();
    const bool wantsMic = modeValue != VoiceMode::Text;
    const bool handsFree = modeValue == VoiceMode::HandsFree;

    if (handsFree) {
        auto_speak_checkbox_->setChecked(true);
        auto_speak_checkbox_->setDisabled(true);
    } else {
        auto_speak_checkbox_->setDisabled(false);
    }

    mic_button_->setDisabled(busy || !wantsMic);
    mic_button_->setText(mic_watcher_.isRunning() ? QStringLiteral("LISTEN...") : QStringLiteral("MIC"));

    QString modeLabel = QStringLiteral("Text only");
    if (modeValue == VoiceMode::Voice) {
        modeLabel = QStringLiteral("Voice input");
    } else if (modeValue == VoiceMode::HandsFree) {
        modeLabel = QStringLiteral("Hands-free speech pipeline");
    }

    voice_status_value_->setText(QString("Mode: %1\nLocale: %2\nAuto speak: %3\nInput: %4\nSecrets: %5")
                                     .arg(modeLabel,
                                          QString::fromStdString(runtime_.config().speech_locale),
                                          auto_speak_checkbox_->isChecked() ? QStringLiteral("on") : QStringLiteral("off"),
                                          runtime_.voiceInputSupported() ? QStringLiteral("ready") : QStringLiteral("unsupported"),
                                          runtime_.localSecretProtectionEnabled() ? QStringLiteral("locked") : QStringLiteral("plain")));
}

void MainWindow::refreshToolUi() {
    QStringList lines;
    const auto tools = runtime_.toolDefinitions();
    for (const auto& tool : tools) {
        lines << QStringLiteral("• %1").arg(QString::fromStdString(tool.name));
    }
    tools_value_->setText(lines.join(QStringLiteral("\n")));
}

void MainWindow::setBusy(bool busy) {
    input_->setDisabled(busy);
    send_button_->setDisabled(busy);
    reload_button_->setDisabled(busy);
    provider_combo_->setDisabled(busy);
    voice_mode_combo_->setDisabled(busy);
    secret_protection_checkbox_->setDisabled(busy);
    if (!busy) {
        refreshVoiceUi();
    } else {
        mic_button_->setDisabled(true);
    }
}

void MainWindow::setVisualState(OrbWidget::VisualState state, const QString& label) {
    if (state != current_visual_state_) {
        current_visual_state_ = state;
        if (state == OrbWidget::VisualState::Thinking) {
            playUiCue("Think");
        } else if (state == OrbWidget::VisualState::Listening) {
            playUiCue("HUD");
        } else if (state == OrbWidget::VisualState::Error) {
            playUiCue("Error");
        }
    }

    orb_->setState(state);
    orb_->setStatusText(label);
    status_badge_->setText(label);

    QString color = QStringLiteral("#00d4c0");
    if (state == OrbWidget::VisualState::Thinking) {
        color = QStringLiteral("#ffcc00");
    } else if (state == OrbWidget::VisualState::Speaking) {
        color = QStringLiteral("#52a8ff");
    } else if (state == OrbWidget::VisualState::Error) {
        color = QStringLiteral("#ff5d6c");
    }
    status_badge_->setStyleSheet(QString(
        "QLabel#badgeLabel { background:#0a1f20; border:1px solid %1; border-radius:12px; padding:8px 12px; color:%1; font-weight:700; }")
        .arg(color));
}

void MainWindow::appendEntry(const QString& label, const QString& text, const QString& color) {
    if (text.trimmed().isEmpty()) {
        return;
    }
    conversation_->moveCursor(QTextCursor::End);
    conversation_->insertHtml(QString(
        "<div style='margin:0 0 12px 0;'>"
        "<div style='color:%1; font-weight:700; letter-spacing:0.5px;'>%2</div>"
        "<div style='margin-top:4px; color:#d6fbf7; line-height:1.5;'>%3</div>"
        "</div>")
        .arg(color, label, toHtml(text)));
    conversation_->moveCursor(QTextCursor::End);
}

void MainWindow::submitInput() {
    const QString text = input_->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    if (watcher_.isRunning()) {
        return;
    }
    if (text == QStringLiteral("/quit") || text == QStringLiteral("/exit")) {
        close();
        return;
    }

    input_->clear();
    appendEntry(QStringLiteral("YOU"), text, QStringLiteral("#ffffff"));
    playUiCue("HUD");
    setBusy(true);
    setVisualState(OrbWidget::VisualState::Thinking, QStringLiteral("THINKING"));

    watcher_.setFuture(QtConcurrent::run([this, value = text.toStdString()]() {
        return executeSafely(value);
    }));
}

void MainWindow::submitVoiceCapture() {
    if (!runtime_.voiceInputSupported() || mic_watcher_.isRunning() || watcher_.isRunning()) {
        return;
    }

    appendEntry(QStringLiteral("SYSTEM"), QStringLiteral("Dinleniyor... Konusmayi bitirince bekleyin."), QStringLiteral("#ffcc00"));
    playUiCue("HUD");
    setBusy(true);
    setVisualState(OrbWidget::VisualState::Listening, QStringLiteral("VOICE INPUT"));
    mic_watcher_.setFuture(QtConcurrent::run([this]() {
        return captureSpeechSafely();
    }));
}

void MainWindow::startSpeechOutputIfNeeded(const RuntimeResult& result) {
    if (result.role != "assistant" || !result.success) {
        return;
    }

    const bool shouldSpeak = auto_speak_checkbox_->isChecked() || selectedVoiceMode() == VoiceMode::HandsFree;
    if (!shouldSpeak || result.text.empty() || tts_watcher_.isRunning()) {
        return;
    }

    setVisualState(OrbWidget::VisualState::Speaking, QStringLiteral("SPEAKING"));
    tts_watcher_.setFuture(QtConcurrent::run([this, text = result.text]() {
        return speakReplySafely(text);
    }));
}

void MainWindow::playUiCue(std::string cue) {
    const std::filesystem::path sound = runtime_.assets().soundFileByStem(cue);
    if (sound.empty()) {
        return;
    }

    [[maybe_unused]] const auto future = QtConcurrent::run([sound]() {
        PlatformServices platform;
        platform.playAudioFile(sound);
    });
}

MainWindow::VoiceMode MainWindow::selectedVoiceMode() const {
    const QString mode = voice_mode_combo_->currentText().trimmed().toUpper();
    if (mode == QStringLiteral("VOICE")) {
        return VoiceMode::Voice;
    }
    if (mode == QStringLiteral("HANDS-FREE")) {
        return VoiceMode::HandsFree;
    }
    return VoiceMode::Text;
}

RuntimeResult MainWindow::executeSafely(const std::string& input) const {
    try {
        return runtime_.processInput(input);
    } catch (const std::exception& error) {
        return RuntimeResult{false, "error", error.what()};
    }
}

std::string MainWindow::captureSpeechSafely() const {
    try {
        return runtime_.recognizeSpeech(10);
    } catch (const std::exception& error) {
        return std::string("Hata: ") + error.what();
    }
}

std::string MainWindow::speakReplySafely(const std::string& input) const {
    try {
        return runtime_.speakText(input);
    } catch (const std::exception& error) {
        return std::string("Hata: ") + error.what();
    }
}

}  // namespace jarvis
