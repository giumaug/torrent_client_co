find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED)

add_library(Torrent STATIC)

include(${CMAKE_SOURCE_DIR}/cmake/fetch_cpr.cmake)

target_include_directories(Torrent PUBLIC ${CMAKE_SOURCE_DIR}/inc/torrent  
  PRIVATE ${Boost_INCLUDE_DIRS}
  PRIVATE ${CMAKE_SOURCE_DIR}/src/torrent}
)

target_sources(Torrent
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src/torrent/Torrent.cpp
    ${CMAKE_SOURCE_DIR}/src/torrent/Message.cpp
    ${CMAKE_SOURCE_DIR}/src/torrent/Utils.cpp
    ${CMAKE_SOURCE_DIR}/src/torrent/Dht.cpp
    ${CMAKE_SOURCE_DIR}/src/torrent/Logger.cpp
)

target_link_libraries(Torrent PRIVATE OpenSSL::Crypto cpr::cpr)


  
   


