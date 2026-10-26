cmake_minimum_required(VERSION 3.22)

project(chromiumos_touch_keyboard)

find_package(PkgConfig)

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -pedantic -Wno-unknown-pragmas")

add_executable(touch_keyboard_handler
	main.cc
	evdevsource.cc
	fakekeyboard.cc
	hinge_monitor.cc
	keyboard_backlight.cc
	faketouchpad.cc
	uinputdevice.cc
	haptic/ff_driver.cc
	haptic/touch_ff_manager.cc
	statemachine/eventkey.cc
	statemachine/slot.cc
	statemachine/statemachine.cc
	logging.cc
	)

include(GNUInstallDirs)

# Unit test for the hinge-gate decision logic.  It only exercises the
# threshold and hysteresis boundaries, so it needs nothing but the monitor
# and the logging helper.
add_executable(hinge_monitor_test
	hinge_monitor_test.cc
	hinge_monitor.cc
	logging.cc
	)
enable_testing()
add_test(NAME hinge_monitor_test COMMAND hinge_monitor_test)

add_executable(keyboard_backlight_test
	keyboard_backlight_test.cc
	keyboard_backlight.cc
	logging.cc
	)
add_test(NAME keyboard_backlight_test COMMAND keyboard_backlight_test)

add_executable(mt_gate_test
	statemachine/mt_gate_test.cc
	evdevsource.cc
	logging.cc
	statemachine/statemachine.cc
	statemachine/slot.cc
	statemachine/eventkey.cc
	)
add_test(NAME mt_gate_test COMMAND mt_gate_test)


# systemd.pc and udev.pc ship in systemd-dev on Debian 13 (trixie) and later,
# in systemd and udev before. Use them when present, and the standard
# locations otherwise; an empty directory would break the install() rules.
# A value given on the command line (-DSYSTEMD_SYSTEM_UNIT_DIR=...) wins.
pkg_check_modules(SYSTEMD "systemd")
if(NOT SYSTEMD_SYSTEM_UNIT_DIR AND SYSTEMD_FOUND)
	pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
endif()
if(NOT SYSTEMD_SYSTEM_UNIT_DIR)
	set(SYSTEMD_SYSTEM_UNIT_DIR "/usr/lib/systemd/system")
endif()

pkg_check_modules(UDEV "udev")
if(NOT UDEV_DIR AND UDEV_FOUND)
	pkg_get_variable(UDEV_DIR udev udevdir)
endif()
if(NOT UDEV_DIR)
	set(UDEV_DIR "/usr/lib/udev")
endif()

install(TARGETS touch_keyboard_handler
	DESTINATION "${CMAKE_INSTALL_SBINDIR}")

install(DIRECTORY layouts
	DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/touch_keyboard)

install(FILES layout-touchpad.csv touch-hw.csv
	DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/touch_keyboard)

install(FILES 60-touch-keyboard.rules
	DESTINATION ${UDEV_DIR}/rules.d)

install(FILES 61-evdev-yogabook.hwdb
	DESTINATION ${UDEV_DIR}/hwdb.d)

install(FILES touch-keyboard-handler.service
	DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR})

