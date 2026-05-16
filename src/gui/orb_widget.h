#pragma once

#include <QTimer>
#include <QWidget>

#include <array>
#include <vector>

namespace jarvis {

class OrbWidget : public QWidget {
public:
    enum class VisualState {
        Initialising,
        Listening,
        Thinking,
        Speaking,
        Error,
        Paused,
    };

    explicit OrbWidget(QWidget* parent = nullptr);

    void setState(VisualState state);
    void setStatusText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Particle {
        qreal angle = 0.0;
        qreal orbit = 0.0;
        qreal speed = 0.0;
        qreal size = 0.0;
        qreal phase = 0.0;
    };

    QColor primaryColor() const;
    QColor accentColor() const;

    VisualState state_ = VisualState::Initialising;
    QString status_text_ = QStringLiteral("INITIALISING");
    QTimer timer_;
    qreal phase_ = 0.0;
    std::array<qreal, 4> ring_rotation_{0.0, 40.0, 110.0, 220.0};
    std::vector<Particle> particles_;
};

}  // namespace jarvis
