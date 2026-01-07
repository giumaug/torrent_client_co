add_subdirectory( ${CMAKE_SOURCE_DIR}/src/torrent/ )

add_executable(${CMAKE_PROJECT_NAME} ${CMAKE_PROJECT_NAME}.cpp)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/inc/ )

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Torrent)

