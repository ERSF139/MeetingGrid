#include "GridContainer.h"
#include "VideoTile.h"

#include <QPainter>

GridContainer::GridContainer(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
}

void GridContainer::addTile(VideoTile *tile)
{
    tile->setParent(this);
    // 父容器已显示后动态新建的子控件不会自动可见，必须显式 show()，
    // 否则成员增减重建后所有宫格会隐藏（表现为"内容不见了"）。
    tile->show();
    m_tiles.append(tile);
    relayout();
}

void GridContainer::removeTile(VideoTile *tile)
{
    const int idx = m_tiles.indexOf(tile);
    if (idx < 0)
        return;
    m_tiles.remove(idx);
    tile->deleteLater();
    if (m_speakerIndex >= m_tiles.size())
        m_speakerIndex = m_tiles.isEmpty() ? -1 : 0;
    relayout();
}

void GridContainer::clearTiles()
{
    for (VideoTile *t : m_tiles)
        t->deleteLater();
    m_tiles.clear();
    m_speakerIndex = -1;
    relayout();
}

VideoTile *GridContainer::tileAt(int index) const
{
    if (index < 0 || index >= m_tiles.size())
        return nullptr;
    return m_tiles.at(index);
}

void GridContainer::setViewMode(ViewMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    if (m_mode == ViewMode::Speaker && m_speakerIndex < 0)
        m_speakerIndex = 0;
    relayout();
}

void GridContainer::setSpeakerIndex(int index)
{
    if (m_speakerIndex == index)
        return;
    m_speakerIndex = index;
    relayout();
}

void GridContainer::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    relayout();
}

void GridContainer::relayout()
{
    m_spec = GridManager::calcGrid(m_tiles.size());
    if (m_mode == ViewMode::Speaker)
        layoutSpeaker();
    else
        layoutGallery();
    update();
}

void GridContainer::layoutGallery()
{
    if (m_tiles.isEmpty())
        return;
    if (!m_spec.isValid())
        return;

    const int rows = m_spec.rows;
    const int cols = m_spec.cols;

    // 可用区域：扣除边距
    const qreal availW = width() - kMargin * 2;
    const qreal availH = height() - kMargin * 2;
    if (availW <= 0 || availH <= 0)
        return;

    const qreal cellW = (availW - (cols - 1) * kSpacing) / cols;
    const qreal cellH = (availH - (rows - 1) * kSpacing) / rows;

    for (int i = 0; i < m_tiles.size(); ++i) {
        const int r = i / cols;
        const int c = i % cols;
        const int x = kMargin + qRound(c * (cellW + kSpacing));
        const int y = kMargin + qRound(r * (cellH + kSpacing));
        m_tiles[i]->setGeometry(x, y, qRound(cellW), qRound(cellH));
        m_tiles[i]->setHighlighted(false);
    }

    // 空格子占位（不足一行的位置留给 paintEvent 画虚线框）
    for (int i = m_tiles.size(); i < rows * cols; ++i) {
        const int r = i / cols;
        const int c = i % cols;
        const int x = kMargin + qRound(c * (cellW + kSpacing));
        const int y = kMargin + qRound(r * (cellH + kSpacing));
        // 不创建实际控件，仅记录，由 paintEvent 绘制虚线占位框
        Q_UNUSED(x);
        Q_UNUSED(y);
        Q_UNUSED(r);
        Q_UNUSED(c);
    }
}

void GridContainer::layoutSpeaker()
{
    if (m_tiles.isEmpty())
        return;

    const qreal availW = width() - kMargin * 2;
    const qreal availH = height() - kMargin * 2;
    if (availW <= 0 || availH <= 0)
        return;

    const int n = m_tiles.size();
    const int mainIndex = (m_speakerIndex >= 0 && m_speakerIndex < n) ? m_speakerIndex : 0;

    // 主画面：占上方约 78% 高度
    const qreal thumbH = qMin(110.0, availH * 0.22);
    const qreal mainH = availH - thumbH - kSpacing;
    const qreal mainW = availW;

    m_tiles[mainIndex]->setGeometry(kMargin, kMargin, qRound(mainW), qRound(mainH));
    m_tiles[mainIndex]->setHighlighted(true);

    // 其余成员：底部缩略条
    const int thumbs = n - 1;
    if (thumbs > 0) {
        const qreal tW = (availW - (thumbs - 1) * kSpacing) / thumbs;
        int idx = 0;
        for (int i = 0; i < n; ++i) {
            if (i == mainIndex)
                continue;
            const int x = kMargin + qRound(idx * (tW + kSpacing));
            const int y = kMargin + qRound(mainH + kSpacing);
            m_tiles[i]->setGeometry(x, y, qRound(tW), qRound(thumbH));
            m_tiles[i]->setHighlighted(false);
            ++idx;
        }
    }
}

void GridContainer::drawEmptyCells(QPainter &p, const QRectF &area)
{
    // 宫格视图下，不足一行的空位绘制虚线占位框
    if (m_mode != ViewMode::Gallery || !m_spec.isValid())
        return;

    const int rows = m_spec.rows;
    const int cols = m_spec.cols;
    const int filled = m_tiles.size();
    if (filled >= rows * cols)
        return;

    const qreal cellW = (area.width() - (cols - 1) * kSpacing) / cols;
    const qreal cellH = (area.height() - (rows - 1) * kSpacing) / rows;

    for (int i = filled; i < rows * cols; ++i) {
        const int r = i / cols;
        const int c = i % cols;
        const qreal x = area.left() + c * (cellW + kSpacing);
        const qreal y = area.top() + r * (cellH + kSpacing);
        p.setPen(QPen(QColor(0xff, 0xff, 0xff, 60), 1.5, Qt::DashLine));
        p.setBrush(QColor(0xff, 0xff, 0xff, 10));
        p.drawRoundedRect(QRectF(x, y, cellW, cellH), 12, 12);
        QFont f = p.font();
        f.setPointSizeF(qMax(9.0, cellH * 0.06));
        p.setFont(f);
        p.setPen(QColor(0xff, 0xff, 0xff, 110));
        p.drawText(QRectF(x, y, cellW, cellH), Qt::AlignCenter, QStringLiteral("等待成员加入"));
    }
}

void GridContainer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 容器背景：深色会议画面区
    p.fillRect(rect(), QColor(0x1d, 0x21, 0x27));

    // 顶部细装饰线
    p.fillRect(QRect(0, 0, width(), 1), QColor(0xff, 0xff, 0xff, 14));

    const QRectF area(kMargin, kMargin, width() - kMargin * 2, height() - kMargin * 2);
    drawEmptyCells(p, area);
}
