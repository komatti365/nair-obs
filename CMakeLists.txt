cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Core Widgets Network WebSockets REQUIRED)
  target_link_libraries(
    ${CMAKE_PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
  )
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
  )
endif()

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_20)

target_sources(
  ${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  src/common/NicoliveData.h
  src/common/ConfigManager.h
  src/common/ConfigManager.cpp
  src/auth/NicoliveAuthManager.h
  src/auth/NicoliveAuthManager.cpp
  src/api/NicoliveApiClient.h
  src/api/NicoliveApiClient.cpp
  src/comment/NdgrDecoder.h
  src/comment/NdgrDecoder.cpp
  src/comment/NicoliveCommentReceiver.h
  src/comment/NicoliveCommentReceiver.cpp
  src/obs/ObsStreamHelper.h
  src/obs/ObsStreamHelper.cpp
  src/ui/NicoliveControlDock.h
  src/ui/NicoliveControlDock.cpp
  src/ui/NicoliveCommentDock.h
  src/ui/NicoliveCommentDock.cpp
  src/ui/NicoliveSettingsDialog.h
  src/ui/NicoliveSettingsDialog.cpp
)

target_include_directories(
  ${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
