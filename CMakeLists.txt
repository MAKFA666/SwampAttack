cmake_minimum_required(VERSION 3.10)
project(SwampAttack)

set(CMAKE_CXX_STANDARD 14)

# Указываем Game.cpp, так как ваш файл называется именно так
set(SOURCES Game.cpp)

if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    add_executable(SwampAttack ${SOURCES})
    target_link_options(SwampAttack PRIVATE
        "-s" "USE_SFML=2"
        "-s" "USE_FREETYPE=1"
        "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@/"
    )
else()
    # Find SFML
    set(SFML_DIR "C:/SFML-2.6.2/lib/cmake/SFML")
    find_package(SFML 2.6 REQUIRED COMPONENTS graphics window system)

    # Use the ${SOURCES} variable instead of hardcoding Game.cpp again
    add_executable(SwampAttack ${SOURCES})

    # Modern CMake approach for SFML: Link the targets directly.
    # This automatically includes the necessary directories, so you don't need target_include_directories.
    target_link_libraries(SwampAttack PRIVATE sfml-graphics sfml-window sfml-system)
endif()