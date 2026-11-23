cmake_minimum_required(VERSION 3.16)
project(neko LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(NEKO_COVERAGE "Enable Clang source coverage for the handler tests" OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(absl CONFIG QUIET)

if(NOT absl_FOUND)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(absl
        URL https://codeload.github.com/abseil/abseil-cpp/tar.gz/refs/tags/20260817.0
        URL_HASH SHA256=f7e05179df39c45434cad433f5783840bb3788ef322976f9138bc6b72b3a107d
    )
    FetchContent_MakeAvailable(absl)
endif()

if(TARGET SQLite3_lib)
    set(neko_sqlite SQLite3_lib)
elseif(TARGET SQLite3::SQLite3)
    set(neko_sqlite SQLite3::SQLite3)
else()
    set(neko_sqlite SQLite::SQLite3)
endif()

add_library(neko_core STATIC src/neko/core/conf.cpp)
target_include_directories(neko_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(index_handler_source "${CMAKE_CURRENT_SOURCE_DIR}/src/handling/index/handlers.cpp")
# Link the index object directly so its automatic controller registration runs.
add_library(neko_index_handlers OBJECT "${index_handler_source}")
target_link_libraries(neko_index_handlers PUBLIC Drogon::Drogon)

set(stories_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/handling/stories")
set(stories_handler_source "${stories_dir}/handlers.cpp")
set(stories_css "${CMAKE_CURRENT_BINARY_DIR}/assets/stories.css")

find_program(NPM_EXECUTABLE npm)
if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "npm is required to build the story list stylesheet")
endif()

add_custom_command(
    OUTPUT "${stories_css}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMAND "${NPM_EXECUTABLE}" run build:css -- --output "${stories_css}"
    DEPENDS package.json package-lock.json "${stories_dir}/stories.css"
            "${stories_dir}/templates/StoriesLayout.csp" "${stories_dir}/templates/StoriesList.csp"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(neko_stories_css DEPENDS "${stories_css}")

# Keep the generated views' registration objects in both executables.
add_library(neko_stories_handlers OBJECT "${stories_handler_source}")
target_link_libraries(neko_stories_handlers
    PUBLIC Drogon::Drogon "${neko_sqlite}"
    PRIVATE neko_core
)
target_include_directories(neko_stories_handlers PRIVATE "${stories_dir}")
target_compile_definitions(neko_stories_handlers PRIVATE NEKO_STORIES_CSS_PATH="${stories_css}")
drogon_create_views(neko_stories_handlers "${stories_dir}/templates" "${CMAKE_CURRENT_BINARY_DIR}/views")

add_executable(neko src/neko.cc)
target_link_libraries(neko PRIVATE
    neko_index_handlers neko_stories_handlers neko_core absl::flags absl::flags_parse absl::flags_usage
)
add_dependencies(neko neko_stories_css)

include(CTest)
if(NEKO_COVERAGE AND NOT BUILD_TESTING)
    message(FATAL_ERROR "NEKO_COVERAGE requires BUILD_TESTING=ON")
endif()

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(neko_tests src/handling/index/handlers-test.cc "${stories_dir}/handlers-test.cc")
    target_link_libraries(neko_tests PRIVATE neko_index_handlers neko_stories_handlers neko_core GTest::gmock_main)
    target_compile_definitions(neko_tests PRIVATE NEKO_SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/db/schema.sql")

    include(GoogleTest)
    gtest_discover_tests(neko_tests)
endif()

if(NEKO_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang)$")
        message(FATAL_ERROR "NEKO_COVERAGE requires Clang or AppleClang")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        find_program(NEKO_XCRUN xcrun)
    endif()
    get_filename_component(neko_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    string(REGEX MATCH "^[0-9]+" neko_llvm_major "${CMAKE_CXX_COMPILER_VERSION}")
    foreach(tool IN ITEMS llvm-profdata llvm-cov)
        string(REPLACE "-" "_" tool_var "NEKO_${tool}")
        string(TOUPPER "${tool_var}" tool_var)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NEKO_XCRUN AND NOT ${tool_var})
            execute_process(
                COMMAND "${NEKO_XCRUN}" --find "${tool}"
                RESULT_VARIABLE neko_xcrun_result
                OUTPUT_VARIABLE neko_tool_path
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(neko_xcrun_result STREQUAL "0")
                set(${tool_var} "${neko_tool_path}")
            endif()
        endif()
        find_program(${tool_var}
            NAMES "${tool}-${neko_llvm_major}" "${tool}"
            HINTS "${neko_compiler_dir}"
        )
        if(NOT ${tool_var} OR NOT EXISTS "${${tool_var}}")
            message(FATAL_ERROR "NEKO_COVERAGE requires ${tool} from the selected Clang toolchain. Set ${tool_var} to its path.")
        endif()
    endforeach()

    foreach(handler IN ITEMS neko_index_handlers neko_stories_handlers)
        target_compile_options(${handler} PRIVATE
            "-fprofile-instr-generate=${CMAKE_CURRENT_BINARY_DIR}/neko-default.profraw"
            -fcoverage-mapping
        )
    endforeach()
    target_link_options(neko PRIVATE -fprofile-instr-generate)
    target_link_options(neko_tests PRIVATE -fprofile-instr-generate)

    set(neko_coverage_dir "${CMAKE_CURRENT_BINARY_DIR}/coverage")
    set(neko_coverage_profile "${neko_coverage_dir}/neko.profdata")
    add_custom_target(coverage
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${neko_coverage_dir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${neko_coverage_dir}/raw"
        COMMAND "${CMAKE_COMMAND}" -E env
            "LLVM_PROFILE_FILE=${neko_coverage_dir}/raw/%p.profraw"
            "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" --output-on-failure
        COMMAND "${NEKO_LLVM_PROFDATA}" merge -sparse "${neko_coverage_dir}/raw"
            -o "${neko_coverage_profile}"
        COMMAND "${NEKO_LLVM_COV}" report "$<TARGET_FILE:neko_tests>"
            "-instr-profile=${neko_coverage_profile}"
            "${index_handler_source}" "${stories_handler_source}"
        COMMAND "${NEKO_LLVM_COV}" show "$<TARGET_FILE:neko_tests>"
            "-instr-profile=${neko_coverage_profile}"
            -format=html -show-branches=count "-output-dir=${neko_coverage_dir}/html"
            "${index_handler_source}" "${stories_handler_source}"
        DEPENDS neko_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        USES_TERMINAL
        VERBATIM
    )
endif()
