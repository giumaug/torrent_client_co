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
  boost::asio::io_context& context;
  //boost::asio::ip::tcp::socket socket {io_context};
  boost::asio::ip::tcp::socket socket;
  std::string _ip;
  unsigned short _port = 0;

public:
  PeerSession(boost::asio::any_io_executor defaultExecutor, boost::asio::io_context& _context);
  ~PeerSession();
  boost::asio::awaitable<void> open(std::string _ip, unsigned short _port);
  void close();
  boost::asio::awaitable<void> send(Message message);
  boost::asio::awaitable<Message> receive();
  boost::asio::awaitable<bool> handshake(std::string infoHash, std::string peerId);
};
