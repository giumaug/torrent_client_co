
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <queue>
#include "bencoding/bencode.hpp"
#include "Utils.hpp"
#include "Dht.hpp"

using boost::asio::ip::udp;

DhtNode::DhtNode(std::string _ip, uint16_t _port, std::string _id) : ip{_ip}, port{_port}, id{_id} {};
DhtPeer::DhtPeer(std::string _ip, uint16_t _port) : ip{_ip}, port{_port} {};

std::string makeNodeId()
{
  return doSha1(randomSequence(20, 33, 126));
}

NodePeer findPeers(std::string infoHash, std::string nodeId, std::string ip, unsigned short port)
{
  NodePeer nodePeer;
  try
  {
    std::string queryResponse = "";
    queryResponse.resize(16384);
    std::string query = bencode::encode(
        bencode::dict{
            {"t", "aa"},
            {"y", "q"},
            {"q", "get_peers"},
            {"a",
             bencode::dict{
                 {"id", bencode::string(nodeId)},
                 {"info_hash", bencode::string(infoHash)}}}});

    //std::cout << "port is" << port << std::endl;
    udp::endpoint remote_endpoint = udp::endpoint(boost::asio::ip::make_address_v4(ip), port);
    boost::asio::io_context io_context;
    udp::socket socket(io_context);
    socket.open(udp::v4());
    socket.send_to(boost::asio::buffer(query), remote_endpoint);
    boost::asio::steady_timer timer(io_context);
    std::vector<unsigned char> xxx(486);
    bool timeout_occurred = false;
    std::size_t read_data = 0;

    socket.async_receive_from(
      boost::asio::buffer(queryResponse), remote_endpoint,
        [&timeout_occurred, &read_data, &timer](const boost::system::error_code& ec, std::size_t bytes_recvd) 
        {
          if (!ec && !timeout_occurred) 
          {
            read_data = bytes_recvd;
            timer.cancel();
          }
        });
    timer.expires_after(std::chrono::milliseconds(500));
    timer.async_wait([&](const boost::system::error_code& ec) 
    {
      if (!ec) 
      {
        timeout_occurred = true;
        socket.cancel();
      }
    });
      std::thread run_thread([&]() {
      io_context.restart();
      io_context.run();
    });
    run_thread.join();

    if (read_data > 0)
    {
      bool foundNode = false;
      queryResponse.resize(read_data);
      auto bencodedResponse = bencode::decode(queryResponse.begin(), queryResponse.end());
      auto keys = std::get<bencode::dict>(bencodedResponse["r"]);
      for (auto it = keys.begin(); it != keys.end(); ++it)
      {
        if (it->first == "nodes")
        {
          foundNode = true;
          std::vector<DhtNode> vDhtNodes;
          auto nodes = std::get<bencode::string>(it->second);
          std::vector<uint8_t> vNodes(nodes.begin(), nodes.end());
          for (auto it = vNodes.begin(); it != vNodes.end(); it += 26)
          {
            std::stringstream _ip;
            //std::string _nodeId(it, it + 19);
            std::string _nodeId = nodeId;
            _ip << (int)*(it + 20) << "." << (int)*(it + 21) << "." << (int)*(it + 22) << "." << (int)*(it + 23);
            uint16_t _port = *(it + 25) + (*(it + 24) << 8);
            DhtNode dhtNode(std::move(_ip.str()), _port, std::move(_nodeId));
            nodePeer.nodes.push_back(std::move(dhtNode));
          }
        }
        else if (it->first == "values")
        {
          std::vector<DhtPeer> vDhtPeers;
          auto peers = std::get<bencode::list>(it->second);
          for (auto it = peers.begin(); it != peers.end(); ++it)
          {
            std::string address = std::get<bencode::string>(*it);
            std::stringstream _ip;
            std::string _nodeId = nodeId;
            _ip << (unsigned int)(address[0] & 0xff)<< "." <<  (unsigned int)(address[1] & 0xff) << "." <<  (unsigned int)(address[2] & 0xff)<< "." <<  (unsigned int)(address[3] & 0xff);
            uint16_t _port =  (((unsigned int)(address[4] & 0xff)) << 8) +  (unsigned int)(address[5] & 0xff);
            {
              DhtPeer dhtPeer(std::move(_ip.str()), _port);
              nodePeer.peers.push_back(std::move(dhtPeer));
            }
          }
        }
      }
      if (!foundNode)
      {
        //std::cout << "Nothing here!!!" << std::endl;
      }
    }
  }
  catch (std::exception &e)
  {
  
  }
  return nodePeer;
}


