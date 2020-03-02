add_library(imgui
    imgui_demo.cpp imgui_draw.cpp imgui_impl_glfw.cpp imgui_widgets.cpp imgui.cpp)
target_include_directories(imgui PUBLIC include)
target_link_libraries(imgui PUBLIC glfw)
