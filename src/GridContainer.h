#pragma once

#include "GridManager.h"

#include <QVector>
#include <QWidget>

class VideoTile;

// 宫格容器：负责把 VideoTile 子控件按 GridManager 计算出的行列，
// 在窗口尺寸变化 / 成员数量变化时自动重新摆放，实现"多宫格动态布局"。
//
//   Gallery 模式：全部成员铺满容器（1x1 / 2x2 / 3x3 / ...）
//   Speaker 模式：选中的宫格放大为主画面，其余成员在底部排成缩略条
class GridContainer : public QWidget
{
    Q_OBJECT
public:
    enum class ViewMode {
        Gallery, // 宫格视图
        Speaker  // 演讲者视图
    };

    explicit GridContainer(QWidget *parent = nullptr);

    void addTile(VideoTile *tile);
    void removeTile(VideoTile *tile);
    void clearTiles();
    int  tileCount() const { return m_tiles.size(); }
    VideoTile *tileAt(int index) const;

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_mode; }
    void setSpeakerIndex(int index);

    void relayout();

    GridManager::GridSpec currentSpec() const { return m_spec; }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void layoutGallery();
    void layoutSpeaker();
    void drawEmptyCells(QPainter &p, const QRectF &area);

    QVector<VideoTile *> m_tiles;
    GridManager::GridSpec m_spec;
    ViewMode m_mode = ViewMode::Gallery;
    int m_speakerIndex = -1;

    static constexpr int kMargin  = 14;
    static constexpr int kSpacing = 8;
};
