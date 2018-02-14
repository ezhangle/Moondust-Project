cmake_minimum_required (VERSION 3.1.0)
project(MoondustMaintainer C CXX)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${DEPENDENCIES_INSTALL_DIR}/include
)
link_directories(${DEPENDENCIES_INSTALL_DIR}/lib)

# Default GIT version
include(../_common/git_version.cmake)
# Common building properties and optimization flags
include(../_common/build_pros.cmake)

add_definitions(-DPGE_FILES_QT)
include(../_common/PGE_File_Formats/pge_file_library.cmake)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Concurrent REQUIRED)

link_directories(
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
)

# Executables fail to build with Qt 5 in the default configuration
# without -fPIE. We add that here.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS} ${Qt5Concurrent_EXECUTABLE_COMPILE_FLAGS}")

qt5_wrap_ui(UIS_HDRS
    main_window.ui
    EpisodeCvt/episode_converter.ui
    Music/audiocvt_sox_gui.ui
)

set(RESOURCE ${CMAKE_CURRENT_SOURCE_DIR}/_resources/maintainer.qrc)
qt5_add_resources(RESOURCE_ADDED ${RESOURCE})

set(PGE_MAINTAINER_SRCS)

list(APPEND PGE_MAINTAINER_SRCS
    main.cpp
    main_window.cpp
    Music/audiocvt_sox_gui.cpp
    common_features/app_path.cpp
    files/episode_box.cpp
    common_features/logger.cpp
    EpisodeCvt/episode_converter.cpp
    ${UIS_HDRS}
    ${RESOURCE_ADDED}
)

if(APPLE)
    list(APPEND PGE_MAINTAINER_SRCS
        _resources/maintainer.icns
    )
endif()

if(WIN32)
    list(APPEND PGE_MAINTAINER_SRCS _resources/maintainer.rc)
endif()

add_executable(pge_maintainer
    ${PGE_MAINTAINER_SRCS}
    ${PGE_FILE_LIBRARY_SRCS}
)

# The Qt5Widgets_INCLUDES also includes the include directories for
# dependencies QtCore and QtGui
target_include_directories(pge_maintainer
    PRIVATE
    ${Qt5Widgets_INCLUDES}
    ${Qt5Concurrent_INCLUDES}
)

set(PGE_MAINTAINER_LINK_LIBS)
set(PGE_MAINTAINER_INSTALLS)

list(APPEND PGE_MAINTAINER_INSTALLS
    pge_maintainer
)

# We need add -DQT_WIDGETS_LIB when using QtWidgets in Qt 5.
target_compile_definitions(pge_maintainer
    PRIVATE
    ${Qt5Widgets_DEFINITIONS}
    ${Qt5Concurrent_DEFINITIONS}
)

if(WIN32)
    list(APPEND PGE_MAINTAINER_LINK_LIBS
        version
    )
else()
    if(UNIX AND NOT APPLE)
        list(APPEND PGE_MAINTAINER_LINK_LIBS dl)
    endif()
endif()

if(NOT EMSCRIPTEN AND NOT MSVC)
    if(WIN32)
        set_target_properties(pge_maintainer PROPERTIES LINK_FLAGS "-static -lpthread")
    else()
        list(APPEND PGE_MAINTAINER_LINK_LIBS pthread)
    endif()
endif()

if(APPLE)
    set_target_properties(pge_maintainer PROPERTIES
        OUTPUT_NAME "PGE Maintainer"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/_resources/maintainer.plist.in"
        MACOSX_BUNDLE_EXECUTABLE_NAME "pge_maintainer"
        MACOSX_BUNDLE_GUI_IDENTIFIER "ru.wohlsoft.pge-maintainer"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.0.1"
        MACOSX_BUNDLE_LONG_VERSION_STRING "0.0.1"
        MACOSX_BUNDLE_ICON_FILE "maintainer.icns"
        CPACK_BUNDLE_NAME "PGE Maintainer"
        MACOSX_BUNDLE_INFO_STRING "A simple looped music player working on SDL Mixer X"
    )
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/_resources/maintainer.icns" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    list(APPEND PGE_MAINTAINER_LINK_LIBS iconv)

    find_library(COREAUDIO_LIBRARY CoreAudio)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${COREAUDIO_LIBRARY})
    find_library(COREVIDEO_LIBRARY CoreVideo)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${COREVIDEO_LIBRARY})
    find_library(IOKIT_LIBRARY IOKit)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${IOKIT_LIBRARY})
    find_library(CARBON_LIBRARY Carbon)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${CARBON_LIBRARY})
    find_library(COCOA_LIBRARY Cocoa)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${COCOA_LIBRARY})
    find_library(FORCEFEEDBAK_LIBRARY ForceFeedback)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${FORCEFEEDBAK_LIBRARY})
    find_library(METAL_LIBRARY Metal)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${METAL_LIBRARY})
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${COREFOUNDATION_LIBRARY})
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${AUDIOTOOLBOX_LIBRARY})
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${AUDIOUNIT_LIBRARY})
    find_library(OPENGL_LIBRARY OpenGL)
    list(APPEND PGE_MAINTAINER_LINK_LIBS ${OPENGL_LIBRARY})
endif()

list(APPEND PGE_MAINTAINER_LINK_LIBS
    Qt5::Widgets
    Qt5::Concurrent
)

target_link_libraries(pge_maintainer
    ${PGE_MAINTAINER_LINK_LIBS}
)

install(TARGETS ${PGE_MAINTAINER_INSTALLS}
    RUNTIME DESTINATION "${PGE_INSTALL_DIRECTORY}"
)