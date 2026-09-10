#include "ControlButton.h"

#include <QPainter>

ControlButton::ControlButton(IconFactory::IconKind icon, const QString &text, QWidget *parent)
    : QPushButton(parent)
    , m_icon(icon)
    , m_text(text)
{
    setCursor(Qt::PointingHandCursor);
    setCheckable(false);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMinimumSize(56, 56);
}

void ControlButton::setDarkTheme(bool dark)
{
    m_darkTheme = dark;
    update();
}

void ControlButton::setIconSizeHint(int size)
{
    m_iconSize = size;
    update();
}

void ControlButton::setIconKind(IconFactory::IconKind kind)
{
    m_icon = kind;
    update();
}

void ControlButton::setLabelText(const QString &text)
{
    m_text = text;
    update();
}

void ControlButton::setDanger(bool danger)
{
    m_danger = danger;
    update();
}

void ControlButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const bool hovered = underMouse() && isEnabled();
    const bool pressed = isDown();
    const bool checked = isCheckable() && isChecked();

    // 背景
    QColor bg = Qt::transparent;
    if (m_danger) {
        bg = pressed ? QColor(0xd1, 0x28, 0x28) : (hovered ? QColor(0xff, 0x3b, 0x30) : QColor(0xe0, 0x35, 0x2c));
    } else if (pressed || checked) {
        bg = m_darkTheme ? QColor(0xff, 0xff, 0xff, 26) : QColor(0x2e, 0x8b, 0xff, 40);
    } else if (hovered) {
        bg = m_darkTheme ? QColor(0xff, 0xff, 0xff, 14) : QColor(0, 0, 0, 8);
    }
    if (bg != Qt::transparent) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(r, 10, 10);
    }

    // 图标颜色
    QColor iconColor;
    if (m_danger)
        iconColor = Qt::white;
    else if (m_darkTheme)
        iconColor = QColor(0xf0, 0xf0, 0xf0);
    else
        iconColor = checked ? QColor(0x2e, 0x8b, 0xff) : QColor(0x44, 0x4a, 0x52);

    // 是否需要"关闭斜线"：麦克风/摄像头未选中时
    bool off = false;
    if ((m_icon == IconFactory::IconKind::Mic || m_icon == IconFactory::IconKind::Camera)
        && isCheckable() && !isChecked())
        off = true;

    // 绘制图标
    const qreal iconR = m_iconSize;
    const qreal labelH = m_text.isEmpty() ? 0 : 15;
    const qreal totalH = iconR + (labelH > 0 ? 6 : 0) + labelH;
    const qreal topY = (height() - totalH) / 2.0;
    IconFactory::drawIcon(p, QRectF((width() - iconR) / 2.0, topY, iconR, iconR), m_icon, iconColor, off);

    // 下方文字
    if (!m_text.isEmpty()) {
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        p.setPen(m_darkTheme ? QColor(0xdd, 0xdd, 0xdd) : QColor(0x5a, 0x5f, 0x66));
        p.drawText(QRectF(0, topY + iconR + 4, width(), labelH), Qt::AlignHCenter, m_text);
    }
}
