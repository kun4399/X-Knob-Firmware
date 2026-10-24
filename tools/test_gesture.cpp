#include "services/GestureFSM.h"
#include <cassert>

int main() {
    GestureFSM g;
    g.update(false, 0, 0);
    g.update(true, 0, 10);
    g.update(true, 0, 40);
    g.update(false, 0, 100);
    g.update(false, 0, 130);
    assert(g.update(false, 0, 551).kind == GestureFSM::SINGLE_CLICK);

    // Mechanical shaft movement during a button click must be consumed by the
    // gesture arbiter instead of leaking into a brightness adjustment.
    GestureFSM click_with_shaft_motion;
    click_with_shaft_motion.update(false, 0, 0);
    click_with_shaft_motion.update(true, 0, 10);
    click_with_shaft_motion.update(true, 0, 40);
    assert(click_with_shaft_motion.update(true, 1, 80).kind == GestureFSM::NONE);
    click_with_shaft_motion.update(false, 1, 100);
    assert(click_with_shaft_motion.update(false, 1, 130).kind == GestureFSM::NONE);
    assert(click_with_shaft_motion.update(false, 1, 551).kind == GestureFSM::SINGLE_CLICK);

    GestureFSM d;
    d.update(false, 0, 0);
    d.update(true, 0, 10); d.update(true, 0, 40);
    d.update(false, 0, 70); d.update(false, 0, 100);
    d.update(true, 0, 150); d.update(true, 0, 180);
    d.update(false, 0, 210);
    assert(d.update(false, 0, 240).kind == GestureFSM::DOUBLE_CLICK);

    GestureFSM h;
    h.update(false, 0, 0);
    h.update(true, 0, 10); h.update(true, 0, 40);
    assert(h.update(true, 1, 170).kind == GestureFSM::HOLD_ROTATE);
    assert(h.update(true, 1, 1240).kind == GestureFSM::NONE);
    h.update(false, 1, 1300);
    assert(h.update(false, 1, 1330).kind == GestureFSM::NONE);

    // A turn arriving on the long-press boundary wins over a stationary
    // long-press event and suppresses the click/long action for that hold.
    GestureFSM hold_and_turn_at_threshold;
    hold_and_turn_at_threshold.update(false, 0, 0);
    hold_and_turn_at_threshold.update(true, 0, 10);
    hold_and_turn_at_threshold.update(true, 0, 40);
    assert(hold_and_turn_at_threshold.update(true, 0, 1230).kind == GestureFSM::NONE);
    GestureFSM::Event boundary_turn = hold_and_turn_at_threshold.update(true, 1, 1240);
    assert(boundary_turn.kind == GestureFSM::HOLD_ROTATE && boundary_turn.delta == 1);
    hold_and_turn_at_threshold.update(false, 1, 1300);
    assert(hold_and_turn_at_threshold.update(false, 1, 1330).kind == GestureFSM::NONE);

    GestureFSM wake;
    wake.update(false, 0, 0);
    wake.consume_wake_press(true);
    wake.update(true, 0, 10); wake.update(true, 0, 40);
    assert(wake.update(true, 4, 80).kind == GestureFSM::NONE);
    wake.update(false, 4, 120);
    assert(wake.update(false, 4, 150).kind == GestureFSM::NONE);
    assert(wake.update(false, 4, 500).kind == GestureFSM::NONE);

    // If the wake press has already been released before its flag is handled,
    // it must not suppress the next real click.
    GestureFSM released_wake;
    released_wake.update(false, 0, 0);
    released_wake.consume_wake_press(false);
    released_wake.update(true, 0, 10); released_wake.update(true, 0, 40);
    released_wake.update(false, 0, 100); released_wake.update(false, 0, 130);
    assert(released_wake.update(false, 0, 551).kind == GestureFSM::SINGLE_CLICK);

    GestureFSM long_press;
    long_press.update(false, 0, 0);
    long_press.update(true, 0, 10); long_press.update(true, 0, 40);
    assert(long_press.update(true, 0, 1240).kind == GestureFSM::LONG_PRESS);
    long_press.update(false, 0, 1300);
    assert(long_press.update(false, 0, 1330).kind == GestureFSM::NONE);
    // A long hold consumed by a child page cannot become a click on its parent.
    long_press.suppress_pressed_until_release(true);
    long_press.update(true, 0, 1400); long_press.update(true, 0, 1430);
    assert(long_press.update(true, 0, 2630).kind == GestureFSM::NONE);
    long_press.update(false, 0, 2680); long_press.update(false, 0, 2710);
    assert(long_press.update(false, 0, 3200).kind == GestureFSM::NONE);

    // Returning to a parent while the child's long press is still held must
    // suppress the release and must never emit a short click afterward.
    GestureFSM returned;
    returned.update(false, 0, 0);
    returned.suppress_pressed_until_release(true);
    returned.update(true, 0, 10);
    returned.update(true, 0, 40);
    returned.update(false, 0, 100);
    returned.update(false, 0, 130);
    assert(returned.update(false, 0, 600).kind == GestureFSM::NONE);
}
