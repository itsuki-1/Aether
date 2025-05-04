cmake_minimum_required(VERSION 3.5)
project(d3d11_hook)
set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/minhook_debug_x64/lib/minhook) # Set minhook path
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(minhook)
include_directories(${CMAKE_CURRENT_LIST_DIR}/imgui_d11)
add_library(${PROJECT_NAME} SHARED dll_main.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/imgui_d11/imgui.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/imgui_d11/imgui_impl_dx11.cpp 
    ${CMAKE_CURRENT_LIST_DIR}/imgui_d11/imgui_impl_win32.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui_d11/imgui_draw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui_d11/imgui_tables.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui_d11/imgui_widgets.cpp
 "Menu/Menu.cpp" "Memory/Memory.h" "Offset/Offset.h" "Menu/Menu.h" "Menu/Menu.cpp" "Menu/Images/Image.h" "Menu/Images/Image.cpp")

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "Aether_Internal") 

target_link_libraries(${PROJECT_NAME} d3d11.lib minhook::minhook)