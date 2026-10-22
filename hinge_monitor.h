// Copyright 2026 Michal Bicz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOUCH_KEYBOARD_HINGE_MONITOR_H_
#define TOUCH_KEYBOARD_HINGE_MONITOR_H_

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace touch_keyboard {

// Decides whether a hinge angle means the printed touch keyboard is reachable.
//
// The two halves of this machine are joined by a 360-degree hinge and the
// embedded controller reports the angle between them through the
// hid-sensor-custom-intel-hinge driver's "hinge" channel (in_angl0_label ==
// "hinge", in_angl0_raw, in whole degrees, wrapping at 360).  Two poses put
// the printed keyboard out of reach, where accidental contacts there must not
// become keystrokes:
//
//   * folded shut, angle near 0 (hinge ~ 1 when closed);
//   * unfolded past the keyboard, angle above 180 (hinge ~ 178 when the panel
//     lies flat with the keyboard against the desk, ~360 folded round behind).
//
// Normal laptop poses (hinge ~ 99) and a flat machine (hinge ~ 178) are usable.
// The sensor fuses the two half angles, so the raw reading moves even when the
// pose does not; both thresholds are therefore applied with a hysteresis band
// instead of as bare comparisons.
class HingePose {
 public:
  // Angle in degrees at or below which the halves count as folded shut.
  static constexpr int kUsableMinDeg = 10;
  // Angle in degrees above which the surface counts as turned away.
  static constexpr int kUsableMaxDeg = 180;
  // Guard band around both thresholds: the reading must clear a threshold by
  // this much before the pose flips back.
  static constexpr int kHysteresisDeg = 5;

  // Feed the current angle and get the usability that applies to it.  The
  // first call establishes the pose without hysteresis.
  bool Update(int angle_deg) {
    if (!known_) {
      known_ = true;
      usable_ = IsUsable(angle_deg);
      return usable_;
    }

    if (usable_ && !IsUsable(angle_deg))
      usable_ = false;
    else if (!usable_ && angle_deg >= kUsableMinDeg + kHysteresisDeg &&
             angle_deg <= kUsableMaxDeg - kHysteresisDeg)
      usable_ = true;

    return usable_;
  }

  // True once an angle has been fed in.
  bool known() const { return known_; }
  // Whether the pose most recently fed in keeps the keyboard reachable.
  bool usable() const { return usable_; }

 private:
  static bool IsUsable(int angle_deg) {
    return angle_deg > kUsableMinDeg && angle_deg <= kUsableMaxDeg;
  }

  bool known_ = false;
  bool usable_ = true;
};

// Polls the hinge sensor and reports pose changes.
//
// The sensor is located by the label of its first angle channel rather than by
// an iio:deviceN number, because those numbers follow probe order.
class HingeMonitor {
 public:
  static constexpr int kPollIntervalMs = 500;

  HingeMonitor() : angle_path_(""), running_(false) {}
  ~HingeMonitor();

  // Find the hinge sensor and start reporting pose changes on a poll thread.
  // The callback is called as soon as the current pose is known and again
  // whenever the pose flips.  Returns false without calling the callback when
  // no hinge sensor is present, so callers leave their gate open.
  bool Start(std::function<void(bool usable)> callback);

 private:
  // Look for the IIO device exposing the hinge channel and store the path of
  // its raw angle attribute.
  bool FindHingeAnglePath(std::string *path) const;

  // Poll the angle and call the callback on pose changes.
  void Run(std::function<void(bool)> callback);

  std::string angle_path_;
  std::atomic<bool> running_;
  std::thread thread_;
};

}  // namespace touch_keyboard

#endif  // TOUCH_KEYBOARD_HINGE_MONITOR_H_
