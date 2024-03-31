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

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ekizu_INSTALL "Generate the install target" ON)
option(ekizu_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

include(CMakeDependentOption)

cmake_dependent_option(
    ekizu_BUILD_EXAMPLES "Build the examples" ON "ekizu_MASTER_PROJECT" OFF
)

if(BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
endif()

include(cmake/CPM.cmake)

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
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/export>
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

set(TRY_BOOST_VERSION "1.81.0")
set(BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED
    "coroutine;filesystem;thread;url"
)
set(BOOST_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED "asio;beast;outcome")

set(IS_BOOST_LOCAL OFF)
if(${CPM_LOCAL_PACKAGES_ONLY})
    message(STATUS "Trying to find Boost...")
    find_package(
        Boost ${TRY_BOOST_VERSION} REQUIRED
        COMPONENTS ${BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED}
    )
    set(IS_BOOST_LOCAL ON)
elseif(${CPM_USE_LOCAL_PACKAGES} OR NOT ${CPM_DOWNLOAD_ALL})
    message(STATUS "Trying to use local Boost...")
    find_package(
        Boost ${TRY_BOOST_VERSION}
        COMPONENTS ${BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED}
    )
    if(${BOOST_FOUND})
        set(IS_BOOST_LOCAL ON)
        message(DEBUG "boost include dir: ${Boost_INCLUDE_DIRS}")
    endif()
endif()

if(NOT (${BOOST_FOUND}) OR (NOT DEFINED BOOST_FOUND))
    message(STATUS "Trying to download Boost...")
    set(BOOST_INCLUDE_LIBRARIES
        "${BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED};${BOOST_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED}"
    )
    set(BOOST_SKIP_INSTALL_RULES OFF)
    cpmaddpackage(
        NAME
        Boost
        URL
        "https://github.com/jontitorr/boost-cmake/releases/download/1.84.0-release/boost-1.84.0.tar.xz"
    )
    set(IS_BOOST_LOCAL OFF)
endif()

cpmfindpackage(
    NAME
    "OpenSSL"
    GITHUB_REPOSITORY
    "jontitorr/openssl-cmake"
    GIT_TAG
    "master"
    OPTIONS
    "WITH_APPS OFF"
)

if(OpenSSL_ADDED)
    add_library(OpenSSL::Crypto ALIAS crypto)
    add_library(OpenSSL::SSL ALIAS ssl)
    install(TARGETS crypto ssl EXPORT ekizuTargets)
else()
    set(SEPARATE_DEPENDENCIES "OpenSSL;SSL;")
endif()

cpmfindpackage(NAME "fmt" GITHUB_REPOSITORY "fmtlib/fmt" GIT_TAG "75f3b1c")
set(SEPARATE_DEPENDENCIES "${SEPARATE_DEPENDENCIES}fmt;;")

if(WIN32)
    target_compile_definitions(
        ekizu PUBLIC _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
endif()

cpmaddpackage(
    NAME certify GITHUB_REPOSITORY "jontitorr/certify" GIT_TAG "master"
)

cpmaddpackage(NAME Opus GITHUB_REPOSITORY "xiph/opus" GIT_TAG "main")
set(SEPARATE_DEPENDENCIES "${SEPARATE_DEPENDENCIES}Opus;;")

cpmaddpackage(
    NAME
    Sodium
    GITHUB_REPOSITORY
    "jontitorr/libsodium-cmake"
    GIT_TAG
    "master"
    OPTIONS
    "SODIUM_DISABLE_TESTS ON"
    "SODIUM_MINIMAL ON"
)

install(TARGETS sodium EXPORT ekizuTargets)

set(SEPARATE_DEPENDENCIES
    "${SEPARATE_DEPENDENCIES}Threads;;Boost;coroutine,date_time,filesystem,system,thread,url"
)

if(NOT ${IS_BOOST_LOCAL})
    add_library(boost INTERFACE)
    target_link_libraries(
        boost INTERFACE $<BUILD_INTERFACE:Boost::asio Boost::beast
                        Boost::outcome> $<INSTALL_INTERFACE:Boost::headers>
    )
    add_library(Boost::boost ALIAS boost)
    install(TARGETS boost EXPORT ekizuTargets)
endif()

cpmaddpackage(
    NAME
    "nlohmann_json"
    VERSION
    "3.11.1"
    URL
    "https://github.com/nlohmann/json/releases/download/v3.11.1/include.zip"
    URL_HASH
    "SHA256=9c15ca7806f863872452bfbc85fee6d1c9868117e3ea5e00a204ae961a2e1ae7"
)

add_library(nlohmann_json INTERFACE)
target_include_directories(
    nlohmann_json
    INTERFACE $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
              $<INSTALL_INTERFACE:include>
)

install(TARGETS core nlohmann_json EXPORT ekizuTargets)

find_package(ZLIB)

if(ZLIB_FOUND)
    set(SEPARATE_DEPENDENCIES "${SEPARATE_DEPENDENCIES}ZLIB;;")
else()
    cpmaddpackage(
        NAME
        "ZLIB"
        GITHUB_REPOSITORY
        "zlib-ng/zlib-ng"
        GIT_TAG
        "2.0.6"
        OPTIONS
        "ZLIB_COMPAT ON"
        "ZLIB_ENABLE_TESTS OFF"
        "SKIP_INSTALL_ALL ON"
    )

    add_library(ZLIB::ZLIB ALIAS zlib)
    list(APPEND ekizu_TARGETS zlib)
    install(TARGETS zlib EXPORT ekizuTargets)
endif()

target_link_libraries(
    ekizu
    PUBLIC Boost::boost certify::core Boost::coroutine Boost::thread fmt::fmt
           nlohmann_json
    PRIVATE Boost::url ZLIB::ZLIB opus sodium
)

include(GenerateExportHeader)

generate_export_header(ekizu EXPORT_FILE_NAME export/ekizu/export.h)

if(ekizu_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(ekizu_INSTALL)
    include(GNUInstallDirs)

    install(
        TARGETS ekizu
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

    install(DIRECTORY ${nlohmann_json_SOURCE_DIR}/include/nlohmann
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in include/ekizu/version.h
        @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/export/ekizu/export.h
                  ${CMAKE_CURRENT_BINARY_DIR}/include/ekizu/version.h
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
