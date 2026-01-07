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
NodePeer findPeers(std::string infoHash, std::string nodeId, std::string ip, unsigned short port);