#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <map>
#include "Message.hpp"
#include "Logger.hpp"

enum DOWNLOAD_STATUS
{
  NOT_DOWNLOADED,
  DOWNLOADING,
  DOWNLOADED
};

struct PieceBuffer
{
  std::string hash;
  unsigned short port;
  std::string ip;
  unsigned int index;
  std::vector<char> data;
  public:
     //PieceBuffer(const PieceBuffer& other);
     PieceBuffer(unsigned int _index, std::vector<char>&& _data );
     //PieceBuffer& operator=(const PieceBuffer& other);
    //~PieceBuffer();
};

struct BlocksBuffer
{
  std::vector<char> data;
  std::vector<bool> status;
  unsigned int num = 0;
  unsigned int index = 0;
  int selectedPiece = -1;
  public:
    BlocksBuffer(unsigned int pieceLength, unsigned int blocksCount);
    void reset(unsigned int pieceLength, unsigned int blocksCount);
};

enum PEER_STATUS
{
  CHOKED,
  UNCHOKED
};

struct Peer
{
  std::atomic<bool> threadRunning = false;
  unsigned short port;
  std::string ip;
  unsigned int pieceIndex = 0;
  BlocksBuffer blocksBuffer;
  PEER_STATUS status;
  std::vector<unsigned char> availablePieces;
  public:
    Peer(std::string ip, unsigned short port, unsigned int pieceLength, unsigned int blocksCount);
};

class Torrent
{
public:
  Torrent(unsigned int const blockSize, unsigned int const parallelReqsNum, LogLevel logLevel);
  ~Torrent();
  void upload();
  void download(std::string torrentURL, std::string ip, uint16_t port, std::string downloadedFilePath);

private:
  void parseTorrentFile(std::string torrentURL);
  void execute();
  boost::asio::awaitable<void> storePieces(std::string downloadedFilePath);
  boost::asio::awaitable<void> downloadFromPeer(std::string ip, unsigned short port, unsigned int pieceLength, unsigned int blocksCount);
  boost::asio::awaitable<bool> selectPeace(PeerSession &peerSession, Peer &peer);
  boost::asio::awaitable<void> downloadCo(std::string torrentURL, std::string dhtBoostrapNodeIp, uint16_t dhtBoostrapNodePort, std::string downloadedFilePath);
  std::string name;
  std::string peerId;
  std::string infoHash;
  std::vector<std::byte> piecesStatus;
  boost::asio::io_context io;
  std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> guard;
  boost::asio::strand<boost::asio::io_context::executor_type> pStatusStd;
  unsigned int pieceLength;
  unsigned int lastPieceLength;
  unsigned int piecesNum;
  unsigned int rcvPiecesNum = 0;
  std::vector<char> piecesHash;
  std::queue<PieceBuffer> piecesQueue;
  boost::asio::strand<boost::asio::io_context::executor_type> pQueueStd;
  std::atomic<DOWNLOAD_STATUS> status;
  boost::asio::strand<boost::asio::io_context::executor_type> statusStd;
  unsigned int interval = 1;
  unsigned int const blockSize;       // 16384;
  unsigned int const parallelReqsNum; // = 5;
  boost::asio::strand<boost::asio::io_context::executor_type> printStd;
  Logger logger;
  unsigned int threadNum;
  std::thread workerThread;
  std::multimap<std::string, bool> threadsMap;
  boost::asio::strand<boost::asio::io_context::executor_type> threadMapStd;
  boost::asio::awaitable<void> releasePendingPieces(Peer &peer);
};
