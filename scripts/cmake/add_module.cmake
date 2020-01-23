function(add_module) # optional argument: list of dependencies
    get_filename_component(module_name ${CMAKE_CURRENT_LIST_DIR} NAME)

    file(GLOB HEADERS
        include/${module_name}/*.hpp
    )

    file(GLOB SOURCES
        *.cpp
    )

    if (SOURCES)
        add_library(${module_name} SHARED
            ${HEADERS}
            ${SOURCES}
        )

        target_include_directories(${module_name} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
        target_link_libraries(${module_name} PUBLIC ${ARGN})

        set_property(TARGET ${module_name} PROPERTY FOLDER Modules)
    else()
        add_library(${module_name} INTERFACE)

        target_include_directories(${module_name} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)
        target_link_libraries(${module_name} INTERFACE ${ARGN})
    endif()
endfunction()
