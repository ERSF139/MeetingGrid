#pragma once

#include "IconFactory.h"

#include <QPushButton>

// 底部控制栏/侧边栏通用按钮：图标 + 下方文字。
// 可选中（checkable）：选中态显示高亮；麦克风/摄像头类按钮通过 checked 表示"开启"，
// 未选中时自动绘制红色斜线表示"关闭"。
class ControlButton : public QPushButton
{
    Q_OBJECT
public:
    explicit ControlButton(IconFactory::IconKind icon, const QString &text,
                           QWidget *parent = nullptr);

    // 图标与文字颜色（默认深灰/白，由 setDarkTheme 切换）
    void setDarkTheme(bool dark);
    void setIconSizeHint(int size);

    // 运行期更换图标与文字（如"宫格视图/演讲者视图"切换）
    void setIconKind(IconFactory::IconKind kind);
    void setLabelText(const QString &text);

    // 结束会议等警示按钮
    void setDanger(bool danger);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    IconFactory::IconKind m_icon;
    QString m_text;
    int     m_iconSize = 24;
    bool    m_darkTheme = false;
    bool    m_danger = false;
};
