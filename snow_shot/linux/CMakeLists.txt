cmake_minimum_required(VERSION 3.22)
project(snow_shot_linux VERSION 0.1.0 LANGUAGES CXX)

# Intentionally independent of the Windows/macOS workspace toolchain. This is
# an experimental Linux frontend, not a build of the complete upstream UI.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network LinguistTools)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_translations.py"
    COMMAND_ERROR_IS_FATAL ANY)
include(GNUInstallDirs)
include(CTest)

add_library(snow_linux_image STATIC src/image.cpp src/image.h)
target_include_directories(snow_linux_image PUBLIC src)
target_link_libraries(snow_linux_image PUBLIC Qt6::Widgets)
target_compile_options(snow_linux_image PRIVATE -Wall -Wextra -Wpedantic)
add_executable(snow-shot-linux src/main.cpp src/widgets.cpp src/widgets.h)
target_link_libraries(snow-shot-linux PRIVATE snow_linux_image Qt6::Network)
target_compile_options(snow-shot-linux PRIVATE -Wall -Wextra -Wpedantic)
qt_add_translations(snow-shot-linux TS_FILES i18n/snow_shot_linux_en_US.ts
    i18n/snow_shot_linux_zh_CN.ts i18n/snow_shot_linux_zh_TW.ts
    RESOURCE_PREFIX /i18n)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(snow-linux-image-tests tests/image_tests.cpp)
    target_link_libraries(snow-linux-image-tests PRIVATE snow_linux_image)
    add_test(NAME snow-linux-image-tests COMMAND snow-linux-image-tests -platform offscreen)
    add_executable(snow-linux-widget-tests tests/widget_tests.cpp src/widgets.cpp src/widgets.h)
    target_link_libraries(snow-linux-widget-tests PRIVATE snow_linux_image Qt6::Test)
    add_test(NAME snow-linux-widget-tests COMMAND snow-linux-widget-tests -platform offscreen)
endif()

install(TARGETS snow-shot-linux RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/io.github.pulipulichen.SnowShotLinux.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/io.github.pulipulichen.SnowShotLinux.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ../LICENSE ../COPYRIGHT README.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/snow-shot-linux)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME snow-shot-linux)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "https://github.com/pulipulichen/snow-apps/issues")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Experimental Snow Shot Linux screenshot frontend (X11)")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Snow Shot Linux fork maintainers")
set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6svg6, qt6-qpa-plugins")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)
