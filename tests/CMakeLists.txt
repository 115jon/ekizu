set(test_app_name ekizu_tests)

file(GLOB test_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_support/*.hpp
)

find_package(Boost REQUIRED COMPONENTS coroutine)
add_executable(${test_app_name} ${test_sources})
target_include_directories(${test_app_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    ${test_app_name} PRIVATE Catch2::Catch2WithMain ekizu::ekizu
                             Boost::coroutine
)
if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    target_compile_features(${test_app_name} PRIVATE cxx_std_20)
endif()

include(Catch)

# Normal tests: exclude canaries
catch_discover_tests(
    ${test_app_name} TEST_PREFIX "ekizu." TEST_SPEC "~[stress]"
)

add_test(NAME ekizu.stress COMMAND ${test_app_name} "[stress]")
set_tests_properties(ekizu.stress PROPERTIES LABELS "stress" TIMEOUT 30)

option(EKIZU_ENABLE_ASAN_UBSAN "Enable ASan+UBSan" OFF)
option(EKIZU_ENABLE_TSAN "Enable TSan" OFF)
option(EKIZU_ENABLE_LLVM_COVERAGE "Enable llvm-cov instrumentation" OFF)

function(ekizu_enable_sanitizers target)
    # Only apply in Debug config (portable across generators).
    set(_dbg $<CONFIG:Debug>)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        if(EKIZU_ENABLE_ASAN_UBSAN)
            target_compile_options(
                ${target} PRIVATE $<${_dbg}:-fsanitize=address,undefined>
            )
            target_link_options(
                ${target} PRIVATE $<${_dbg}:-fsanitize=address,undefined>
            )
        endif()

        if(EKIZU_ENABLE_TSAN)
            target_compile_options(
                ${target} PRIVATE $<${_dbg}:-fsanitize=thread>
            )
            target_link_options(${target} PRIVATE $<${_dbg}:-fsanitize=thread>)
        endif()

        if(EKIZU_ENABLE_LLVM_COVERAGE)
            # Usually only meaningful for clang; left as-is but Debug-only.
            target_compile_options(
                ${target} PRIVATE $<${_dbg}:-fprofile-instr-generate
                                  -fcoverage-mapping>
            )
            target_link_options(
                ${target} PRIVATE $<${_dbg}:-fprofile-instr-generate
                -fcoverage-mapping>
            )
        endif()

        # Common sanitizer runtime requirement; Debug-only and does not set
        # -O/-g.
        if(EKIZU_ENABLE_ASAN_UBSAN OR EKIZU_ENABLE_TSAN)
            target_compile_options(
                ${target} PRIVATE $<${_dbg}:-fno-omit-frame-pointer>
            )
        endif()

    elseif(MSVC)
        if(EKIZU_ENABLE_ASAN_UBSAN)
            target_compile_options(
                ${target} PRIVATE $<${_dbg}:/fsanitize=address>
            )
            # ASan is incompatible with incremental linking; apply only in
            # Debug.
            target_link_options(${target} PRIVATE $<${_dbg}:/INCREMENTAL:NO>)
        endif()

        if(EKIZU_ENABLE_TSAN)
            message(
                WARNING
                    "EKIZU_ENABLE_TSAN is not supported with MSVC (/fsanitize=thread is not available)."
            )
        endif()

        if(EKIZU_ENABLE_LLVM_COVERAGE)
            message(
                WARNING
                    "EKIZU_ENABLE_LLVM_COVERAGE is clang-only on Windows in this helper."
            )
        endif()
    endif()
endfunction()

ekizu_enable_sanitizers(ekizu_tests)
ekizu_enable_sanitizers(ekizu)
