target_sources(pixhawk_telemetry PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/pixhawk_connection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/telemetry_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/telemetry_recorder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/telemetry_replay.cpp)
