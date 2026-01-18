vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    cisco/mlspp
    REF
    "${VERSION}"
    SHA512
    5d37631e2c47daae1133ef074e60cc09ca2d395f9e11c416f829060e374051cf219d2d7fe98dae49d1d045292e07d6a09f4814a5f16e6cc05e67e7cd96f146c4
    PATCHES
    fix-windows-internal-libs-static.patch
)

# Platform-specific compiler flags
if(VCPKG_TARGET_IS_WINDOWS)
    # MSVC: /EHsc enables exception handling semantics (fixes C4530 warning)
    set(EXTRA_CXX_FLAGS "/EHsc")
elseif(
    VCPKG_TARGET_IS_LINUX
    OR VCPKG_TARGET_IS_OSX
    OR VCPKG_TARGET_IS_FREEBSD
)
    # GCC 15+ has false positive maybe-uninitialized warnings with std::variant
    set(EXTRA_CXX_FLAGS
        "-Wno-error=maybe-uninitialized -Wno-unknown-warning-option"
    )
else()
    set(EXTRA_CXX_FLAGS "")
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}" OPTIONS -DDISABLE_GREASE=ON
    -DMLS_CXX_NAMESPACE="mlspp"
    "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} ${EXTRA_CXX_FLAGS}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
