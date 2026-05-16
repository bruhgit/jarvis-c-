#include "gui/orb_widget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QRandomGenerator>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

namespace jarvis {

namespace {

constexpr qreal kTau = 6.28318530717958647692;

qreal randomBetween(qreal minimum, qreal maximum) {
    return minimum + (maximum - minimum) * QRandomGenerator::global()->generateDouble();
}

}  // namespace

OrbWidget::OrbWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(420, 420);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    particles_.reserve(120);
    for (int index = 0; index < 120; ++index) {
        particles_.push_back(Particle{
            randomBetween(0.0, kTau),
            randomBetween(0.18, 1.00),
            randomBetween(-0.03, 0.03),
            randomBetween(1.0, 3.6),
            randomBetween(0.0, kTau),
        });
    }

    connect(&timer_, &QTimer::timeout, this, [this]() {
        phase_ += 0.045;
        ring_rotation_[0] += 0.9;
        ring_rotation_[1] -= 0.5;
        ring_rotation_[2] += 1.2;
        ring_rotation_[3] -= 0.28;
        update();
    });
    timer_.start(16);
}

void OrbWidget::setState(VisualState state) {
    state_ = state;
    update();
}

void OrbWidget::setStatusText(const QString& text) {
    status_text_ = text;
    update();
}

void OrbWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), Qt::transparent);

    const QColor base = primaryColor();
    const QColor accent = accentColor();
    const QRectF area = rect().adjusted(8.0, 8.0, -8.0, -8.0);
    const QPointF center = area.center();
    const qreal radius = std::min(area.width(), area.height()) * 0.24;
    const qreal pulse = 1.0 + 0.08 * std::sin(phase_ * 2.4);
    const qreal outerRadius = radius * 1.85 * pulse;

    for (int ring = 9; ring >= 1; --ring) {
        const qreal scale = 1.0 + ring * 0.085;
        QColor glow = base;
        glow.setAlpha(10 + ring * 7);
        painter.setPen(QPen(glow, 3.0));
        painter.drawEllipse(center, outerRadius * scale / 2.0, outerRadius * scale / 2.0);
    }

    for (const Particle& particle : particles_) {
        const qreal localPhase = phase_ * (state_ == VisualState::Speaking ? 2.6 : 1.4);
        const qreal angle = particle.angle + particle.speed * localPhase * 30.0;
        const qreal wobble = 0.78 + 0.28 * std::sin(localPhase + particle.phase);
        const qreal orbit = radius * 1.5 * particle.orbit * wobble;
        const QPointF point(
            center.x() + std::cos(angle) * orbit,
            center.y() + std::sin(angle) * orbit * (0.80 + particle.orbit * 0.20));

        QColor dot = (static_cast<int>(particle.orbit * 100.0) % 7 == 0) ? accent : base;
        dot.setAlpha(38 + static_cast<int>(particle.orbit * 150.0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(dot);
        painter.drawEllipse(point, particle.size, particle.size);
    }

    painter.setBrush(Qt::NoBrush);
    for (int index = 0; index < 4; ++index) {
        const qreal arcRadius = radius * (1.55 - index * 0.18);
        QColor arcColor = (index % 2 == 0) ? base : accent;
        arcColor.setAlpha(180 - index * 25);
        painter.setPen(QPen(arcColor, index < 2 ? 3.0 : 2.0));
        painter.drawArc(
            QRectF(center.x() - arcRadius, center.y() - arcRadius, arcRadius * 2.0, arcRadius * 2.0),
            static_cast<int>(ring_rotation_[index] * 16.0),
            static_cast<int>((52 + index * 10) * 16.0));
    }

    QRadialGradient core(center, radius * 1.18);
    QColor coreBright = accent;
    coreBright.setAlpha(210);
    QColor coreMid = base;
    coreMid.setAlpha(160);
    QColor coreEdge(2, 12, 12, 220);
    core.setColorAt(0.0, coreBright);
    core.setColorAt(0.48, coreMid);
    core.setColorAt(1.0, coreEdge);
    painter.setPen(QPen(QColor(0, 212, 192, 90), 1.6));
    painter.setBrush(core);
    painter.drawEllipse(center, radius, radius);

    painter.setPen(QColor(220, 255, 248, 210));
    QFont titleFont(QStringLiteral("Grift Extra Bold"), 15);
    if (!titleFont.exactMatch()) {
        titleFont.setFamily(QStringLiteral("Segoe UI"));
        titleFont.setBold(true);
    }
    painter.setFont(titleFont);
    painter.drawText(QRectF(center.x() - radius, center.y() - 24.0, radius * 2.0, 28.0),
                     Qt::AlignCenter,
                     QStringLiteral("J.A.R.V.I.S"));

    QFont statusFont(QStringLiteral("Grift"), 10);
    if (!statusFont.exactMatch()) {
        statusFont.setFamily(QStringLiteral("Segoe UI"));
    }
    statusFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    painter.setFont(statusFont);
    painter.setPen(base.lighter(135));
    painter.drawText(QRectF(center.x() - radius * 1.2, center.y() + 12.0, radius * 2.4, 24.0),
                     Qt::AlignCenter,
                     status_text_);
}

QColor OrbWidget::primaryColor() const {
    switch (state_) {
        case VisualState::Listening:
            return QColor(0, 255, 136);
        case VisualState::Thinking:
            return QColor(255, 204, 0);
        case VisualState::Speaking:
            return QColor(82, 168, 255);
        case VisualState::Error:
            return QColor(255, 78, 90);
        case VisualState::Paused:
            return QColor(100, 130, 128);
        case VisualState::Initialising:
        default:
            return QColor(0, 212, 192);
    }
}

QColor OrbWidget::accentColor() const {
    switch (state_) {
        case VisualState::Thinking:
            return QColor(255, 140, 0);
        case VisualState::Speaking:
            return QColor(214, 240, 255);
        case VisualState::Error:
            return QColor(255, 186, 186);
        default:
            return QColor(0, 212, 192);
    }
}

}  // namespace jarvis
