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
