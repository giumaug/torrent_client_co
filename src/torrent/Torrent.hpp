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
  unsigned int index;
  std::vector<char> data;
  public:
    PieceBuffer(unsigned int _index, std::vector<char> _data );
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
  void upload();
  void download(std::string torrentURL, std::string ip, uint16_t port, std::string downloadedFilePath);

private:
  void parseTorrentFile(std::string torrentURL);
  //void announce(std::string torrentURL, std::string ip, std::string port, std::string event);
  void storePieces(std::string downloadedFilePath);
  void downloadFromPeer(std::string ip, unsigned short port, unsigned int pieceLength, unsigned int blocksCount);
  bool selectPeace(PeerSession &peerSession, Peer &peer);
  std::string name;
  std::string peerId;
  std::string infoHash;
  std::vector<std::byte> piecesStatus;
  std::mutex pStatusMtx;
  unsigned int pieceLength;
  unsigned int lastPieceLength;
  unsigned int piecesNum;
  unsigned int rcvPiecesNum = 0;
  std::vector<char> piecesHash;
  std::queue<PieceBuffer> piecesQueue;
  std::mutex pQueueMtx;
  std::atomic<DOWNLOAD_STATUS> status;
  std::mutex statusMtx;
  unsigned int interval = 1;
  unsigned int const blockSize;       // 16384;
  unsigned int const parallelReqsNum; // = 5;
  //std::mutex printMtx;
  Logger logger;
};
