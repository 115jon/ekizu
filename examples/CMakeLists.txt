project(${CMAKE_PROJECT_NAME}_examples LANGUAGES C CXX)

file(GLOB_RECURSE examples "*.cpp")

find_package(Boost REQUIRED COMPONENTS coroutine2)
find_package(portaudio REQUIRED)

set(portaudio_target "")
if(TARGET portaudio)
    set(portaudio_target portaudio)
elseif(TARGET portaudio_static)
    set(portaudio_target portaudio_static)
else()
    message(
        FATAL_ERROR
            "Neither 'portaudio' nor 'portaudio_static' CMake target exists"
    )
endif()

find_package(spdlog REQUIRED)

foreach(example ${examples})
    get_filename_component(example_name ${example} NAME_WE)
    add_executable(${example_name} ${example})
    target_link_libraries(
        ${example_name} PRIVATE ${CMAKE_PROJECT_NAME} ${portaudio_target}
                                spdlog::spdlog Boost::coroutine2
    )
endforeach()

file(COPY airhorn.dca DESTINATION ${CMAKE_BINARY_DIR}/bin)
