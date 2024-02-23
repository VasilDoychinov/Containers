cmake_minimum_required(VERSION 3.5)

project(qBTS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

set(MY_LOGGER_DIR "~/src/Logger")

add_executable(qBTS test_qb.cpp ${MY_LOGGER_DIR}/lib/Logger_impl.cpp
                    src/queue_bts.hpp src/queue_bts.cpp
              )

target_include_directories(qBTS PUBLIC ${MY_LOGGER_DIR}/include)
target_link_libraries(qBTS PUBLIC pthread)
