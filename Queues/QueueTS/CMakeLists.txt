cmake_minimum_required(VERSION 3.5)

project(QueueTS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)


set(MY_LOGGER_DIR "~/src/Logger")

add_executable(QueueTS test_qts.cpp ${MY_LOGGER_DIR}/lib/Logger_impl.cpp
   src/queue_tsfg.hpp
   src/queue_tsfg.cpp)

target_include_directories(QueueTS PUBLIC ${MY_LOGGER_DIR}/include)
target_link_libraries(QueueTS PUBLIC pthread)

include(GNUInstallDirs)
install(TARGETS QueueTS
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
