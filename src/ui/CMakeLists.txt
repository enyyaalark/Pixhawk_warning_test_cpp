qt5_add_resources(PIXHAWK_PFD_RESOURCES qml.qrc)

add_executable(pixhawk_pfd
  pfd_main.cpp
  pfd_replay_model.cpp
  pfd_replay_model.hpp
  pfd_live_model.cpp
  pfd_live_model.hpp
  ${PIXHAWK_PFD_RESOURCES})

set_target_properties(pixhawk_pfd PROPERTIES
  AUTOMOC ON)

target_link_libraries(pixhawk_pfd PRIVATE
  pixhawk_telemetry
  Qt5::Core
  Qt5::Gui
  Qt5::Qml
  Qt5::Quick)
