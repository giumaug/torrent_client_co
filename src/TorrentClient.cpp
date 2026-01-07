#include <iostream>
#include "torrent/Torrent.hpp"

constexpr auto torrent_file_path = "/home/peppe/Desktop/vlsi/cpp/torrent_client/doc/torrent_files/2.torrent";
constexpr auto download_file_path = "/home/peppe/Desktop/vlsi/cpp/torrent_client/doc/torrent_files/download";
constexpr auto block_size = 16384;
constexpr auto requests_num = 5;
constexpr auto dht_boostrap_node_ip = "67.215.246.10";
constexpr auto dht_boostrap_node_port = 6881;

int main()
{
  Torrent torrent(block_size, requests_num, DEBUG);
  torrent.download(torrent_file_path, dht_boostrap_node_ip, dht_boostrap_node_port, download_file_path);
  std::cout << "Download completed." << std::endl;
  return 0;
}
