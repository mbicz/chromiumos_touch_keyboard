// Copyright 2026 Michal Bicz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for the hinge-gate decision logic.  The thresholds and the
// hysteresis band are what keeps the touch keyboard from flickering while the
// machine is moved, so exercise the boundaries and the wrap-around pose.

#include <cstdio>

#include "hinge_monitor.h"

using touch_keyboard::HingePose;

static int failures = 0;

static void Expect(bool condition, const char *what) {
  if (condition) {
    printf("ok   : %s\n", what);
  } else {
    printf("FAIL : %s\n", what);
    failures++;
  }
}

int main() {
  // Booting in a normal laptop pose (the device reads 99 deg): usable.
  HingePose pose;
  Expect(pose.known() == false, "pose is unknown before the first reading");
  Expect(pose.Update(99) == true, "laptop pose 99 deg is usable");

  // Folding the machine shut disables the keyboard at once.
  Expect(pose.Update(20) == true, "20 deg is usable");
  Expect(pose.Update(10) == false, "10 deg (folded shut boundary) disables");
  Expect(pose.Update(12) == false, "12 deg stays disabled inside guard band");
  Expect(pose.Update(14) == false, "14 deg stays disabled inside guard band");
  Expect(pose.Update(15) == true, "15 deg clears the guard edge and enables");
  Expect(pose.Update(16) == true, "16 deg clears the guard band and enables");

  // Turning the surface away beyond 180 deg disables; the machine can then
  // come back through flat-on-the-desk.
  Expect(pose.Update(180) == true, "180 deg stays usable");
  Expect(pose.Update(181) == false, "181 deg disables immediately");
  Expect(pose.Update(176) == false, "176 deg stays disabled inside guard band");
  Expect(pose.Update(175) == true, "175 deg clears the guard band and enables");

  // A machine folded round behind (the driver wraps 360 back to a small
  // angle) must read as folded shut, not as a usable laptop pose.
  HingePose folded;
  Expect(folded.Update(2) == false, "2 deg (folded round) disables");
  Expect(folded.Update(45) == true, "45 deg enables once unfolded");
  Expect(folded.Update(200) == false, "200 deg disables again");
  Expect(folded.Update(99) == true, "99 deg (laptop pose) enables");

  // A boot straight into a folded pose must not type either.
  HingePose closed;
  Expect(closed.Update(0) == false, "booting at 0 deg is disabled");
  Expect(closed.usable() == false, "closed pose stays disabled");

  if (failures)
    printf("%d check(s) FAILED\n", failures);
  else
    printf("all hinge pose checks passed\n");
  return failures == 0 ? 0 : 1;
}
