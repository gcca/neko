cmake_minimum_required(VERSION 3.16)
project(neko LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_library(neko_index_handlers STATIC src/handling/index/handlers.cpp)
target_link_libraries(neko_index_handlers PUBLIC Drogon::Drogon)

set(a_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/handling/a")
set(posts_css "${CMAKE_CURRENT_BINARY_DIR}/assets/posts.css")

find_program(NPM_EXECUTABLE npm)
if(NOT NPM_EXECUTABLE)
    message(FATAL_ERROR "npm is required to build the post list stylesheet")
endif()

add_custom_command(
    OUTPUT "${posts_css}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMAND "${NPM_EXECUTABLE}" run build:css -- --output "${posts_css}"
    DEPENDS package.json package-lock.json "${a_dir}/posts.css"
            "${a_dir}/templates/ALayout.csp" "${a_dir}/templates/APosts.csp"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
)
add_custom_target(neko_posts_css DEPENDS "${posts_css}")

# Keep the generated views' registration objects in both executables.
add_library(neko_a_handlers OBJECT "${a_dir}/handlers.cpp")
target_link_libraries(neko_a_handlers PUBLIC Drogon::Drogon "${neko_sqlite}")
target_include_directories(neko_a_handlers PRIVATE "${a_dir}")
target_compile_definitions(neko_a_handlers PRIVATE NEKO_POSTS_CSS_PATH="${posts_css}")
drogon_create_views(neko_a_handlers "${a_dir}/templates" "${CMAKE_CURRENT_BINARY_DIR}/views")

add_executable(neko src/neko.cc src/neko/core/conf.cpp)
target_link_libraries(neko PRIVATE
    neko_index_handlers neko_a_handlers absl::flags absl::flags_parse absl::flags_usage
)
add_dependencies(neko neko_posts_css)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(neko_tests src/handling/index/handlers-test.cc "${a_dir}/handlers-test.cc")
    target_link_libraries(neko_tests PRIVATE neko_index_handlers neko_a_handlers GTest::gmock_main)
    target_compile_definitions(neko_tests PRIVATE NEKO_SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/db/schema.sql")

    include(GoogleTest)
    gtest_discover_tests(neko_tests)
endif()
