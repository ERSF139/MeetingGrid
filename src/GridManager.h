#pragma once

// 宫格布局算法：根据成员数量动态计算宫格的行列数。
// 核心规则：
//   1 人   -> 1x1
//   2 人   -> 1x2（两人并排）
//   3~4 人 -> 2x2
//   5~6 人 -> 3x2
//   ...    -> 列数 = ceil(sqrt(n))，行数 = ceil(n / 列数)，尽量接近正方形
// 这样在任意成员数量下，宫格都能自动铺满可用区域。

namespace GridManager {

struct GridSpec {
    int rows = 0;
    int cols = 0;

    int cellCount() const { return rows * cols; }
    bool isValid() const { return rows > 0 && cols > 0; }
};

// 根据成员数量计算宫格行列。memberCount <= 0 时返回空规格。
GridSpec calcGrid(int memberCount);

} // namespace GridManager
