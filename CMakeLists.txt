cmake_minimum_required(VERSION 3.16)

project(
    "ekizu"
    VERSION 0.1.0
    LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
        FATAL_ERROR
            "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

if(NOT DEFINED ekizu_MASTER_PROJECT)
    set(ekizu_MASTER_PROJECT OFF)

    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(ekizu_MASTER_PROJECT ON)
        message(STATUS "CMake version: ${CMAKE_VERSION}")
    endif()
endif()

if(UNIX)
    add_compile_options("$<$<CONFIG:DEBUG>:-D_DEBUG>")
endif()

string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWERCASE)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPERCASE)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ekizu_BUILD_EXAMPLES "Build the examples" OFF)
option(ekizu_BUILD_TESTS "Build the tests" OFF)
option(ekizu_ENABLE_TESTING_HOOKS
       "Enable internal test-only hooks (EKIZU_TESTING)" OFF
)
option(ekizu_INSTALL "Generate the install target" ON)
option(ekizu_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
endif()

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "include/*.hpp" "src/*.cpp")

add_library(ekizu ${sources})
add_library(ekizu::ekizu ALIAS ekizu)

target_compile_definitions(
    ekizu PUBLIC $<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:EKIZU_STATIC_DEFINE>
)

target_compile_features(ekizu PUBLIC cxx_std_17)
target_compile_options(
    ekizu PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->"
                 "$<$<PLATFORM_ID:Windows>:/bigobj>"
)

target_include_directories(
    ekizu
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
           $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_definitions(
        ekizu
        PUBLIC -D_WIN32_WINNT=0x0601
               -D_SCL_SECURE_NO_WARNINGS=1
               -D_CRT_SECURE_NO_WARNINGS=1
               -D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
               -D_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING
    )
endif()

include(cmake/CompilerWarnings.cmake)

set_project_warnings(ekizu)

if(WIN32)
    target_compile_definitions(
        ekizu PUBLIC _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
endif()

find_package(Boost REQUIRED COMPONENTS asio charconv filesystem url)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
add_subdirectory(3rdparty/certify)
find_package(MLSPP CONFIG REQUIRED)
find_package(libdave CONFIG REQUIRED)

target_compile_definitions(ekizu PUBLIC BOOST_ASIO_NO_DEPRECATED)
target_link_libraries(
    ekizu
    PUBLIC fmt::fmt nlohmann_json::nlohmann_json certify::core
    PRIVATE Boost::charconv Boost::url ZLIB::ZLIB Opus::opus
            unofficial-sodium::sodium libdave::libdave
)

include(GenerateExportHeader)

generate_export_header(
    ekizu EXPORT_FILE_NAME
    ${CMAKE_CURRENT_BINARY_DIR}/generated/ekizu/export.hpp
)

if(ekizu_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(ekizu_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 3 REQUIRED)
    add_subdirectory(tests)
endif()

if(ekizu_ENABLE_TESTING_HOOKS)
    target_compile_definitions(ekizu PUBLIC EKIZU_TESTING)
endif()

if(ekizu_INSTALL)
    include(GNUInstallDirs)

    install(
        TARGETS ekizu core
        EXPORT ekizuTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        EXPORT ekizuTargets
        FILE ekizuTargets.cmake
        NAMESPACE ekizu::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ekizu
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/ekizu/version.hpp @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/ekizu/export.hpp
                  ${CMAKE_CURRENT_BINARY_DIR}/generated/ekizu/version.hpp
            DESTINATION include/ekizu
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ekizu
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
        ekizuConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ekizuConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ekizu
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ekizuConfig.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/ekizuConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ekizu
    )

    set(CPACK_PACKAGE_VENDOR jontitor)
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Discord library for C++")
    set(CPACK_PACKAGE_DESCRIPTION "A Discord library for C++")
    set(CPACK_PACKAGE_CONTACT "mail@jontitor.com")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "A Discord library for C++")

    if(WIN32)
        set(CPACK_GENERATOR ZIP)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CPACK_GENERATOR "DEB;RPM")
    else()
        set(CPACK_GENERATOR "TBZ2")
    endif()

    include(CPack)
endif()

# target_compile_options(ekizu PUBLIC -fsanitize=thread)
# target_link_options(ekizu PUBLIC -fsanitize=thread)
