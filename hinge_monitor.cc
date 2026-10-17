// Copyright 2026 Michal Bicz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <stdlib.h>
#include <chrono>
#include <fstream>
#include <string>

#include <logging.h>

#include "hinge_monitor.h"

namespace touch_keyboard {

namespace {

constexpr char kIIODevicesDirectory[] = "/sys/bus/iio/devices";

// Read a small sysfs attribute, trimming the trailing newline.  Returns false
// on any error so callers treat a missing attribute as "not this device".
bool ReadAttribute(std::string const &path, std::string *value) {
  std::ifstream stream(path);
  std::string line;
  if (!stream.is_open() || !std::getline(stream, line))
    return false;
  size_t end = line.find_last_not_of(" \t\r\n");
  *value = (end == std::string::npos) ? "" : line.substr(0, end + 1);
  return true;
}

bool IsDirectory(std::string const &path) {
  DIR *dir = opendir(path.c_str());
  if (dir == NULL)
    return false;
  closedir(dir);
  return true;
}

}  // namespace

HingeMonitor::~HingeMonitor() {
  running_.store(false);
  if (thread_.joinable())
    thread_.join();
}

bool HingeMonitor::FindHingeAnglePath(std::string *path) const {
  DIR *devices = opendir(kIIODevicesDirectory);
  if (devices == NULL) {
    LOG(WARNING) << "No IIO devices, hinge pose is unknown.\n";
    return false;
  }

  while (struct dirent *entry = readdir(devices)) {
    std::string name = entry->d_name;
    if (name.compare(0, 7, "iio:dev") != 0)
      continue;

    std::string device = std::string(kIIODevicesDirectory) + "/" + name;
    if (!IsDirectory(device))
      continue;

    // The hinge driver exposes three angle channels; the first one is the angle
    // between the halves and carries the label "hinge".
    std::string label;
    if (!ReadAttribute(device + "/in_angl0_label", &label) || label != "hinge")
      continue;

    std::string raw_path = device + "/in_angl0_raw";
    std::string raw;
    if (!ReadAttribute(raw_path, &raw))
      continue;

    LOG(INFO) << "Hinge angle sensor: " << raw_path << " (" << raw << " deg)\n";
    *path = raw_path;
    closedir(devices);
    return true;
  }

  closedir(devices);
  LOG(WARNING) << "No hinge angle sensor, touch keyboard stays enabled.\n";
  return false;
}

void HingeMonitor::Run(std::function<void(bool)> callback) {
  HingePose pose;

  while (running_.load()) {
    std::string value;
    if (ReadAttribute(angle_path_, &value)) {
      int angle = atoi(value.c_str());
      bool was_usable = pose.usable();
      bool was_known = pose.known();
      bool usable = pose.Update(angle);

      if (!was_known || usable != was_usable) {
        LOG(INFO) << "Hinge angle " << angle << " deg, touch keyboard " <<
          (usable ? "enabled" : "disabled") << ".\n";
        callback(usable);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kPollIntervalMs));
  }

  // Open the gate when the monitor stops, so a stopped monitor cannot leave
  // the keyboard stuck off.
  if (pose.known() && !pose.usable())
    callback(true);
}

bool HingeMonitor::Start(std::function<void(bool usable)> callback) {
  if (!FindHingeAnglePath(&angle_path_))
    return false;

  running_.store(true);
  thread_ = std::thread(&HingeMonitor::Run, this, callback);
  return true;
}

}  // namespace touch_keyboard
