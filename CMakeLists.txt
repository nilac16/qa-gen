cmake_minimum_required(VERSION 3.15)

project(QAGen C CXX)

find_package(DCMTK REQUIRED)
find_package(ITK REQUIRED)
find_package(json-c REQUIRED)

find_package(ZLIB REQUIRED)
find_package(unofficial-minizip REQUIRED)
find_path(LXW_INCLUDE_DIRS xlsxwriter.h)
find_library(LXW_LIBRARIES xlsxwriter REQUIRED)

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
                      ZLIB::ZLIB
                      unofficial::minizip::minizip
                      -manifest:embed
                      -manifestinput:${APP_MANIFEST})

set_property(TARGET ${PROJECT_NAME}
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


add_executable(mhd2dcm mhd2dcm.c
               ${CMAKE_SOURCE_DIR}/src/qagen-path.c
               ${CMAKE_SOURCE_DIR}/src/qagen-error.c
               ${CMAKE_SOURCE_DIR}/src/qagen-memory.c
               ${CMAKE_SOURCE_DIR}/src/qagen-debug.c
               ${CMAKE_SOURCE_DIR}/src/qagen-log.c
               ${CMAKE_SOURCE_DIR}/src/qagen-metaio.cxx)

target_link_libraries(mhd2dcm
              PUBLIC  PathCch
              PRIVATE DCMTK::DCMTK
                      ${ITK_LIBRARIES})

set_property(TARGET mhd2dcm
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


add_executable(img2dcm img2dcm.c
               ${CMAKE_SOURCE_DIR}/src/qagen-path.c
               ${CMAKE_SOURCE_DIR}/src/qagen-error.c
               ${CMAKE_SOURCE_DIR}/src/qagen-memory.c
               ${CMAKE_SOURCE_DIR}/src/qagen-debug.c
               ${CMAKE_SOURCE_DIR}/src/qagen-log.c
               ${CMAKE_SOURCE_DIR}/src/qagen-img2dcm.cxx)

target_link_libraries(img2dcm
              PUBLIC  PathCch
              PRIVATE DCMTK::DCMTK
                      ${ITK_LIBRARIES})

set_property(TARGET img2dcm
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
