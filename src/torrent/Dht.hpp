#include <string>

struct DhtNode
{
  std::string ip;
  uint16_t port;
  std::string id;
  public:
    DhtNode(std::string ip, uint16_t port, std::string id);
};

struct DhtPeer
{
  std::string ip;
  uint16_t port;
  public:
    DhtPeer(std::string ip, uint16_t port);
};

struct NodePeer
{
  std::vector<DhtPeer> peers;
  std::vector<DhtNode> nodes;
};

std::string makeNodeId();
boost::asio::awaitable<NodePeer> findPeers(boost::asio::io_context &io, std::string infoHash, std::string nodeId, std::string ip, unsigned short port);