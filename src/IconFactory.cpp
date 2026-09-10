#include "IconFactory.h"

#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QtMath>

namespace IconFactory {

// 在图标矩形内绘制一条红色斜线，表示"关闭/禁用"
static void drawSlash(QPainter &p, const QRectF &r, const QColor &slashColor)
{
    QPen pen(slashColor, qMax(1.6, r.width() * 0.12), Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(r.left() + r.width() * 0.18, r.top() + r.height() * 0.18),
               QPointF(r.right() - r.width() * 0.18, r.bottom() - r.height() * 0.18));
}

// 麦克风：圆角机身 + 底座 + 支架 + 声波点
static void drawMic(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    const qreal w  = r.width() * 0.34;
    const qreal h  = r.height() * 0.52;
    const qreal cx = r.center().x();
    const qreal top = r.top() + r.height() * 0.06;

    // 机身
    p.drawRoundedRect(QRectF(cx - w / 2, top, w, h), 2, 2);
    // 底座
    p.drawRoundedRect(QRectF(cx - w / 2, top + h + 1, w, r.height() * 0.08), 2, 2);

    // 支架
    QPen pen(color, 1.5);
    p.setPen(pen);
    p.drawLine(QPointF(cx, top + h + 1 + r.height() * 0.08),
               QPointF(cx, r.bottom() - r.height() * 0.06));
    // 支架底横线
    p.drawLine(QPointF(cx - w * 0.55, r.bottom() - r.height() * 0.06),
               QPointF(cx + w * 0.55, r.bottom() - r.height() * 0.06));

    // 声波点
    p.setPen(color);
    p.setBrush(color);
    p.drawEllipse(QPointF(cx - w * 0.72, top + h * 0.4), 1.2, 1.2);
    p.drawEllipse(QPointF(cx + w * 0.72, top + h * 0.4), 1.2, 1.2);
}

// 摄像头：机身 + 镜头
static void drawCamera(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    const qreal w = r.width() * 0.62;
    const qreal h = r.height() * 0.44;
    const qreal cx = r.center().x() - r.width() * 0.08;
    const qreal cy = r.center().y();

    // 机身
    p.drawRoundedRect(QRectF(cx - w / 2, cy - h / 2, w, h), 3, 3);
    // 镜头（右下）
    p.setBrush(QColor(0, 0, 0, 60));
    p.drawEllipse(QPointF(cx + w * 0.16, cy + h * 0.14), r.width() * 0.09, r.height() * 0.13);
    // 机身左上指示灯
    p.setBrush(color);
    p.drawEllipse(QPointF(cx - w * 0.34, cy - h * 0.22), 1.4, 1.4);
}

// 共享屏幕：屏幕 + 箭头
static void drawShare(QPainter &p, const QRectF &r, const QColor &color)
{
    const qreal w = r.width() * 0.7;
    const qreal h = r.height() * 0.46;
    const qreal cx = r.center().x();
    const qreal cy = r.center().y() - r.height() * 0.04;

    p.setPen(QPen(color, qMax(1.4, r.width() * 0.08)));
    p.setBrush(Qt::NoBrush);
    // 屏幕
    p.drawRoundedRect(QRectF(cx - w / 2, cy - h / 2, w, h), 2, 2);
    // 屏幕底座
    p.drawLine(QPointF(cx - w * 0.16, cy + h / 2 + r.height() * 0.04),
               QPointF(cx + w * 0.16, cy + h / 2 + r.height() * 0.04));
    p.drawLine(QPointF(cx - w * 0.10, cy + h / 2 + r.height() * 0.14),
               QPointF(cx + w * 0.10, cy + h / 2 + r.height() * 0.14));
    // 屏幕内箭头
    QPen arrow(color, qMax(1.3, r.width() * 0.07));
    p.setPen(arrow);
    p.drawLine(QPointF(cx - w * 0.2, cy + h * 0.02), QPointF(cx + w * 0.2, cy + h * 0.02));
    p.drawLine(QPointF(cx + w * 0.2, cy + h * 0.02), QPointF(cx + w * 0.08, cy - h * 0.16));
    p.drawLine(QPointF(cx + w * 0.2, cy + h * 0.02), QPointF(cx + w * 0.14, cy + h * 0.18));
}

// 聊天：气泡 + 三点
static void drawChat(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.4, r.width() * 0.09)));
    p.setBrush(Qt::NoBrush);
    const qreal w = r.width() * 0.7;
    const qreal h = r.height() * 0.44;
    const QRectF bubble(r.center().x() - w / 2, r.center().y() - h / 2, w, h);
    p.drawRoundedRect(bubble, 3, 3);
    // 小尾巴
    p.drawLine(QPointF(bubble.left() + w * 0.18, bubble.bottom() - 1),
               QPointF(bubble.left() + w * 0.08, bubble.bottom() + r.height() * 0.1));
    // 三点
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QPointF(bubble.center().x() - w * 0.16, bubble.center().y()), 1.4, 1.4);
    p.drawEllipse(QPointF(bubble.center().x(), bubble.center().y()), 1.4, 1.4);
    p.drawEllipse(QPointF(bubble.center().x() + w * 0.16, bubble.center().y()), 1.4, 1.4);
}

// 成员：两个人形
static void drawMembers(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const qreal cx = r.center().x();
    // 前一个人
    p.drawEllipse(QPointF(cx + r.width() * 0.16, r.top() + r.height() * 0.32), r.width() * 0.16, r.height() * 0.16);
    QPainterPath body;
    const qreal bw = r.width() * 0.26;
    body.moveTo(cx + r.width() * 0.16 - bw, r.bottom() - r.height() * 0.10);
    body.quadTo(cx + r.width() * 0.16 - bw, r.top() + r.height() * 0.5, cx + r.width() * 0.16, r.top() + r.height() * 0.5);
    body.quadTo(cx + r.width() * 0.16 + bw, r.top() + r.height() * 0.5, cx + r.width() * 0.16 + bw, r.bottom() - r.height() * 0.10);
    body.closeSubpath();
    p.drawPath(body);
    // 后一个人（略小、略暗）
    QColor back = color.darker(150);
    p.setBrush(back);
    p.drawEllipse(QPointF(cx - r.width() * 0.18, r.top() + r.height() * 0.38), r.width() * 0.12, r.height() * 0.12);
    QPainterPath body2;
    const qreal bw2 = r.width() * 0.18;
    body2.moveTo(cx - r.width() * 0.18 - bw2, r.bottom() - r.height() * 0.14);
    body2.quadTo(cx - r.width() * 0.18 - bw2, r.top() + r.height() * 0.52, cx - r.width() * 0.18, r.top() + r.height() * 0.52);
    body2.quadTo(cx - r.width() * 0.18 + bw2, r.top() + r.height() * 0.52, cx - r.width() * 0.18 + bw2, r.bottom() - r.height() * 0.14);
    body2.closeSubpath();
    p.drawPath(body2);
}

// 宫格视图：2x2 小方格
static void drawGrid(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.4, r.width() * 0.08)));
    p.setBrush(Qt::NoBrush);
    const qreal m = r.width() * 0.12;
    const qreal cw = (r.width() - m * 3) / 2;
    const qreal ch = (r.height() - m * 3) / 2;
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            p.drawRoundedRect(QRectF(r.left() + m + i * (cw + m),
                                     r.top() + m + j * (ch + m), cw, ch), 1.5, 1.5);
}

// 演讲者视图：一个大矩形 + 两个小矩形
static void drawSpeaker(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.4, r.width() * 0.08)));
    p.setBrush(Qt::NoBrush);
    const qreal m = r.width() * 0.10;
    const qreal bigW = r.width() * 0.66;
    const qreal bigH = r.height() * 0.72;
    const qreal smallW = r.width() * 0.22;
    const qreal smallH = r.height() * 0.34;
    p.drawRoundedRect(QRectF(r.left() + m, r.top() + m, bigW, bigH), 2, 2);
    p.drawRoundedRect(QRectF(r.right() - smallW - m, r.top() + m, smallW, smallH), 2, 2);
    p.drawRoundedRect(QRectF(r.right() - smallW - m, r.top() + m + smallH + m, smallW, smallH), 2, 2);
}

// 结束会议：电话听筒（红色）
static void drawEndCall(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(2.2, r.width() * 0.14), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    const QPointF tl(r.center().x() - r.width() * 0.30, r.center().y() - r.height() * 0.18);
    const QPointF tr(r.center().x() + r.width() * 0.30, r.center().y() - r.height() * 0.18);
    // 电话听筒底部两脚 + 弧线
    QPainterPath path;
    path.moveTo(tl + QPointF(r.width() * 0.06, r.height() * 0.26));
    path.lineTo(tl + QPointF(r.width() * 0.18, r.height() * 0.26));
    path.lineTo(tl + QPointF(r.width() * 0.22, r.height() * 0.12));
    path.quadTo(QPointF(r.center().x(), r.center().y() + r.height() * 0.02),
                tr + QPointF(-r.width() * 0.22, r.height() * 0.12));
    path.lineTo(tr + QPointF(-r.width() * 0.18, r.height() * 0.26));
    path.lineTo(tr + QPointF(-r.width() * 0.06, r.height() * 0.26));
    p.drawPath(path);
}

// 加号
static void drawPlus(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(2.0, r.width() * 0.14), Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(r.center().x(), r.top() + r.height() * 0.2),
               QPointF(r.center().x(), r.bottom() - r.height() * 0.2));
    p.drawLine(QPointF(r.left() + r.width() * 0.2, r.center().y()),
               QPointF(r.right() - r.width() * 0.2, r.center().y()));
}

// 减号
static void drawMinus(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(2.0, r.width() * 0.14), Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(r.left() + r.width() * 0.2, r.center().y()),
               QPointF(r.right() - r.width() * 0.2, r.center().y()));
}

// 设置：齿轮
static void drawGear(QPainter &p, const QRectF &r, const QColor &color)
{
    const QPointF c = r.center();
    const qreal R = r.width() * 0.40;
    const qreal r2 = r.width() * 0.26;
    const int teeth = 8;
    const qreal kPi = 3.14159265358979323846;

    // 外圈齿
    QPainterPath path;
    for (int i = 0; i < teeth; ++i) {
        const qreal a0 = i * 2 * kPi / teeth;
        const qreal a1 = a0 + kPi / teeth;
        const qreal a2 = a1 + kPi / teeth * 0.7;
        const QPointF p0 = c + QPointF(qCos(a0), qSin(a0)) * R;
        const QPointF p1 = c + QPointF(qCos(a1), qSin(a1)) * R;
        const QPointF p2 = c + QPointF(qCos(a2), qSin(a2)) * r2;
        if (i == 0) path.moveTo(p0);
        else path.lineTo(p0);
        path.lineTo(p1);
        path.lineTo(p2);
    }
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(path);

    // 中心圆孔
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawEllipse(c, r2 * 0.62, r2 * 0.62);
}

// 发送：纸飞机
static void drawSend(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.6, r.width() * 0.09), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    QPainterPath plane;
    plane.moveTo(r.left() + r.width() * 0.08, r.center().y() - r.height() * 0.02);
    plane.lineTo(r.right() - r.width() * 0.08, r.top() + r.height() * 0.08);
    plane.lineTo(r.right() - r.width() * 0.22, r.center().y());
    plane.lineTo(r.right() - r.width() * 0.08, r.bottom() - r.height() * 0.08);
    plane.closeSubpath();
    p.drawPath(plane);
    p.drawLine(QPointF(r.right() - r.width() * 0.22, r.center().y()),
               QPointF(r.left() + r.width() * 0.08, r.center().y() + r.height() * 0.02));
}

// 窗口最小化：居中短横线
static void drawWindowMinimize(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.6, r.width() * 0.12), Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(r.left() + r.width() * 0.22, r.center().y()),
               QPointF(r.right() - r.width() * 0.22, r.center().y()));
}

// 窗口最大化：空心矩形
static void drawWindowMaximize(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.6, r.width() * 0.12)));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(r.left() + r.width() * 0.2, r.top() + r.height() * 0.2,
                      r.width() * 0.6, r.height() * 0.6));
}

// 窗口还原：两个交叠矩形
static void drawWindowRestore(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.5, r.width() * 0.11)));
    p.setBrush(Qt::NoBrush);
    const qreal w = r.width() * 0.5;
    const qreal h = r.height() * 0.5;
    // 后面一个（偏右上）
    p.drawRect(QRectF(r.center().x() - w * 0.16, r.top() + r.height() * 0.14, w, h));
    // 前面一个（偏左下）
    p.drawRect(QRectF(r.left() + r.width() * 0.16, r.center().y() - h * 0.16, w, h));
}

// 窗口关闭：X
static void drawWindowClose(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.6, r.width() * 0.12), Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    const qreal m = r.width() * 0.26;
    p.drawLine(QPointF(r.left() + m, r.top() + m), QPointF(r.right() - m, r.bottom() - m));
    p.drawLine(QPointF(r.right() - m, r.top() + m), QPointF(r.left() + m, r.bottom() - m));
}

// 联网会议：地球（圆 + 经线 + 纬线）
static void drawNetwork(QPainter &p, const QRectF &r, const QColor &color)
{
    p.setPen(QPen(color, qMax(1.3, r.width() * 0.09)));
    p.setBrush(Qt::NoBrush);
    const QPointF c = r.center();
    const qreal R = r.width() * 0.42;
    p.drawEllipse(c, R, R);                        // 外圆
    p.drawEllipse(c, R, R * 0.42);                 // 横向纬线（扁椭圆）
    p.drawLine(QPointF(c.x(), c.y() - R),          // 竖向经线
               QPointF(c.x(), c.y() + R));
    p.drawLine(QPointF(c.x() - R, c.y()),          // 横向经线
               QPointF(c.x() + R, c.y()));
}

void drawIcon(QPainter &p, const QRectF &r, IconKind kind, const QColor &color, bool off)
{
    switch (kind) {
    case IconKind::Mic:      drawMic(p, r, color); break;
    case IconKind::Camera:   drawCamera(p, r, color); break;
    case IconKind::Share:    drawShare(p, r, color); break;
    case IconKind::Chat:     drawChat(p, r, color); break;
    case IconKind::Members:  drawMembers(p, r, color); break;
    case IconKind::Grid:     drawGrid(p, r, color); break;
    case IconKind::Speaker:  drawSpeaker(p, r, color); break;
    case IconKind::EndCall:  drawEndCall(p, r, color); break;
    case IconKind::Plus:     drawPlus(p, r, color); break;
    case IconKind::Minus:    drawMinus(p, r, color); break;
    case IconKind::Send:     drawSend(p, r, color); break;
    case IconKind::Settings: drawGear(p, r, color); break;
    case IconKind::Minimize: drawWindowMinimize(p, r, color); break;
    case IconKind::Maximize: drawWindowMaximize(p, r, color); break;
    case IconKind::Restore:  drawWindowRestore(p, r, color); break;
    case IconKind::Close:    drawWindowClose(p, r, color); break;
    case IconKind::Network:  drawNetwork(p, r, color); break;
    }
    if (off)
        drawSlash(p, r, QColor(0xff, 0x45, 0x45));
}

} // namespace IconFactory
