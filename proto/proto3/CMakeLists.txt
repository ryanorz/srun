set(PROTO_META_BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND PROTO_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB SRUN_PROTOS "*.proto")

foreach(EACH_PROTO ${SRUN_PROTOS})
  get_filename_component(EACH_NAME ${EACH_PROTO} NAME_WE)

  list(APPEND SRUN_PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${EACH_NAME}.pb.cc")
  list(APPEND SRUN_PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${EACH_NAME}.pb.h")

  execute_process(
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTO_FLAGS} --cpp_out=${PROTO_META_BASE_DIR} ${EACH_PROTO}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endforeach()

add_library(srun_proto STATIC ${SRUN_PROTO_SRCS} ${SRUN_PROTO_HDRS})
target_link_libraries(srun_proto ${PROTOBUF_LIBRARIES})
