cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(peripherial_min)

target_sources(app PRIVATE
  src/main.c
  src/custom_gatt.c
  src/sync_capture.c
)

if(CONFIG_EMG_PREEXPERIMENT)
  target_sources(app PRIVATE src/preexperiment.c)
elseif(CONFIG_EMG_POSTEXPERIMENT)
  target_sources(app PRIVATE
    src/postexperiment.c
    src/energy_markers.c
  )
else()
  message(FATAL_ERROR
    "Seleccione preexperiment.conf o postexperiment.conf mediante EXTRA_CONF_FILE"
  )
endif()

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../common
)
