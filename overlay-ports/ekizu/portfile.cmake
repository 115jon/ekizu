# ekizu vcpkg port - builds ekizu from the local source directory

# Use the source from the parent directory (when used as overlay port)
set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../..")

# Map features to CMake options INVERTED_FEATURES: when feature is ABSENT, the
# CMake option is ON
vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS INVERTED_FEATURES boost
    EKIZU_USE_SYSTEM_BOOST
)

vcpkg_cmake_configure(
    SOURCE_PATH
    "${SOURCE_PATH}"
    OPTIONS
    -Dekizu_BUILD_EXAMPLES=OFF
    -Dekizu_BUILD_TESTS=OFF
    -Dekizu_INSTALL=ON
    ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ekizu CONFIG_PATH lib/cmake/ekizu)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# Install license
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
