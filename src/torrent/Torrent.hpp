#include <boost/asio.hpp>
//#include <boost/asio/spawn.hpp>
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
  unsigned short port;
  std::string ip;
  unsigned int index;
  std::vector<char> data;
  public:
    PieceBuffer(unsigned int _index, std::vector<char> _data );
    ~PieceBuffer();
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
  //void announce(std::string torrentURL, std::string ip, std::string port, std::string event);
  void execute();
  boost::asio::awaitable<void> storePieces(std::string downloadedFilePath);
  boost::asio::awaitable<void> downloadFromPeer(std::string ip, unsigned short port, unsigned int pieceLength, unsigned int blocksCount);
  boost::asio::awaitable<bool> selectPeace(PeerSession &peerSession, Peer &peer);
  boost::asio::awaitable<void> downloadCo(std::string torrentURL, std::string dhtBoostrapNodeIp, uint16_t dhtBoostrapNodePort, std::string downloadedFilePath);
  //void __download(std::string torrentURL, std::string dhtBoostrapNodeIp, uint16_t dhtBoostrapNodePort, std::string downloadedFilePath);
  std::string name;
  std::string peerId;
  std::string infoHash;
  std::vector<std::byte> piecesStatus;
  boost::asio::io_context io;
  std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> guard;
  //std::mutex pStatusMtx;
  //boost::asio::strand<boost::asio::any_io_executor> pStatusStd = boost::asio::make_strand(io);
  //boost::asio::strand<boost::asio::io_context::executor_type> pStatusStd;
  boost::asio::strand<boost::asio::io_context::executor_type> pStatusStd = boost::asio::make_strand(io);
  unsigned int pieceLength;
  unsigned int lastPieceLength;
  unsigned int piecesNum;
  unsigned int rcvPiecesNum = 0;
  std::vector<char> piecesHash;
  std::queue<PieceBuffer> piecesQueue;
  //std::mutex pQueueMtx;
  //boost::asio::strand<boost::asio::any_io_executor> pQueueStd = boost::asio::make_strand(io);
  //boost::asio::strand<boost::asio::io_context::executor_type> pQueueStd;
  boost::asio::strand<boost::asio::io_context::executor_type> pQueueStd = boost::asio::make_strand(io);
  std::atomic<DOWNLOAD_STATUS> status;
  //std::mutex statusMtx;
  //boost::asio::strand<boost::asio::any_io_executor> statusStd = boost::asio::make_strand(io);
  //boost::asio::strand<boost::asio::io_context::executor_type> statusStd;
  boost::asio::strand<boost::asio::io_context::executor_type> statusStd = boost::asio::make_strand(io);
  unsigned int interval = 1;
  unsigned int const blockSize;       // 16384;
  unsigned int const parallelReqsNum; // = 5;
  //std::mutex printMtx;
  boost::asio::strand<boost::asio::io_context::executor_type> printStd = boost::asio::make_strand(io);
  Logger logger;
  unsigned int threadNum;
  std::thread workerThread;
  std::string _ip;
  unsigned short _port;
  std::multimap<std::string, bool> threadsMap;
  boost::asio::strand<boost::asio::io_context::executor_type> threadMapStd = boost::asio::make_strand(io);
  boost::asio::awaitable<void> releasePendingPieces(Peer &peer);
  boost::asio::awaitable<void> queueCheck();
};
