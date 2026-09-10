#include "VideoTile.h"
#include "IconFactory.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

VideoTile::VideoTile(const QString &name, const QColor &avatarColor, bool isSelf,
                     int index, QWidget *parent)
    : QWidget(parent)
    , m_name(name)
    , m_avatarColor(avatarColor)
    , m_isSelf(isSelf)
    , m_index(index)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(80, 60);
}

void VideoTile::setMicOn(bool on)
{
    if (m_micOn == on)
        return;
    m_micOn = on;
    update();
}

void VideoTile::setCamOn(bool on)
{
    if (m_camOn == on)
        return;
    m_camOn = on;
    update();
}

void VideoTile::setSpeaking(bool speaking)
{
    if (m_speaking == speaking)
        return;
    m_speaking = speaking;
    update();
}

void VideoTile::setHighlighted(bool highlighted)
{
    if (m_highlighted == highlighted)
        return;
    m_highlighted = highlighted;
    update();
}

void VideoTile::setOccupied(bool occupied)
{
    if (m_occupied == occupied)
        return;
    m_occupied = occupied;
    update();
}

void VideoTile::setSharing(bool sharing)
{
    if (m_sharing == sharing)
        return;
    m_sharing = sharing;
    update();
}

void VideoTile::setFillMode(FillMode mode)
{
    if (m_fillMode == mode)
        return;
    m_fillMode = mode;
    update();
}

void VideoTile::setVideoFrame(const QImage &frame)
{
    m_videoImage = frame;
    m_hasVideo = true;
    update();
}

void VideoTile::clearVideoFrame()
{
    m_videoImage = QImage();
    m_hasVideo = false;
    update();
}

void VideoTile::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    m_hover = true;
    update();
}

void VideoTile::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hover = false;
    update();
}

void VideoTile::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    emit clicked(m_index);
}

void VideoTile::drawStatusIcons(QPainter &p, const QRectF &r) const
{
    const qreal icon = 15;
    const qreal gap  = 5;
    const qreal y    = r.bottom() - icon - 8;
    const qreal xCam = r.right() - icon - 8;
    const qreal xMic = xCam - icon - gap;

    const QColor iconColor(255, 255, 255, 200);
    IconFactory::drawIcon(p, QRectF(xCam, y, icon, icon), IconFactory::IconKind::Camera, iconColor, !m_camOn);
    IconFactory::drawIcon(p, QRectF(xMic, y, icon, icon), IconFactory::IconKind::Mic, iconColor, !m_micOn);
}

void VideoTile::drawVideo(QPainter &p, const QRectF &r)
{
    if (m_videoImage.isNull())
        return;

    const qreal scale = (m_fillMode == FillMode::Crop)
                            ? qMax(r.width() / m_videoImage.width(), r.height() / m_videoImage.height())
                            : qMin(r.width() / m_videoImage.width(), r.height() / m_videoImage.height());
    const int w = qRound(m_videoImage.width() * scale);
    const int h = qRound(m_videoImage.height() * scale);
    const QRect target(int(r.center().x()) - w / 2, int(r.center().y()) - h / 2, w, h);

    // 圆角裁剪：先裁剪到圆角路径，再绘制视频
    QPainterPath clip;
    clip.addRoundedRect(r, 12, 12);
    p.save();
    p.setClipPath(clip);
    p.drawImage(target, m_videoImage);
    p.restore();
}

void VideoTile::drawAvatar(QPainter &p, const QRectF &r)
{
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal av = qMin(r.width(), r.height()) * 0.42;
    QRectF avRect(cx - av / 2, cy - av / 2, av, av);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 45));
    p.drawEllipse(avRect.translated(0, 2));
    p.setBrush(QColor(255, 255, 255, 235));
    p.drawEllipse(avRect);

    QFont f = p.font();
    f.setPointSizeF(av * 0.36);
    f.setBold(true);
    p.setFont(f);
    p.setPen(m_avatarColor.darker(120));
    p.drawText(avRect, Qt::AlignCenter, m_name.left(1));
}

void VideoTile::drawSilhouette(QPainter &p, const QRectF &r)
{
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal hs = qMin(r.width(), r.height()) * 0.26;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 45));
    p.drawEllipse(QPointF(cx, cy - hs * 0.35), hs * 0.52, hs * 0.52);

    QPainterPath body;
    body.moveTo(cx - hs * 0.85, cy + hs * 0.95);
    body.quadTo(cx - hs * 0.85, cy + hs * 0.2, cx, cy + hs * 0.2);
    body.quadTo(cx + hs * 0.85, cy + hs * 0.2, cx + hs * 0.85, cy + hs * 0.95);
    body.closeSubpath();
    p.drawPath(body);

    QFont f = p.font();
    f.setPointSizeF(qMax(8.0, r.height() * 0.05));
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 150));
    p.drawText(QRectF(0, cy + hs * 1.05, width(), height() * 0.1), Qt::AlignHCenter,
               QStringLiteral("摄像头已关闭"));
}

void VideoTile::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);

    // 空位占位：虚线框 + "等待加入"
    if (!m_occupied) {
        p.setPen(QPen(QColor(0xff, 0xff, 0xff, 70), 1.5, Qt::DashLine));
        p.setBrush(QColor(0xff, 0xff, 0xff, 12));
        p.drawRoundedRect(r, 12, 12);
        QFont f = p.font();
        f.setPointSizeF(qMax(8.0, r.height() * 0.055));
        p.setFont(f);
        p.setPen(QColor(0xff, 0xff, 0xff, 130));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("等待成员加入"));
        return;
    }

    // 背景渐变
    QLinearGradient grad(r.topLeft(), r.bottomRight());
    if (m_camOn) {
        grad.setColorAt(0.0, m_avatarColor.lighter(140));
        grad.setColorAt(1.0, m_avatarColor.darker(140));
    } else {
        grad.setColorAt(0.0, QColor(0x33, 0x38, 0x40));
        grad.setColorAt(1.0, QColor(0x1f, 0x23, 0x29));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(r, 12, 12);

    // 中央内容：摄像头关闭 -> 剪影；开启且有画面 -> 视频；否则 -> 头像（等待画面）
    if (!m_camOn) {
        drawSilhouette(p, r);
    } else if (m_hasVideo) {
        drawVideo(p, r);
    } else {
        drawAvatar(p, r);
        // 提示"摄像头启动中"
        QFont f = p.font();
        f.setPointSizeF(qMax(8.0, r.height() * 0.045));
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 120));
        p.drawText(QRectF(0, r.center().y() + r.height() * 0.30, width(), height() * 0.1),
                   Qt::AlignHCenter, QStringLiteral("摄像头启动中..."));
    }

    // 名字胶囊（左下）
    QFont nf = p.font();
    nf.setPointSizeF(qMax(8.5, r.height() * 0.055));
    p.setFont(nf);
    const QString shown = m_isSelf ? m_name + QStringLiteral("（我）") : m_name;
    const QFontMetrics fm(nf);
    const int textW = fm.horizontalAdvance(shown);
    const int pillH = qMax(18, int(r.height() * 0.13));
    const int pad   = 9;
    QRectF pill(r.left() + 8, r.bottom() - pillH - 8, textW + pad * 2, pillH);
    p.setPen(Qt::NoPen);
    p.setBrush(m_isSelf ? QColor(0x2e, 0x8b, 0xff, 200) : QColor(0, 0, 0, 110));
    p.drawRoundedRect(pill, pillH / 2, pillH / 2);
    p.setPen(Qt::white);
    p.drawText(pill.adjusted(pad, 0, -pad, 0), Qt::AlignLeft | Qt::AlignVCenter, shown);

    // 状态图标（右下）
    drawStatusIcons(p, r);

    // 共享中角标（左上）
    if (m_sharing) {
        QFont sf = p.font();
        sf.setPointSizeF(qMax(7.5, r.height() * 0.05));
        p.setFont(sf);
        const QString label = QStringLiteral("共享中");
        const QFontMetrics sfm(sf);
        const int sh = qMax(16, int(r.height() * 0.11));
        const int sw = sfm.horizontalAdvance(label) + 9 + 10 + 8; // 图标+文字+内边距
        const QRectF badge(r.left() + 8, r.top() + 8, sw, sh);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x2e, 0x8b, 0xff, 215));
        p.drawRoundedRect(badge, sh / 2, sh / 2);
        const qreal ic = sh * 0.6;
        IconFactory::drawIcon(p, QRectF(badge.left() + 6, badge.top() + (sh - ic) / 2, ic, ic),
                              IconFactory::IconKind::Share, Qt::white);
        p.setPen(Qt::white);
        p.drawText(QRectF(badge.left() + 6 + ic + 3, badge.top(), sw - 6 - ic - 3, sh),
                   Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    // 说话 / 选中边框
    if (m_speaking) {
        p.setPen(QPen(QColor(0x2e, 0xc4, 0x6b), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    } else if (m_highlighted) {
        p.setPen(QPen(QColor(0x4a, 0x9e, 0xff), 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    } else if (m_hover) {
        p.setPen(QPen(QColor(255, 255, 255, 110), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    }
}
