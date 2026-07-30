cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(central_min)

if(CONFIG_EMG_NETWORK_SCAN)
  target_sources(app PRIVATE
    src/network_scan.c
  )
else()
  target_sources(app PRIVATE
    src/main.c
    src/custom_client.c
    src/experiment_receiver.c
    src/sync_controller.c
  )
endif()

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common
)
