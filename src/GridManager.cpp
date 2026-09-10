#include "GridManager.h"

#include <cmath>

namespace GridManager {

GridSpec calcGrid(int memberCount)
{
    GridSpec spec;
    if (memberCount <= 0)
        return spec;

    if (memberCount == 1) {
        spec.rows = 1;
        spec.cols = 1;
        return spec;
    }

    // 列数优先取 sqrt 上取整，行数补齐，得到接近正方形的网格，
    // 且列数 >= 行数，保证视频宫格宽大于高（符合常见会议布局）。
    spec.cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(memberCount))));
    spec.rows = static_cast<int>(std::ceil(memberCount / static_cast<double>(spec.cols)));
    return spec;
}

} // namespace GridManager
