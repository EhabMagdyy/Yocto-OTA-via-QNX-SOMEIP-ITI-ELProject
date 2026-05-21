######################### Yocto Client Build #########################
cmake_minimum_required(VERSION 3.10)
project(helloClient CXX)
set(CMAKE_CXX_STANDARD 17)

# Use find_package with sysroot cmake dirs (passed via -D at cmake time)
find_package(CommonAPI REQUIRED)
find_package(CommonAPI-SomeIP REQUIRED)
find_package(vsomeip3 REQUIRED)

set(GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPDeployment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen/v1/commonapi/HelloSomeIPStubAdapter.cpp
)

add_executable(helloClient
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Client.cpp
    ${GENERATED_SOURCES}
)

target_include_directories(helloClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fidl/src-gen
)

target_link_libraries(helloClient
    vsomeip3
    CommonAPI
    CommonAPI-SomeIP
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