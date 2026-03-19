cmake_minimum_required(VERSION 3.16)
project(kate-liveshare VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets WebSockets)
find_package(KF6 6.0 REQUIRED COMPONENTS TextEditor I18n XmlGui CoreAddons)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(PLUGIN_SOURCES
    ${SRC}/ot_engine.cpp
    ${SRC}/participant.cpp
    ${SRC}/host_server.cpp
    ${SRC}/guest_client.cpp
    ${SRC}/session.cpp
    ${SRC}/plugin.cpp
    ${SRC}/tool_view.cpp
)

kcoreaddons_add_plugin(kateplugin_liveshare
    INSTALL_NAMESPACE "kf6/ktexteditor"
    SOURCES ${PLUGIN_SOURCES}
)

target_include_directories(kateplugin_liveshare PRIVATE ${SRC})

target_link_libraries(kateplugin_liveshare
    Qt6::Core
    Qt6::Widgets
    Qt6::WebSockets
    KF6::TextEditor
    KF6::I18n
    KF6::XmlGui
    KF6::CoreAddons
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
