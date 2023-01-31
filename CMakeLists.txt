cmake_minimum_required(VERSION 3.15)

project(QAGen C CXX)

find_package(DCMTK REQUIRED)
find_package(ITK REQUIRED)
find_package(json-c REQUIRED)

find_package(ZLIB REQUIRED)
find_path(LXW_INCLUDE_DIRS xlsxwriter.h)
find_library(LXW_LIBRARIES xlsxwriter)

include(${ITK_USE_FILE})

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /W3")   # These are ignored, wtf
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Zc:__cplusplus /W3")

set(APP_MANIFEST ${CMAKE_SOURCE_DIR}/QAGen.exe.manifest)

add_subdirectory(${CMAKE_SOURCE_DIR}/src)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
       PUBLIC  ${QAGEN_SOURCES}
       PRIVATE ${APP_MANIFEST})

target_link_libraries(${PROJECT_NAME}
              PUBLIC  User32
                      Comctl32
                      PathCch
                      DbgHelp
              PRIVATE DCMTK::DCMTK
                      ${ITK_LIBRARIES}
                      json-c::json-c
                      ${LXW_LIBRARIES}
                      ${ZLIB_LIBRARIES}
                      -manifest:embed
                      -manifestinput:${APP_MANIFEST})

set_property(TARGET ${PROJECT_NAME}
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


add_executable(MHD2DCM mhd2dcm.c
               ${CMAKE_SOURCE_DIR}/src/qagen-path.c
               ${CMAKE_SOURCE_DIR}/src/qagen-error.c
               ${CMAKE_SOURCE_DIR}/src/qagen-memory.c
               ${CMAKE_SOURCE_DIR}/src/qagen-debug.c
               ${CMAKE_SOURCE_DIR}/src/qagen-log.c
               ${CMAKE_SOURCE_DIR}/src/qagen-metaio.cxx)

target_link_libraries(MHD2DCM
              PUBLIC  PathCch
              PRIVATE DCMTK::DCMTK
                      ${ITK_LIBRARIES})

set_property(TARGET MHD2DCM
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
