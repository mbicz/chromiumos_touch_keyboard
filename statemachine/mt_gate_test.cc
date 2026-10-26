// Copyright 2026 Michal Bicz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>
#include <cstdio>
#include <unordered_map>

#include "statemachine/statemachine.h"
#include "evdevsource.h"

using mtstatemachine::MtFinger;
using mtstatemachine::MtStateMachine;
using Snapshot = std::unordered_map<int, MtFinger>;

static int failures = 0;

static void Expect(bool condition, const char *what) {
  printf("%s: %s\n", condition ? "ok" : "FAIL", what);
  if (!condition)
    ++failures;
}

static bool Event(MtStateMachine &state, int type, int code, int value,
                  Snapshot *snapshot = nullptr) {
  struct input_event event = {};
  event.type = type;
  event.code = code;
  event.value = value;
  return state.AddEvent(event, snapshot);
}

static void Axis(MtStateMachine &state, int code, int value) {
  Event(state, EV_ABS, code, value);
}

static void Frame(MtStateMachine &state, Snapshot *snapshot) {
  Expect(Event(state, EV_SYN, SYN_REPORT, 0, snapshot),
         "SYN_REPORT completes a frame");
}

static void ExpectFinger(const Snapshot &snapshot, int tid, int x, int y,
                         int pressure, int major, const char *what) {
  auto it = snapshot.find(tid);
  Expect(it != snapshot.end() && it->second.x == x && it->second.y == y &&
         it->second.p == pressure && it->second.touch_major == major, what);
}

// Emulate only the kernel's startup state ioctls through the existing syscall
// seam, then exercise actual tracking-ID-only input against the seeded cache.
class NativeSlots : public touch_keyboard::SyscallHandler {
 public:
  bool pressure = true;
  bool fail_slots = false;
  int fail_axis = -1;

  int ioctl(int, long request, int64_t *axes) const override {
    if (static_cast<unsigned long>(request) !=
        EVIOCGBIT(EV_ABS, ((ABS_CNT + 63) / 64) * sizeof(int64_t))) {
      errno = EINVAL;
      return -1;
    }
    for (int axis : {ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y,
                     ABS_MT_PRESSURE, ABS_MT_TOUCH_MAJOR}) {
      if (axis != ABS_MT_PRESSURE || pressure)
        axes[axis / 64] |= static_cast<int64_t>(uint64_t{1} << (axis % 64));
    }
    return sizeof(int64_t) * ((ABS_CNT + 63) / 64);
  }

  int ioctl(int, long request, struct input_absinfo *info) const override {
    if (static_cast<unsigned long>(request) == EVIOCGABS(ABS_MT_SLOT)) {
      if (fail_slots) {
        errno = EIO;
        return -1;
      }
      *info = {};
      info->maximum = 3;
      info->value = 3;
      return 0;
    }
    for (int axis : {ABS_MT_POSITION_X, ABS_MT_POSITION_Y,
                     ABS_MT_PRESSURE, ABS_MT_TOUCH_MAJOR}) {
      // Like evdev, ABS info alone does not establish capability support.
      if (static_cast<unsigned long>(request) == EVIOCGABS(axis)) {
        *info = {};
        return 0;
      }
    }
    errno = EINVAL;
    return -1;
  }

  int ioctl(int, long request, int *values) const override {
    if (static_cast<unsigned long>(request) != EVIOCGMTSLOTS(5 * sizeof(int)) ||
        values[0] == fail_axis) {
      errno = EIO;
      return -1;
    }
    for (int slot = 0; slot < 4; ++slot) {
      switch (values[0]) {
        case ABS_MT_POSITION_X: values[slot + 1] = slot == 3 ? 1131 : 403; break;
        case ABS_MT_POSITION_Y: values[slot + 1] = slot == 3 ? 1269 : 58; break;
        case ABS_MT_PRESSURE: values[slot + 1] = 80; break;
        case ABS_MT_TOUCH_MAJOR: values[slot + 1] = 500; break;
        default: errno = EINVAL; return -1;
      }
    }
    return 0;
  }
};

class SeedSource : public touch_keyboard::EvdevSource {
 public:
  explicit SeedSource(NativeSlots *device) : EvdevSource(device) {
    source_fd_ = 7;
  }
  using EvdevSource::SeedMultitouchState;
};

static void TestStartupSeed() {
  NativeSlots device;
  SeedSource source(&device);
  MtStateMachine restarted;
  Snapshot snapshot;
  Expect(source.SeedMultitouchState(&restarted), "seed existing native source");
  Frame(restarted, &snapshot);
  Expect(snapshot.empty(), "startup does not promote already-held contacts");
  Axis(restarted, ABS_MT_POSITION_X, 1130);
  Frame(restarted, &snapshot);
  Expect(snapshot.empty(), "moving pre-startup contact does not resurrect it");
  Axis(restarted, ABS_MT_TRACKING_ID, -1);
  Frame(restarted, &snapshot);
  Axis(restarted, ABS_MT_TRACKING_ID, 300);
  Frame(restarted, &snapshot);
  ExpectFinger(snapshot, 300, 1130, 1269, 80, 500,
               "restart accepts new tracking ID without geometry resend");
  Axis(restarted, ABS_MT_SLOT, 1);
  Axis(restarted, ABS_MT_TRACKING_ID, 301);
  Frame(restarted, &snapshot);
  ExpectFinger(snapshot, 301, 403, 58, 80, 500,
               "startup imports inactive slot geometry too");

  device.pressure = false;
  MtStateMachine no_pressure;
  Expect(source.SeedMultitouchState(&no_pressure), "absent pressure is supported");
  Axis(no_pressure, ABS_MT_TRACKING_ID, 302);
  Frame(no_pressure, &snapshot);
  ExpectFinger(snapshot, 302, 1131, 1269, -1, 500,
               "unsupported pressure remains missing rather than fake zero");

  device.fail_axis = ABS_MT_POSITION_X;
  MtStateMachine failed_axis;
  Expect(!source.SeedMultitouchState(&failed_axis),
         "axis ioctl failure is reported without crashing");
  Axis(failed_axis, ABS_MT_TRACKING_ID, 303);
  Frame(failed_axis, &snapshot);
  ExpectFinger(snapshot, 303, -1, 1269, -1, 500,
               "failed native axis is not fabricated");
  Axis(failed_axis, ABS_MT_POSITION_X, 1131);
  Frame(failed_axis, &snapshot);
  ExpectFinger(snapshot, 303, 1131, 1269, -1, 500,
               "real source update can recover a failed startup axis");

  device.fail_slots = true;
  MtStateMachine failed_slots;
  Expect(!source.SeedMultitouchState(&failed_slots),
         "slot ioctl failure is reported without crashing");
  Frame(failed_slots, &snapshot);
  Expect(snapshot.empty(), "failed source does not create fake contacts");
}

int main() {
  TestStartupSeed();
  MtStateMachine state;
  Snapshot snapshot;

  // Native type-B slots retain ABS values across contacts. A new tracking ID
  // at precisely the same place may be the only delta in its entire frame.
  Axis(state, ABS_MT_SLOT, 3);
  Axis(state, ABS_MT_TRACKING_ID, 100);
  Axis(state, ABS_MT_POSITION_X, 1131);
  Axis(state, ABS_MT_POSITION_Y, 1269);
  Axis(state, ABS_MT_PRESSURE, 80);
  Axis(state, ABS_MT_TOUCH_MAJOR, 500);
  Frame(state, &snapshot);
  ExpectFinger(snapshot, 100, 1131, 1269, 80, 500, "initial F10 geometry");

  // Close the gate with a contact held. The daemon drains events with no
  // snapshot consumer: a release and later press need not resend axes.
  Axis(state, ABS_MT_TRACKING_ID, -1);
  Frame(state, nullptr);
  state.DropContacts();  // reopening the gate
  Axis(state, ABS_MT_TRACKING_ID, 101);  // no SLOT, position, or size events
  Frame(state, &snapshot);
  Expect(snapshot.size() == 1, "new tracking ID accepted without axis jitter");
  ExpectFinger(snapshot, 101, 1131, 1269, 80, 500,
               "new same-coordinate F10 retains all cached ABS values");
  Expect(state.slot_ == 3, "gate reopening preserves selected native slot");

  // A contact still held when reopening must stay suppressed forever, even
  // if it later moves onto another key. No tracking ID means no new press.
  state.DropContacts();
  Frame(state, &snapshot);
  Expect(snapshot.empty(), "held contact suppressed at gate reopen");
  Axis(state, ABS_MT_POSITION_X, 1100);
  Axis(state, ABS_MT_POSITION_Y, 1300);
  Frame(state, &snapshot);
  Expect(snapshot.empty(), "moving held contact cannot resurrect it");
  Axis(state, ABS_MT_TRACKING_ID, -1);
  Frame(state, &snapshot);
  Expect(snapshot.empty(), "suppressed contact release remains empty");
  Axis(state, ABS_MT_TRACKING_ID, 102);
  Frame(state, &snapshot);
  ExpectFinger(snapshot, 102, 1100, 1300, 80, 500,
               "genuinely new ID uses latest axes of suppressed contact");

  // While disabled, keep processing all slots and every ABS delta. Contacts
  // that begin while disabled are also suppressed on reopening, not promoted
  // just because they were not present when the gate originally closed.
  Axis(state, ABS_MT_SLOT, 1);
  Axis(state, ABS_MT_TRACKING_ID, 200);
  Axis(state, ABS_MT_POSITION_X, 403);
  Axis(state, ABS_MT_POSITION_Y, 58);
  Axis(state, ABS_MT_PRESSURE, 70);
  Axis(state, ABS_MT_TOUCH_MAJOR, 600);
  Frame(state, nullptr);
  Axis(state, ABS_MT_SLOT, 3);
  Axis(state, ABS_MT_POSITION_X, 1131);
  Axis(state, ABS_MT_POSITION_Y, 1269);
  Axis(state, ABS_MT_TOUCH_MAJOR, 550);
  Frame(state, nullptr);
  state.DropContacts();
  Frame(state, &snapshot);
  Expect(snapshot.empty(), "all contacts held during disabled interval suppressed");
  Axis(state, ABS_MT_SLOT, 1);
  Axis(state, ABS_MT_POSITION_X, 410);
  Frame(state, &snapshot);
  Expect(snapshot.empty(), "moving gated Fn contact cannot latch Fn");

  Axis(state, ABS_MT_TRACKING_ID, -1);
  Frame(state, &snapshot);
  Axis(state, ABS_MT_TRACKING_ID, 201);
  Frame(state, &snapshot);
  ExpectFinger(snapshot, 201, 410, 58, 70, 600,
               "new Fn contact retains its own native slot cache");
  Axis(state, ABS_MT_SLOT, 3);
  Axis(state, ABS_MT_TRACKING_ID, -1);
  Frame(state, &snapshot);
  Axis(state, ABS_MT_TRACKING_ID, 103);
  Frame(state, &snapshot);
  Expect(snapshot.size() == 2, "independent new contacts work after gating");
  ExpectFinger(snapshot, 103, 1131, 1269, 80, 550,
               "F10 inherits axis updates drained while gate was closed");
  ExpectFinger(snapshot, 201, 410, 58, 70, 600,
               "F10 slot does not overwrite Fn slot cache");

  // Repeated hinge cycles do not discard native deltas or selected slot.
  for (int tid = 104; tid < 107; ++tid) {
    state.DropContacts();
    Frame(state, &snapshot);
    Expect(snapshot.empty(), "repeated gate cycles suppress held contacts");
    Axis(state, ABS_MT_TRACKING_ID, -1);
    Frame(state, nullptr);
    state.DropContacts();
    Axis(state, ABS_MT_TRACKING_ID, tid);
    Frame(state, &snapshot);
    ExpectFinger(snapshot, tid, 1131, 1269, 80, 550,
                 "repeated reopen accepts tracking-ID-only new F10 press");
  }

  return failures == 0 ? 0 : 1;
}
