vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS FEATURES disable-logging
    DISABLE_LOGGING
)

set(PATCHES "vcpkg-support.patch" "fix-arm64-overflow.patch")
if(DISABLE_LOGGING)
    list(APPEND PATCHES "disable-logging.patch")
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH
    SOURCE_PATH
    REPO
    discord/libdave
    REF
    "${VERSION}"
    SHA512
    6284ba5842519eb3be64bd2dd98dacdd75443738ca1b5b5a0642d2b21576cc57b28b0e0cf517654efd2b22af7160d818f951404132069f5907c5dc528f735165
    PATCHES
    ${PATCHES}
)

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}/cpp")

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME libdave CONFIG_PATH lib/cmake/libdave)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
