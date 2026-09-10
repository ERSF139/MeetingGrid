#include "GridManager.h"

#include <QtTest>
#include <QVector>

// 宫格布局算法单元测试：
// 验证 GridManager::calcGrid 在不同成员数量下能给出正确的行列数。
class TestGridManager : public QObject
{
    Q_OBJECT

private slots:
    // 边界与典型值
    void knownSizes()
    {
        struct Case {
            int members;
            int rows;
            int cols;
        };
        const QVector<Case> cases = {
            {0, 0, 0},   // 无成员
            {1, 1, 1},   // 单人
            {2, 1, 2},   // 两人并排
            {3, 2, 2},
            {4, 2, 2},
            {5, 2, 3},
            {6, 2, 3},
            {7, 3, 3},
            {8, 3, 3},
            {9, 3, 3},
            {10, 3, 4},
            {12, 3, 4},
            {13, 4, 4},
            {16, 4, 4},
            {20, 4, 5},
            {25, 5, 5},
            {36, 6, 6},
        };
        for (const Case &c : cases) {
            const GridManager::GridSpec s = GridManager::calcGrid(c.members);
            QVERIFY2(s.rows == c.rows, qPrintable(
                         QStringLiteral("n=%1 期望 rows=%2 实际 rows=%3")
                             .arg(c.members).arg(c.rows).arg(s.rows)));
            QVERIFY2(s.cols == c.cols, qPrintable(
                         QStringLiteral("n=%1 期望 cols=%2 实际 cols=%3")
                             .arg(c.members).arg(c.cols).arg(s.cols)));
        }
    }

    // 全覆盖：1..100 人的不变量
    void invariants()
    {
        for (int n = 1; n <= 100; ++n) {
            const GridManager::GridSpec s = GridManager::calcGrid(n);
            QVERIFY2(s.rows > 0 && s.cols > 0, qPrintable(QStringLiteral("n=%1 非法规格").arg(n)));
            // 格子数必须不少于成员数（有足够的格子放所有人）
            QVERIFY2(s.rows * s.cols >= n,
                     qPrintable(QStringLiteral("n=%1 格子数 %2x%3=%4 < %1")
                                    .arg(n).arg(s.rows).arg(s.cols).arg(s.rows * s.cols)));
            // 尽量接近正方形且列 >= 行（视频宫格宽>高）
            QVERIFY2(s.cols >= s.rows,
                     qPrintable(QStringLiteral("n=%1 列<%行").arg(n)));
            // 避免过度浪费：格子数 - 成员数 < 列数（空位不超过一行）
            QVERIFY2(s.rows * s.cols - n < s.cols,
                     qPrintable(QStringLiteral("n=%1 空位过多").arg(n)));
        }
    }
};

QTEST_GUILESS_MAIN(TestGridManager)
#include "tst_gridmanager.moc"
