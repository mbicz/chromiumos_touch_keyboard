// Copyright 2026 Michal Bicz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOUCH_KEYBOARD_KEYBOARD_BACKLIGHT_H_
#define TOUCH_KEYBOARD_KEYBOARD_BACKLIGHT_H_

#include <string>

namespace touch_keyboard {

class KeyboardBacklight {
 public:
  // Prefer the Android-model LED; otherwise use only the known Windows LED.
  KeyboardBacklight();
  // The explicit directory also lets tests use ordinary attribute files.
  explicit KeyboardBacklight(const std::string &directory);

  // Consume plain F10, including releases and repeats, even if the LED fails.
  // Other keycodes (including Fn+F10's KEY_KEYBOARD) remain ordinary events.
  bool HandleKeyEvent(int code, int value);

 private:
  void Cycle();

  const std::string brightness_path_;
  const std::string maximum_path_;
  bool pressed_ = false;
};

}  // namespace touch_keyboard

#endif  // TOUCH_KEYBOARD_KEYBOARD_BACKLIGHT_H_
