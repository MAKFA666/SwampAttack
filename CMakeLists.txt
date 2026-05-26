cmake_minimum_required(VERSION 3.10)
project(SwampAttack)

set(CMAKE_CXX_STANDARD 14)

# ВАЖНО: Эта команда генерирует файл compile_commands.json, 
# который скажет VS Code, где искать заголовочные файлы SDL2.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES Game.cpp)

if(EMSCRIPTEN)
    # --- НАСТРОЙКИ ДЛЯ ВЕБА (EMSCRIPTEN) ---
    add_executable(SwampAttack ${SOURCES})

    target_link_options(SwampAttack PRIVATE 
        "-sUSE_SDL=2" 
        "-sUSE_SDL_IMAGE=2" 
        "-sUSE_SDL_TTF=2"                # <-- ВОТ ЭТОТ ФЛАГ ОБЯЗАТЕЛЬНО ДОБАВЬ
        "-sSDL2_IMAGE_FORMATS=[\"png\"]" 
        "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    )
    
    message(STATUS "Configuring for WebAssembly with Emscripten!")
    
    message(STATUS "Configuring for WebAssembly with Emscripten!")

else()
    # --- НАСТРОЙКИ ДЛЯ ДЕСКТОПА (ЖЕСТКИЕ ПУТИ MinGW) ---
    add_executable(SwampAttack ${SOURCES})

    # Базовая папка, куда ты распаковал библиотеки
    set(SDL_WINGW_DIR "C:/SDL2")

    # 1. Явно говорим, где искать заголовочные файлы (.h)
    # Чтобы работал #include <SDL2/SDL.h>, путь должен вести строго в папочку include
    target_include_directories(SwampAttack PRIVATE "${SDL_WINGW_DIR}/include")

    # 2. Явно говорим, где искать файлы библиотек (.a)
    target_link_directories(SwampAttack PRIVATE "${SDL_WINGW_DIR}/lib")

    # 3. Линкуем библиотеки напрямую по именам файлов из папки lib
    # Порядок важен! Сначала расширения, потом сам SDL2 и SDL2main
    target_link_libraries(SwampAttack PRIVATE 
        mingw32 
        SDL2main 
        SDL2 
        SDL2_image 
        SDL2_ttf
    )
    
    message(STATUS "Configuring for Desktop with DIRECT MinGW paths!")
endif()