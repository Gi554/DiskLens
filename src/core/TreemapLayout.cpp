#include "core/TreemapLayout.h"

#include <algorithm>
#include <functional>

namespace disklens {
std::vector<MapRect> layoutTreemap(std::vector<MapItem> items, double width, double height) {
    std::vector<MapRect> rectangles;
    if (width <= 0 || height <= 0) return rectangles;
    items.erase(std::remove_if(items.begin(), items.end(), [](auto item) { return item.bytes == 0; }),
                items.end());
    if (items.empty()) return rectangles;
    std::stable_sort(items.begin(), items.end(), [](auto a, auto b) { return a.bytes > b.bytes; });
    std::vector<long double> sums(items.size() + 1, 0);
    for (std::size_t i = 0; i < items.size(); ++i) sums[i + 1] = sums[i] + items[i].bytes;
    std::function<void(std::size_t, std::size_t, double, double, double, double)> split;
    split = [&](std::size_t first, std::size_t end, double x, double y, double w, double h) {
        if (end - first == 1) {
            rectangles.push_back({items[first].id, x, y, w, h});
            return;
        }
        const long double total = sums[end] - sums[first];
        const long double midpoint = sums[first] + total / 2;
        auto found = std::lower_bound(sums.begin() + static_cast<std::ptrdiff_t>(first + 1),
                                     sums.begin() + static_cast<std::ptrdiff_t>(end), midpoint);
        std::size_t cut = static_cast<std::size_t>(found - sums.begin());
        if (cut == end) --cut;
        if (cut > first + 1 && midpoint - sums[cut - 1] < sums[cut] - midpoint) --cut;
        const double fraction = static_cast<double>((sums[cut] - sums[first]) / total);
        if (w >= h) {
            split(first, cut, x, y, w * fraction, h);
            split(cut, end, x + w * fraction, y, w * (1 - fraction), h);
        } else {
            split(first, cut, x, y, w, h * fraction);
            split(cut, end, x, y + h * fraction, w, h * (1 - fraction));
        }
    };
    split(0, items.size(), 0, 0, width, height);
    return rectangles;
}
} // namespace disklens
