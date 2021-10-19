cmake_minimum_required(VERSION 3.9)
cmake_policy(VERSION 3.9)

project("route_diag")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-pie")
set(CMAKE_EXE_LINKER_FLAGS "-no-pie")

add_executable(route_diag src/main.cpp)
target_link_libraries(route_diag
  libvpr
  )

#Supress IPO link warnings if IPO is enabled
get_target_property(TEST_ROUTE_DIAG_USES_IPO route_diag INTERPROCEDURAL_OPTIMIZATION)
if (TEST_ROUTE_DIAG_USES_IPO)
    set_property(TARGET route_diag APPEND PROPERTY LINK_FLAGS ${IPO_LINK_WARN_SUPRESS_FLAGS})
endif()

