# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nRF)

# ── Production sources (always compiled) ──────────────────────────────
target_sources(app PRIVATE
    src/main.c
    src/flash_storage/flash_storage.c
    src/imu/imu.c
    src/shock_detector/shock_detector.c
    src/ble_service/ble_service.c
)

target_include_directories(app PRIVATE src src/tests)

# ── Bringup self-test toggle ──────────────────────────────────────────
set(BRINGUP_SELFTEST ON)

if(BRINGUP_SELFTEST)
    target_sources(app PRIVATE
        src/tests/flash_test.c
        src/tests/imu_test.c
        src/tests/shock_test.c
        src/tests/ble_test.c
    )
    zephyr_compile_definitions(CONFIG_BRINGUP_SELFTEST)
    message(STATUS "Bringup self-test: ENABLED")
else()
    message(STATUS "Bringup self-test: DISABLED (production mode)")
endif()