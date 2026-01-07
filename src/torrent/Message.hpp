#include <cstddef>
#include <boost/asio.hpp>

enum MESSAGE_ID
{
  CHOKE,
  UNCHOKE,
  INTERESTED,
  NOT_INTERESTED,
  HAVE,
  BITFIELD,
  REQUEST,
  PIECE,
  CANCEL,
  EMPTY
};

struct Message
{
  unsigned int length;
  MESSAGE_ID id;
  std::vector<unsigned char> payload;
  std::vector<unsigned char> static makeRequestPayload(unsigned int index, unsigned int begin, unsigned int blockSize);
};

class PeerSession
{
private:
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket {io_context};
  std::size_t async_receive(std::vector<unsigned char> &buffer);
  std::size_t async_write(std::vector<unsigned char> &buffer);

public:
  void open(std::string _ip, unsigned short _port);
  void close();
  void send(Message message);
  Message receive();
  bool handshake(std::string infoHash, std::string peerId);
};
