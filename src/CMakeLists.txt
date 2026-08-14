add_library(pixhawk_telemetry STATIC)

target_include_directories(pixhawk_telemetry PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(pixhawk_telemetry PUBLIC
  mavlink_headers
  nlohmann_json::nlohmann_json)

add_subdirectory(telemetry)
add_subdirectory(warning)
add_subdirectory(tts)
add_subdirectory(utils)

add_executable(pixhawk_monitor main.cpp)
target_link_libraries(pixhawk_monitor PRIVATE
  pixhawk_telemetry
  CLI11::CLI11)

add_executable(pixhawk_telemetry_hub telemetry_hub.cpp)
target_link_libraries(pixhawk_telemetry_hub PRIVATE
  pixhawk_telemetry
  CLI11::CLI11)

if(BUILD_PFD)
  find_package(Qt5 5.12 COMPONENTS Core Gui Qml Quick QUIET)
  if(Qt5_FOUND)
    add_subdirectory(ui)
  else()
    message(WARNING
      "Qt 5 Core/Gui/Qml/Quick not found; pixhawk_pfd will not be built")
  endif()
endif()
