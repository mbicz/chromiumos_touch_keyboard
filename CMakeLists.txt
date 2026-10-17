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


pkg_check_modules(SYSTEMD "systemd")
# Only take the unit directory from pkg-config when the module is present:
# pkg_get_variable() clobbers an explicitly configured
# SYSTEMD_SYSTEM_UNIT_DIR with an empty value when it is not, which then
# breaks the install() rule below.
if(SYSTEMD_FOUND)
	pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
endif()
if(NOT SYSTEMD_SYSTEM_UNIT_DIR OR SYSTEMD_SYSTEM_UNIT_DIR STREQUAL "/lib/systemd/system")
	set(SYSTEMD_SYSTEM_UNIT_DIR "/usr/lib/systemd/system")
endif()
if(NOT UDEV_DIR OR UDEV_DIR STREQUAL "/lib/udev")
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

install(FILES 61-yogabook.quirks
	DESTINATION ${CMAKE_INSTALL_DATADIR}/libinput)

install(FILES touch-keyboard-handler.service
	DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR})

