# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nRF)

target_sources(app PRIVATE
    src/main.c
    src/flash_storage/flash_storage.c
)

target_include_directories(app PRIVATE src)

# ── Flash self-test toggle ────────────────────────────────────────────
# Set to ON to run erase/write/read verification on every boot.
# Set to OFF for production builds to avoid consuming flash erase cycles.
# This compiles out the entire self-test function when OFF.
set(FLASH_STORAGE_SELFTEST ON)

if(FLASH_STORAGE_SELFTEST)
    zephyr_compile_definitions(CONFIG_FLASH_STORAGE_SELFTEST)
    message(STATUS "Flash self-test: ENABLED")
else()
    message(STATUS "Flash self-test: DISABLED (production mode)")
endif()