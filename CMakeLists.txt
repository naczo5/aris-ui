cmake_minimum_required(VERSION 3.20)
project(aris-ui VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ARIS_UI_BUILD_EXAMPLES "Build example applications (desktop runner)" ON)
option(ARIS_UI_BUILD_TOOLS "Build tools (MSDF atlas generator)" ON)
option(ARIS_UI_BUILD_TESTS "Build the zero-dependency test suite" ON)

# ---------------------------------------------------------------------------
# Third-party
# ---------------------------------------------------------------------------
# Yoga (vendored, pinned v3.2.1) — builds `yogacore` static library
add_subdirectory(third_party/yoga/yoga)

# glad (generated GL core 3.3 loader)
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# stb — header-only (implementations instantiated in aris-ui sources)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE third_party/stb)

# nlohmann/json — single header
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE third_party)

# ---------------------------------------------------------------------------
# Tools & Atlas Generator (FreeType + vendored msdfgen)
# ---------------------------------------------------------------------------
if (ARIS_UI_BUILD_TOOLS)
    set(MSDFGEN_USE_VCPKG        OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_USE_SKIA         OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_USE_OPENMP       OFF CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_PNG      ON  CACHE BOOL "" FORCE)
    set(MSDFGEN_DISABLE_SVG      ON  CACHE BOOL "" FORCE)
    set(MSDFGEN_INSTALL          OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/msdfgen)

    find_package(Freetype REQUIRED)

    add_library(aris-atlasgen STATIC
        tools/atlas_generator/atlas_gen.cpp
    )
    target_include_directories(aris-atlasgen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tools)
    target_link_libraries(aris-atlasgen
        PUBLIC  msdfgen::msdfgen-core msdfgen::msdfgen-ext
        PRIVATE stb
    )
    target_compile_definitions(aris-atlasgen PRIVATE STB_IMAGE_WRITE_IMPLEMENTATION)

    add_executable(atlasgen tools/atlas_generator/main.cpp)
    target_link_libraries(atlasgen PRIVATE aris-atlasgen)
endif()

# ---------------------------------------------------------------------------
# aris-ui core library (core engine + layout + OpenGL backend)
# ---------------------------------------------------------------------------
add_library(aris-ui STATIC
    src/core/context.cpp
    src/core/theme.cpp
    src/renderer/command_queue.cpp
    src/renderer/emit_draw.cpp
    src/renderer/batcher.cpp
    src/renderer/gl_state_guard.cpp
    src/renderer/vulkan_renderer.cpp
    src/font/font_atlas.cpp
    src/widgets/widgets.cpp
    src/widgets/icons.cpp
    src/widgets/kit.cpp
)
target_include_directories(aris-ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan/include
)
target_include_directories(aris-ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer)
target_link_libraries(aris-ui
    PUBLIC  yogacore
    PRIVATE glad stb nlohmann_json
)
target_compile_definitions(aris-ui PRIVATE
    STB_IMAGE_IMPLEMENTATION
)
if (ARIS_UI_BUILD_TOOLS)
    target_include_directories(aris-ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
    target_link_libraries(aris-ui PRIVATE aris-atlasgen)
    target_compile_definitions(aris-ui PRIVATE ARIS_UI_HAS_ATLASGEN=1)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if (ARIS_UI_BUILD_EXAMPLES)
    find_package(glfw3 QUIET)
    if (NOT glfw3_FOUND)
        include(FetchContent)
        set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.4
        )
        FetchContent_MakeAvailable(glfw)
    endif()

    add_executable(aris_runner examples/desktop_runner/main.cpp)
    target_include_directories(aris_runner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples)
    target_link_libraries(aris_runner PRIVATE aris-ui aris-atlasgen glfw glad stb)

    # Headless UI snapshot tool — renders named scenes to PNGs (agent-friendly
    # visual verification; no visible window required).
    add_executable(aris_scene_preview examples/scene_preview/main.cpp)
    target_include_directories(aris_scene_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples)
    target_link_libraries(aris_scene_preview PRIVATE aris-ui aris-atlasgen glfw glad stb)
endif()

# ---------------------------------------------------------------------------
# Tests (zero-dependency assert harness, no GL required)
# ---------------------------------------------------------------------------
if (ARIS_UI_BUILD_TESTS)
    enable_testing()
    add_executable(aris_tests
        tests/test_main.cpp
        tests/test_math.cpp
        tests/test_easing.cpp
        tests/test_id.cpp
        tests/test_command_queue.cpp
        tests/test_widget_state.cpp
        tests/test_layout.cpp
    )
    target_include_directories(aris_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(aris_tests PRIVATE aris-ui)
    add_test(NAME aris_tests COMMAND aris_tests)
endif()