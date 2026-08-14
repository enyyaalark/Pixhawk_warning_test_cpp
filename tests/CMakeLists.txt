add_executable(pixhawk_tests
  test_telemetry_data.cpp
  test_pixhawk_connection.cpp
  test_telemetry_reader.cpp
  test_telemetry_recorder.cpp
  test_telemetry_replay.cpp
  test_warning_engine.cpp
  test_warning_scheduler.cpp
  test_tts.cpp
  test_main_replay.cpp)

target_link_libraries(pixhawk_tests PRIVATE
  pixhawk_telemetry
  GTest::gtest_main)

target_include_directories(pixhawk_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(pixhawk_tests)
