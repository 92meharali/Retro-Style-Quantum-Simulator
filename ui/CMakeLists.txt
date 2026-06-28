find_package(OpenGL)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui-1.90.4)

add_executable(quantum-lab
    app.cpp
    util.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(quantum-lab PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(quantum-lab PRIVATE quantum-core glfw)

if(WIN32)
    target_link_libraries(quantum-lab PRIVATE opengl32 comdlg32 shell32)
    if(MINGW)
        target_link_options(quantum-lab PRIVATE -static-libgcc -static-libstdc++)
    endif()
else()
    target_link_libraries(quantum-lab PRIVATE OpenGL::GL)
endif()

target_compile_definitions(quantum-lab PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

add_custom_command(TARGET quantum-lab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/presets
        $<TARGET_FILE_DIR:quantum-lab>/presets
)
