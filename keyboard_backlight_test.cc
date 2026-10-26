// Copyright 2026 Michal Bicz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <linux/input.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "keyboard_backlight.h"

using touch_keyboard::KeyboardBacklight;

static int failures = 0;

static void Expect(bool condition, const char *what) {
  printf("%s: %s\n", condition ? "ok" : "FAIL", what);
  if (!condition)
    ++failures;
}

static void Write(const std::string &path, const char *value) {
  std::ofstream stream(path);
  stream << value << '\n';
  stream.close();
  Expect(!stream.fail(), "prepare LED attribute");
}

static int Read(const std::string &path) {
  std::ifstream stream(path);
  int value = -1;
  stream >> value;
  return value;
}

int main() {
  char directory[] = "/tmp/touch-keyboard-backlight-XXXXXX";
  if (!mkdtemp(directory)) {
    perror("mkdtemp");
    return 1;
  }
  const std::string brightness = std::string(directory) + "/brightness";
  const std::string maximum = std::string(directory) + "/max_brightness";
  Write(maximum, "255");
  Write(brightness, "0");
  KeyboardBacklight backlight(directory);

  Expect(backlight.HandleKeyEvent(KEY_F10, 0), "orphan release is consumed");
  Expect(Read(brightness) == 0, "orphan release does not change brightness");
  Expect(backlight.HandleKeyEvent(KEY_F10, 2), "orphan repeat is consumed");
  Expect(Read(brightness) == 0, "orphan repeat does not change brightness");
  Expect(backlight.HandleKeyEvent(KEY_F10, 1), "plain F10 press is consumed");
  Expect(Read(brightness) == 128, "off advances to rounded-up half");
  Expect(backlight.HandleKeyEvent(KEY_F10, 1), "duplicate down is consumed");
  Expect(backlight.HandleKeyEvent(KEY_F10, 2), "held repeat is consumed");
  Expect(Read(brightness) == 128, "holding F10 advances only once");
  Expect(backlight.HandleKeyEvent(KEY_F10, 0), "plain F10 release is consumed");
  Expect(Read(brightness) == 128, "release does not change brightness");
  backlight.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 255, "half advances to full");
  backlight.HandleKeyEvent(KEY_F10, 0);
  backlight.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 0, "full wraps to off");
  backlight.HandleKeyEvent(KEY_F10, 0);

  // The same release path is used when the hinge gate closes on a held key.
  backlight.HandleKeyEvent(KEY_F10, 1);
  backlight.HandleKeyEvent(KEY_F10, 0);
  backlight.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 255, "forced release rearms the next press");
  backlight.HandleKeyEvent(KEY_F10, 0);

  for (int value : {1, 2, 0}) {
    Expect(!backlight.HandleKeyEvent(KEY_KEYBOARD, value),
           "Fn+F10 passes through unchanged");
    Expect(!backlight.HandleKeyEvent(KEY_F9, value),
           "other function keys pass through unchanged");
    Expect(!backlight.HandleKeyEvent(KEY_FN, value),
           "Fn press/repeat/release pass through unchanged");
  }
  Expect(Read(brightness) == 255, "Fn and unrelated keys do not touch LED");

  Write(brightness, "42");
  // A fresh handler starts from the real brightness, not an assumed level.
  KeyboardBacklight fresh(directory);
  fresh.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 128, "arbitrary low current advances to half");
  fresh.HandleKeyEvent(KEY_F10, 0);
  Write(brightness, "200");
  fresh.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 255, "external high current advances to full");
  fresh.HandleKeyEvent(KEY_F10, 0);
  Write(maximum, "100");
  Write(brightness, "0");
  fresh.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 50, "levels follow actual max_brightness");
  fresh.HandleKeyEvent(KEY_F10, 0);

  // Missing interfaces must log an error, remain consumed, and never create
  // a pretend sysfs attribute. They may recover on a subsequent real press.
  unlink(brightness.c_str());
  std::ostringstream errors;
  std::streambuf *previous = std::cout.rdbuf(errors.rdbuf());
  bool consumed = fresh.HandleKeyEvent(KEY_F10, 1);
  std::cout.rdbuf(previous);
  Expect(consumed, "missing LED still consumes F10");
  Expect(access(brightness.c_str(), F_OK) != 0, "missing LED is not created");
  Expect(errors.str().find("Keyboard backlight:") != std::string::npos &&
         errors.str().find("not changed successfully") != std::string::npos,
         "missing LED failure is visible");
  Write(brightness, "0");
  fresh.HandleKeyEvent(KEY_F10, 2);
  fresh.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 0, "failed held press never retries brightness");
  fresh.HandleKeyEvent(KEY_F10, 0);
  fresh.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 50, "new press recovers when LED returns");
  fresh.HandleKeyEvent(KEY_F10, 0);

  unlink(maximum.c_str());
  fresh.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 50, "missing maximum leaves brightness alone");
  fresh.HandleKeyEvent(KEY_F10, 0);
  for (const char *invalid : {"0", "-1", "garbage", "99999999999999999999"}) {
    Write(maximum, invalid);
    fresh.HandleKeyEvent(KEY_F10, 1);
    Expect(Read(brightness) == 50, "invalid maximum does not write brightness");
    fresh.HandleKeyEvent(KEY_F10, 0);
  }
  Write(maximum, "100");
  Write(brightness, "101");
  fresh.HandleKeyEvent(KEY_F10, 1);
  Expect(Read(brightness) == 101, "out-of-range current is not overwritten");
  fresh.HandleKeyEvent(KEY_F10, 0);

  // Exercise an actual unwritable attribute even if package tests run as root.
  // Drop privileges only in the child, and touch only this private fixture.
  Write(brightness, "50");
  chmod(directory, 0755);
  chmod(maximum.c_str(), 0444);
  chmod(brightness.c_str(), 0444);
  fflush(stdout);
  pid_t child = fork();
  if (child == 0) {
    if (geteuid() == 0 && setuid(65534) != 0)
      _exit(2);
    std::ostringstream output;
    std::cout.rdbuf(output.rdbuf());
    KeyboardBacklight readonly(directory);
    bool handled = readonly.HandleKeyEvent(KEY_F10, 1);
    bool logged = output.str().find("not changed successfully") != std::string::npos;
    _exit(handled && logged ? 0 : 1);
  }
  int status = 0;
  Expect(child > 0 && waitpid(child, &status, 0) == child &&
         WIFEXITED(status) && WEXITSTATUS(status) == 0,
         "unwritable LED is consumed and logs failure without crashing");
  Expect(Read(brightness) == 50, "unwritable brightness is unchanged");

  unlink(brightness.c_str());
  unlink(maximum.c_str());
  rmdir(directory);
  return failures == 0 ? 0 : 1;
}
