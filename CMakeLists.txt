cmake_minimum_required(VERSION 3.24)
project(DiskLens VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
find_package(Threads REQUIRED)

add_library(disklens_core
    src/core/Scanner.cpp
    src/core/Duplicates.cpp
    src/core/TreemapLayout.cpp
)
target_include_directories(disklens_core PUBLIC src)
target_link_libraries(disklens_core PUBLIC Qt6::Core Threads::Threads)

add_library(disklens_ui
    src/ui/MainWindow.cpp
    src/ui/TreemapWidget.cpp
)
target_link_libraries(disklens_ui PUBLIC disklens_core Qt6::Widgets)
qt_add_executable(DiskLens src/main.cpp)
target_link_libraries(DiskLens PRIVATE disklens_ui)
set_target_properties(DiskLens PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

foreach(target disklens_core disklens_ui DiskLens)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(disklens_tests tests/CoreTests.cpp)
    target_link_libraries(disklens_tests PRIVATE disklens_core)
    add_test(NAME core COMMAND disklens_tests)
    add_executable(disklens_ui_tests tests/UiTests.cpp)
    target_link_libraries(disklens_ui_tests PRIVATE disklens_ui)
    add_test(NAME ui COMMAND disklens_ui_tests)
    set_tests_properties(ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
endif()

include(GNUInstallDirs)
install(TARGETS DiskLens
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES README.md LICENSE THIRD_PARTY.md DESTINATION .)
install(DIRECTORY licenses DESTINATION .)
install(DIRECTORY docs DESTINATION .)
qt_generate_deploy_app_script(TARGET DiskLens OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
