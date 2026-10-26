// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "evdevsource.h"

#include <cerrno>

namespace touch_keyboard {

bool EvdevSource::OpenSourceDevice(std::string const &source_device_path) {
  source_fd_ = syscall_handler_->open(source_device_path.c_str(), O_RDONLY);
  if (source_fd_ < 0) {
    PLOG(ERROR) << "Failed to open() source device " << source_device_path << ". (" << source_fd_ << ")\n";
    return false;
  }
  return true;
}

bool EvdevSource::SeedMultitouchState(
    mtstatemachine::MtStateMachine *state) const {
  int64_t axes[(ABS_CNT + 63) / 64] = {};
  if (syscall_handler_->ioctl(source_fd_, EVIOCGBIT(EV_ABS, sizeof(axes)),
                              axes) < 0) {
    LOG(WARNING) << "Cannot query native ABS capabilities: " << strerror(errno)
                 << "; waiting for source ABS updates.\n";
    return false;
  }
  const auto supported = [&axes](int axis) {
    return (static_cast<uint64_t>(axes[axis / 64]) &
            (uint64_t{1} << (axis % 64))) != 0;
  };
  if (!supported(ABS_MT_SLOT)) {
    LOG(WARNING) << "Cannot seed source without native multitouch slots; "
                    "waiting for source ABS updates.\n";
    return false;
  }
  struct input_absinfo slots;
  if (syscall_handler_->ioctl(source_fd_, EVIOCGABS(ABS_MT_SLOT), &slots) < 0) {
    LOG(WARNING) << "Cannot seed native multitouch slots: " << strerror(errno)
                 << "; waiting for source ABS updates.\n";
    return false;
  }
  if (slots.minimum != 0 || slots.maximum < 0 ||
      slots.maximum >= mtstatemachine::kNumSlots ||
      slots.value < 0 || slots.value > slots.maximum) {
    LOG(WARNING) << "Cannot seed unsupported multitouch slot range; "
                    "waiting for source ABS updates.\n";
    return false;
  }
  state->slot_ = slots.value;
  bool complete = true;
  for (int axis : {ABS_MT_POSITION_X, ABS_MT_POSITION_Y,
                   ABS_MT_PRESSURE, ABS_MT_TOUCH_MAJOR}) {
    // EVIOCGABS alone can return zeroes even for an unsupported axis. Use
    // the capability bits so absent pressure/size remain missing (-1).
    if (!supported(axis)) {
      if (axis == ABS_MT_PRESSURE || axis == ABS_MT_TOUCH_MAJOR)
        continue;
      LOG(WARNING) << "Missing native multitouch axis " << axis
                   << "; waiting for source ABS updates.\n";
      complete = false;
      continue;
    }
    int values[mtstatemachine::kNumSlots + 1];
    values[0] = axis;
    const int count = slots.maximum + 1;
    if (syscall_handler_->ioctl(source_fd_,
          EVIOCGMTSLOTS((count + 1) * sizeof(values[0])), values) < 0) {
      LOG(WARNING) << "Cannot seed native multitouch axis " << axis << ": "
                   << strerror(errno) << "; waiting for source ABS updates.\n";
      complete = false;
      continue;
    }
    const mtstatemachine::EventKey key(EV_ABS, axis);
    for (int slot = 0; slot < count; ++slot)
      state->slots_[slot][key] = values[slot + 1];
  }
  // Do not seed ABS_MT_TRACKING_ID: existing fingers must lift before they
  // can type. A new tracking ID can reuse the native geometry immediately.
  state->DropContacts();
  return complete;
}

bool EvdevSource::GetNextEvent(int timeout_ms, struct input_event *ev) const {
  if (timeout_ms > 0) {
    int num_ready;
    struct timeval timeout = {0, timeout_ms * 1000};
    fd_set set;

    // Block until there's something to read or we hit a timeout.
    FD_ZERO(&set);
    FD_SET(source_fd_, &set);
    num_ready = syscall_handler_->select(source_fd_ + 1, &set,
                                         NULL, NULL, &timeout);

    // If the timeout triggered, return false instead of waiting forever.
    if (num_ready != 1) {
      return false;
    }
  }

  int num_bytes_read = syscall_handler_->read(source_fd_, ev, sizeof(*ev));
  if (num_bytes_read != sizeof(*ev)) {
    PLOG(ERROR) << "ERROR: A read failed to read an entire event. Only read " <<
                   num_bytes_read << " of " << sizeof(*ev) << "expected bytes.\n";
    if (num_bytes_read == -1) {
	    throw "Evdev read() failed";
    }
    return false;
  }
  return true;
}

}  // namespace touch_keyboard
