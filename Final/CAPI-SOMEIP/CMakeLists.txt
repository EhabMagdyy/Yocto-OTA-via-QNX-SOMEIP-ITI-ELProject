######################### QNX Build #########################
cmake_minimum_required(VERSION 3.10)
project(qnxService CXX C)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/OtaSomeIPDeployment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/OtaSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/OtaSomeIPStubAdapter.cpp
)

# Executable
add_executable(qnxService
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qnx.cpp
    ${GENERATED_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/eventfd_stub.c
)

target_compile_definitions(qnxService PRIVATE
    _QNX_SOURCE
    SA_RESTART=0
    __unix__
    __LITTLE_ENDIAN=1234
    __BIG_ENDIAN=4321
    __BYTE_ORDER=1234
)

target_include_directories(qnxService PRIVATE
    ${COMMONAPI_INCLUDE_DIR}
    ${COMMONAPI_SOMEIP_INCLUDE_DIR}
    ${VSOMEIP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen
)

target_link_libraries(qnxService PRIVATE
    ${COMMONAPI_LIB}
    ${COMMONAPI_SOMEIP_LIB}
    ${VSOMEIP_LIB}
    ${BOOST_THREAD_LIB}
    ${BOOST_SYSTEM_LIB}
    -L${LIB_DIR}
    -Wl,-rpath-link,${LIB_DIR}
)

######################### Yocto Build #########################
# cmake_minimum_required(VERSION 3.10)
# project(yoctoService CXX)
# set(CMAKE_CXX_STANDARD 17)

# # Use find_package with sysroot cmake dirs (passed via -D at cmake time)
# find_package(CommonAPI REQUIRED)
# find_package(CommonAPI-SomeIP REQUIRED)
# find_package(vsomeip3 REQUIRED)

# set(GENERATED_SOURCES
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/OtaSomeIPDeployment.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/OtaSomeIPProxy.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/OtaSomeIPStubAdapter.cpp
# )

# add_executable(yoctoService
#     ${CMAKE_CURRENT_SOURCE_DIR}/src/yocto.cpp
#     ${GENERATED_SOURCES}
# )

# target_include_directories(yoctoService PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen
# )

# target_link_libraries(yoctoService
#     vsomeip3
#     CommonAPI
#     CommonAPI-SomeIP
# )
