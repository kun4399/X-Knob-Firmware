#include "services/MenuNavigation.h"
#include <cassert>

int main() {
    assert(menu_navigation_apply_delta(0, 1, 6) == 1);  // one detent, one app
    assert(menu_navigation_apply_delta(2, -1, 6) == 1);
    assert(menu_navigation_apply_delta(5, 1, 6) == 0);  // clockwise wrap
    assert(menu_navigation_apply_delta(0, -1, 6) == 5); // counterclockwise wrap
    assert(menu_navigation_apply_delta(0, 0, 6) == 0);
    assert(menu_navigation_apply_delta(3, 13, 6) == 4);
    assert(menu_navigation_apply_delta(2, 1, 0) == 0);
}
