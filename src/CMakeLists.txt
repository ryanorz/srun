
set(SRUND_SRC
  srund.cpp
  manager.cpp
  output.cpp
  utils.cpp
)
add_executable(srund ${SRUND_SRC})
target_link_libraries(srund
  srun_proto
  ${PROTOBUF_LIBRARIES}
  ${Boost_LIBRARIES}
)

set(SRUNCTL_SRC
  srunctl.cpp
  srunc.cpp
  utils.cpp
)
add_executable(srunctl ${SRUNCTL_SRC})
target_link_libraries(srunctl
  srun_proto
  ${PROTOBUF_LIBRARIES}
  ${Boost_LIBRARIES}
)

install(TARGETS srund srunctl
RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR}
)
