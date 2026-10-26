// Copyright 2026 Michal Bicz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "keyboard_backlight.h"

#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include "logging.h"

namespace touch_keyboard {
namespace {

const char *BacklightDirectory() {
  const char *android = "/sys/class/leds/yogabook::kbd_backlight";
  const char *windows = "/sys/class/leds/ybwmi::kbd_backlight";
  // Only absence selects the other known Yoga Book interface. Do not mask
  // permission errors or search arbitrary keyboard LEDs.
  if (access(android, F_OK) < 0 && errno == ENOENT &&
      access(windows, F_OK) == 0)
    return windows;
  return android;
}

bool AttributeError(const std::string &path, int error) {
  LOG(ERROR) << "Keyboard backlight: " << path << ": " << strerror(error)
             << "; brightness was not changed successfully.\n";
  return false;
}

bool ReadLevel(const std::string &path, int *level) {
  int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return AttributeError(path, errno);
  char buffer[64];
  ssize_t count = read(fd, buffer, sizeof(buffer) - 1);
  int error = errno;
  close(fd);
  if (count < 0)
    return AttributeError(path, error);
  if (count == 0 || count == static_cast<ssize_t>(sizeof(buffer) - 1))
    return AttributeError(path, EINVAL);
  buffer[count] = '\0';
  char *end;
  errno = 0;
  long value = strtol(buffer, &end, 10);
  if (errno != 0 || end == buffer || value < 0 || value > INT_MAX)
    return AttributeError(path, EINVAL);
  while (isspace(static_cast<unsigned char>(*end)))
    ++end;
  if (*end != '\0')
    return AttributeError(path, EINVAL);
  *level = static_cast<int>(value);
  return true;
}

}  // namespace

KeyboardBacklight::KeyboardBacklight()
    : KeyboardBacklight(BacklightDirectory()) {}

KeyboardBacklight::KeyboardBacklight(const std::string &directory)
    : brightness_path_(directory + "/brightness"),
      maximum_path_(directory + "/max_brightness") {}

bool KeyboardBacklight::HandleKeyEvent(int code, int value) {
  if (code != KEY_F10)
    return false;
  if (value == 0) {
    pressed_ = false;
  } else if (value == 1 && !pressed_) {
    pressed_ = true;
    Cycle();
  }
  return true;
}

void KeyboardBacklight::Cycle() {
  int maximum, current;
  // Read on each press: startup and external brightness changes must not
  // leave a cached level out of step with the actual LED.
  if (!ReadLevel(maximum_path_, &maximum) ||
      !ReadLevel(brightness_path_, &current))
    return;
  if (maximum <= 0 || current > maximum) {
    AttributeError(brightness_path_, EINVAL);
    return;
  }
  const int half = maximum / 2 + maximum % 2;
  const int next = current < half ? half : current < maximum ? maximum : 0;
  char buffer[32];
  int length = snprintf(buffer, sizeof(buffer), "%d\n", next);
  // Do not create a missing attribute, or search for unrelated keyboard LEDs.
  int fd = open(brightness_path_.c_str(), O_WRONLY | O_CLOEXEC | O_TRUNC);
  if (fd < 0) {
    AttributeError(brightness_path_, errno);
    return;
  }
  ssize_t written = write(fd, buffer, length);
  int error = errno;
  int closed = close(fd);
  if (written != length) {
    AttributeError(brightness_path_, written < 0 ? error : EIO);
    return;
  }
  if (closed < 0) {
    AttributeError(brightness_path_, errno);
    return;
  }
  LOG(INFO) << "Keyboard backlight: " << current << " -> " << next
            << " (max " << maximum << ").\n";
}

}  // namespace touch_keyboard
