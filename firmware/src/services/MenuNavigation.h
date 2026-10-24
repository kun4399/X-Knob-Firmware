#pragma once

// Apply motor detent steps directly to a circular menu. One detent always
// advances or reverses one item; no half-step accumulator is used.
static inline int menu_navigation_apply_delta(int selected, int delta, int count) {
    if (count <= 0) return 0;
    const int step = delta % count;
    return (selected + step + count) % count;
}
