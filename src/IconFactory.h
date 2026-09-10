#pragma once

#include <QColor>

class QPainter;
class QRectF;

// 图标工厂：用 QPainter 手绘所有需要的图标（麦克风、摄像头、共享、聊天、
// 成员、宫格、演讲者、结束、加号、减号等），避免依赖外部图片资源，
// 保证原型在任何环境下都能正常显示。
namespace IconFactory {

enum class IconKind {
    Mic,        // 麦克风
    Camera,     // 摄像头
    Share,      // 共享屏幕
    Chat,       // 聊天
    Members,    // 成员列表
    Grid,       // 宫格视图
    Speaker,    // 演讲者视图
    EndCall,    // 结束会议
    Plus,       // 添加
    Minus,      // 移除
    Send,       // 发送
    Settings,   // 设置（齿轮）
    Minimize,   // 窗口最小化
    Maximize,   // 窗口最大化
    Restore,    // 窗口还原
    Close,      // 窗口关闭
    Network     // 联网会议（地球）
};

// 在矩形 r 内绘制 kind 图标。off 为 true 时绘制红色斜线表示"已关闭/禁用"。
void drawIcon(QPainter &p, const QRectF &r, IconKind kind, const QColor &color, bool off = false);

} // namespace IconFactory
