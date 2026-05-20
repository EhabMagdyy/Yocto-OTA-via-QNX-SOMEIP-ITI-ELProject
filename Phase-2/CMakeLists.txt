######################### QNX Client Build #########################
cmake_minimum_required(VERSION 3.10)
project(helloClient CXX C)
set(CMAKE_CXX_STANDARD 17)

# Stage Directory (all libraries and headers are here)
set(STAGE_DIR "/home/ehab/qnxCTI/src/stage/usr")

# Include Paths
set(COMMONAPI_INCLUDE_DIR        ${STAGE_DIR}/include/CommonAPI-3.2)
set(COMMONAPI_SOMEIP_INCLUDE_DIR ${STAGE_DIR}/include/CommonAPI-SomeIP-3.2)
set(VSOMEIP_INCLUDE_DIR          ${STAGE_DIR}/include)

# Library Paths
set(LIB_DIR ${STAGE_DIR}/lib)

set(COMMONAPI_LIB        ${LIB_DIR}/libCommonAPI.so)
set(COMMONAPI_SOMEIP_LIB ${LIB_DIR}/libCommonAPI-SomeIP.so)
set(VSOMEIP_LIB          ${LIB_DIR}/libvsomeip3.so)

set(BOOST_THREAD_LIB  ${LIB_DIR}/libboost_thread.so)
set(BOOST_SYSTEM_LIB  ${LIB_DIR}/libboost_system.so)

# Validate that libraries exist before proceeding
foreach(LIB ${COMMONAPI_LIB} ${COMMONAPI_SOMEIP_LIB} ${VSOMEIP_LIB})
    if(NOT EXISTS ${LIB})
        message(FATAL_ERROR "Library not found: ${LIB}")
    endif()
endforeach()

message(STATUS "CommonAPI lib:        ${COMMONAPI_LIB}")
message(STATUS "CommonAPI-SomeIP lib: ${COMMONAPI_SOMEIP_LIB}")
message(STATUS "vsomeip3 lib:         ${VSOMEIP_LIB}")

# Generated SOME/IP Sources
set(GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPDeployment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPStubAdapter.cpp
)

# Executable
add_executable(helloClient
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.cpp
    ${GENERATED_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eventfd_stub.c
)

target_compile_definitions(helloClient PRIVATE
    _QNX_SOURCE
    SA_RESTART=0
    __unix__
    __LITTLE_ENDIAN=1234
    __BIG_ENDIAN=4321
    __BYTE_ORDER=1234
)

target_include_directories(helloClient PRIVATE
    ${COMMONAPI_INCLUDE_DIR}
    ${COMMONAPI_SOMEIP_INCLUDE_DIR}
    ${VSOMEIP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen
)

target_link_libraries(helloClient PRIVATE
    ${COMMONAPI_LIB}
    ${COMMONAPI_SOMEIP_LIB}
    ${VSOMEIP_LIB}
    ${BOOST_THREAD_LIB}
    ${BOOST_SYSTEM_LIB}
    -L${LIB_DIR}
    -Wl,-rpath-link,${LIB_DIR}
)

######################### Host Server Build #########################
# project(helloServer)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # Find packages (Linux system)
# find_package(CommonAPI REQUIRED)
# find_package(CommonAPI-SomeIP REQUIRED)
# find_package(vsomeip3 REQUIRED)

# message(STATUS "CommonAPI found: ${CommonAPI_FOUND}")
# message(STATUS "CommonAPI-SomeIP found: ${CommonAPI-SomeIP_FOUND}")
# message(STATUS "vsomeip3 found: ${vsomeip3_FOUND}")

# include_directories(
#     ${CMAKE_CURRENT_SOURCE_DIR}/src
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen
# )

# set(GENERATED_SOMEIP_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPDeployment.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPProxy.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPStubAdapter.cpp
# )

# # Server only (Lab PC)
# add_executable(helloServer
#     src/Server.cpp
#     ${GENERATED_SOMEIP_SOURCES}
# )

# target_link_libraries(helloServer
#     PUBLIC
#     CommonAPI
#     CommonAPI-SomeIP
#     vsomeip3
# )

# # For testing: Client on same PC
# add_executable(helloClient
#     src/Client.cpp
#     ${GENERATED_SOMEIP_SOURCES}
# )

# target_link_libraries(helloClient
#     CommonAPI
#     CommonAPI-SomeIP
#     vsomeip3
# )