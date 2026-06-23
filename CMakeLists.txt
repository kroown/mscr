cmake_minimum_required(VERSION 3.20)
project(metadata-scraper VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

set(SOURCES
    src/main.cpp
    src/scanner.cpp
    src/image.cpp
    src/text.cpp
    src/json.cpp
    src/strip.cpp
)

set(HEADERS
    src/scanner.hpp
    src/image.hpp
    src/text.hpp
    src/json.hpp
    src/strip.hpp
)

add_executable(mscr ${SOURCES} ${HEADERS})
target_include_directories(mscr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS mscr RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
