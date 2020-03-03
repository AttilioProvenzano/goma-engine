set(app_name sample-app)

add_executable(${app_name}
    app_main.cpp
)
target_link_libraries(${app_name} PUBLIC engine)
set_property(TARGET ${app_name} PROPERTY FOLDER Apps)
