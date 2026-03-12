#include <string>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <unordered_map>
#include <openssl/sha.h>
#include "bencoding/bencode.hpp"
#include <cpr/cpr.h>
#include "Common.hpp"
#include "Torrent.hpp"
#include "Utils.hpp"
#include "Dht.hpp"
//#include <stacktrace>

PieceBuffer::PieceBuffer(unsigned int _index, std::vector<char> _data) : index{_index}, data{_data}
{}

PieceBuffer::~PieceBuffer()
{
  auto now = std::chrono::system_clock::now();
  std::cout << std::format("{:%F %T}", now) << "Inside PieceBuffer destructor" << ip << ":" << std::to_string(port) << std::endl;
}

Peer::Peer(std::string _ip, unsigned short _port, unsigned int pieceLength, unsigned int blocksCount) : ip{_ip}, port{_port}, blocksBuffer(pieceLength, blocksCount)
{
  status = CHOKED;
}

BlocksBuffer::BlocksBuffer(unsigned int pieceLength, unsigned int blocksCount)
{
  data.resize(pieceLength);
  status.resize(blocksCount);
  this->reset(pieceLength, blocksCount);
}

void BlocksBuffer::reset(unsigned int pieceLength, unsigned int blocksCount)
{
  data.resize(pieceLength);
  status.resize(blocksCount);
  for (auto it = data.begin(); it != data.end(); ++it)
    *it = 0;
  for (auto it = status.begin(); it != status.end(); ++it)
    *it = false;
}

void Torrent::parseTorrentFile(std::string torrentURL)
{
  try
  {
    std::ifstream in(torrentURL);
    auto decodedTorrent = bencode::decode(in, bencode::no_check_eof);
    auto keys = std::get<bencode::dict>(decodedTorrent);
    auto info = decodedTorrent["info"];
    auto encodedInfo = bencode::encode(info);
    unsigned int length = std::get<bencode::integer>(info["length"]);
    this->name = std::get<bencode::string>(info["name"]);
    this->pieceLength = std::get<bencode::integer>(info["piece length"]);
    this->lastPieceLength = length % this->pieceLength;
    this->infoHash = doSha1(encodedInfo);
    this->piecesNum = length / this->pieceLength + ((length % this->pieceLength) > 0 ? 1 :0);
    piecesStatus.resize(this->piecesNum);
    auto _tmp = std::get<bencode::string>(info["pieces"]);
    std::vector<char> pieceHash(_tmp.begin(), _tmp.end());
    this->piecesHash = pieceHash;
    for (auto i = 0; i < this->piecesNum; i++)
      piecesStatus[i] = static_cast<std::byte>(NOT_DOWNLOADED);
  }
  catch (std::exception &e)
  {
    e.what();
    throw TorrentException(PARSE_FAILED);
  }
}

Torrent::Torrent(unsigned int const _blockSize, unsigned int const _parallelReqsNum, LogLevel logLevel) : blockSize{_blockSize}, parallelReqsNum{_parallelReqsNum}, logger(logLevel)
{
  peerId = randomSequence(20, 33, 126);
  pStatusStd = boost::asio::make_strand(io);
  pQueueStd = boost::asio::make_strand(io);
  statusStd = boost::asio::make_strand(io);
  guard.emplace(boost::asio::make_work_guard(io));
  workerThread = std::thread(&Torrent::execute, this);
}

Torrent::~Torrent() 
{
  workerThread.join();
}

void Torrent::execute()
{  
  std::vector<std::thread> threadsPool;

  for (int n = 0; n < 8; ++n) 
  {
    threadsPool.emplace_back(
      [&ioc = this->io]()
      { 
        ioc.run(); 
      }
    );
  }
  for (auto& t : threadsPool)
  { 
    t.join();
  }
}

void Torrent::upload()
{
}

void Torrent::download(std::string torrentURL, std::string dhtBoostrapNodeIp, uint16_t dhtBoostrapNodePort, std::string downloadedFilePath)
{
  std::future<void> future = co_spawn(io, downloadCo(torrentURL, dhtBoostrapNodeIp, dhtBoostrapNodePort, downloadedFilePath), boost::asio::use_future);
  future.get();
  guard.reset(); 
}

boost::asio::awaitable<void> Torrent::downloadCo(std::string torrentURL, std::string dhtBoostrapNodeIp, uint16_t dhtBoostrapNodePort, std::string downloadedFilePath)
{
  try 
  {
    auto defaultExecutor = co_await boost::asio::this_coro::executor;
    std::stack<DhtNode> dhtNodeStack;
    parseTorrentFile(torrentURL);
    auto storeStd = boost::asio::make_strand(defaultExecutor);
    auto dhtStd = boost::asio::make_strand(defaultExecutor);
    boost::asio::co_spawn(storeStd, storePieces(downloadedFilePath), boost::asio::detached);
    std::string nodeId = makeNodeId();
  
    DhtNode dhtNode(std::move(dhtBoostrapNodeIp), dhtBoostrapNodePort, std::move(nodeId));
    do
    {
      dhtNodeStack.push(std::move(dhtNode));
      do
      {
        DhtNode dhtNode = std::move(dhtNodeStack.top());
        dhtNodeStack.pop();
        std::future<NodePeer> future = co_spawn(dhtStd, 
        findPeers(this->io, this->infoHash, dhtNode.id, dhtNode.ip, dhtNode.port), boost::asio::use_future);
        NodePeer nodePeer = future.get();

       int counter = 0;
       if (nodePeer.peers.size() > 0)
       {
          logger.log(ERROR, "Found peer list with size" + std::to_string(nodePeer.peers.size()));
          bool inserted = false;
          for (auto &_peer : nodePeer.peers)
          {
            auto key = _peer.ip + ":" + std::to_string(_peer.port);
            co_await boost::asio::post(threadMapStd, boost::asio::use_awaitable);
            if (!threadsMap.contains(key))
            {
              std::cout << "Updating threadsMap with:" << key << std::endl;
              auto downloadStd = boost::asio::make_strand(defaultExecutor);
              threadsMap.emplace(key,true);
              std::cout << "threadsMap size is" << threadsMap.size() << std::endl;
              boost::asio::co_spawn(downloadStd, 
                downloadFromPeer(_peer.ip, _peer.port, this->pieceLength, (this->pieceLength / this->blockSize)),
                boost::asio::detached);
            }
            co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
          }
        }
        if (nodePeer.nodes.size() > 0)
        {
          for (auto &dhtNode : nodePeer.nodes)
          {
            dhtNodeStack.push(std::move(dhtNode));
          }
        }
      } 
      while (!dhtNodeStack.empty() && this->status != DOWNLOADED);
    }
    while (this->status != DOWNLOADED);
    guard.reset(); 
    logger.log(DEBUG, "All threads completed!!!");
  }
  catch(...) 
  {
    //std::cout << "stacktrac:" << std::stacktrace::current() << std::endl;
  }
  co_return;
}

boost::asio::awaitable<bool> Torrent::selectPeace(PeerSession &peerSession, Peer &peer)
{
  bool pieceFound = false;
  bool endSearch = false;
  unsigned int blocksCount = this->pieceLength / this->blockSize;
  unsigned int lastBlocksCount = this->lastPieceLength / this->blockSize;
  unsigned startIndex = peer.pieceIndex;
  auto defaultExecutor = co_await boost::asio::this_coro::executor;

  co_await boost::asio::post(pStatusStd, boost::asio::use_awaitable);
  while (!pieceFound && !endSearch)
  {
    unsigned int pieceByte = peer.pieceIndex / 8;
    unsigned int pieceBit = peer.pieceIndex % 8;
    bool piecePresent = (peer.availablePieces[pieceByte] & (128 >> pieceBit)) > 0 ? true : false;  
    bool pieceNotDownloaded = piecesStatus[peer.pieceIndex] == std::byte(NOT_DOWNLOADED);
    if (pieceNotDownloaded && piecePresent)
    {
      piecesStatus[peer.pieceIndex] = std::byte(DOWNLOADING);
      pieceFound = true;
      peer.blocksBuffer.selectedPiece = peer.pieceIndex;
    }
    else
    {
      peer.pieceIndex = (++peer.pieceIndex) % piecesNum;
      if (peer.pieceIndex == startIndex)
      {
        endSearch = true;
        peer.blocksBuffer.selectedPiece = -1;
      }
    }
  }
  co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);

  if (pieceFound)
  {
    int blocks = peer.pieceIndex == (piecesNum) - 1 ? lastBlocksCount : blocksCount;
    int lenght = peer.pieceIndex == (piecesNum) - 1 ? this->lastPieceLength : this->pieceLength;
    int reqsNum = blocks < this->parallelReqsNum ? blocks : this->parallelReqsNum;
    peer.blocksBuffer.reset(lenght, blocks);
    for (int i = 0; i < reqsNum; i++)
    {
      Message request;
      request.id = REQUEST;
      request.length = 13;
      request.payload = Message::makeRequestPayload(peer.pieceIndex, (i * this->blockSize), this->blockSize);
      
      co_await logger.log(DEBUG, (peer.ip + ":" + std::to_string(peer.port) + "---" + "Sending1 index=" + std::to_string(peer.pieceIndex) + " begin=" + std::to_string((i * this->blockSize))), printStd);
      peer.blocksBuffer.status[i] = true;
      co_await peerSession.send(request);
    }
    peer.blocksBuffer.index = reqsNum - 1;
  }
  else
  {
    co_await logger.log(DEBUG, std::to_string(peer.port) + "---" + "No piece found", printStd);
  }
  co_return pieceFound;
}

boost::asio::awaitable<void> Torrent::downloadFromPeer(std::string ip, unsigned short port, unsigned int pieceLength, unsigned int blocksCount)
{
  this->_ip = ip;
  this->_port = port;
  int exceptionNum;
  Peer peer(ip, port, pieceLength, blocksCount);
  std::string stacktrace;
  auto defaultExecutor = co_await boost::asio::this_coro::executor;
  try
  {
    peer.threadRunning = true;
    bool availablePieces = false;
    unsigned int blocksCount = this->pieceLength / this->blockSize;
    unsigned int lastPieceBlocksCount = this->lastPieceLength / this->blockSize;
    PeerSession peerSession(defaultExecutor, io);

    co_await logger.log(DEBUG, "calling " + peer.ip + ":" + std::to_string(peer.port), printStd);
    co_await peerSession.open(peer.ip, peer.port);
    if (! co_await peerSession.handshake(this->infoHash, this->peerId))
      throw TorrentException(HANDSHAKE_ERROR);
    co_await boost::asio::post(statusStd, boost::asio::use_awaitable);
    if (status != DOWNLOADING && status != DOWNLOADED)
      status = DOWNLOADING;
    co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);

    Message bitfield = co_await peerSession.receive();
    if (bitfield.id != BITFIELD)
      throw TorrentException(READ_ERROR);
    peer.availablePieces = std::move(bitfield.payload);

    Message interested;
    interested.id = INTERESTED;
    interested.length = 1;
    co_await peerSession.send(interested);

    co_await logger.log(DEBUG, "Thread" + peer.ip + ":" + std::to_string(peer.port) + "started", printStd);
    bool exit = false;
    while (status == DOWNLOADING && !exit)
    {
      Message message = co_await peerSession.receive();
      switch (message.id)
      {
        case UNCHOKE:
        {
          co_await logger.log(DEBUG, std::to_string(peer.port) + "--- UNCHOKE", printStd);

          if (peer.status == CHOKED)
          {
            availablePieces = co_await selectPeace(peerSession, peer);
            peer.status = UNCHOKED;
          }
          break;
        }
        case EMPTY:
        {
        
          co_await logger.log(DEBUG, std::to_string(peer.port) + "--- EMPTY", printStd);
          co_await releasePendingPieces(peer);
          exit = true;
          break;
        }
        case CHOKE:
        {
          bool isLog = false;
          co_await logger.log(DEBUG, std::to_string(peer.port) + "--- CHOKE", printStd);
          if (peer.status == UNCHOKED)
          {
            peer.status = CHOKED;
            co_await boost::asio::post(this->pStatusStd, boost::asio::use_awaitable);
            int selectedPiece = peer.blocksBuffer.selectedPiece;
            if (selectedPiece != -1)
            {
             piecesStatus[selectedPiece] = std::byte(NOT_DOWNLOADED);
             peer.blocksBuffer.selectedPiece = -1;
             isLog = true;
            }
            co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
            if (isLog)
            {
              co_await logger.log(DEBUG, "releasing piece" + std::to_string(peer.pieceIndex), printStd);
            }
          }
          break;
        }
        case HAVE:
        {
          unsigned int newPieceIndex = (message.payload[0] << 24) + (message.payload[1] << 16) + (message.payload[2] << 8) + message.payload[3];
          unsigned int newPieceByte = newPieceIndex / 8;
          unsigned int newPieceBit = newPieceIndex % 8;

          co_await logger.log(DEBUG, std::to_string(peer.port) + "--- HAVE", printStd);
          peer.availablePieces[newPieceByte] &= (1 << newPieceBit);
          if (availablePieces == false && peer.status == UNCHOKED)
            availablePieces = co_await selectPeace(peerSession, peer);
          break;
        }
        case PIECE:
        {
          unsigned int blocksCount = this->pieceLength / this->blockSize;
          unsigned int lastBlocksCount = this->lastPieceLength / this->blockSize;
          int blocks = peer.pieceIndex == (piecesNum - 1) ? lastBlocksCount : blocksCount;
          unsigned int index = (message.payload[0] << 24) + (message.payload[1] << 16) + (message.payload[2] << 8) + (message.payload[3]);
          unsigned int begin = (message.payload[4] << 24) + (message.payload[5] << 16) + (message.payload[6] << 8) + message.payload[7];
          
          co_await logger.log(DEBUG, peer.ip + ":" + std::to_string(peer.port) + "--- Received index=" + std::to_string(index) + " begin=" + std::to_string(begin), printStd);
          if (peer.pieceIndex == index)
          {
            peer.blocksBuffer.num++;
            for (int i = 0; i < this->blockSize; i++)
            {
              peer.blocksBuffer.data[i + begin] = message.payload[8 + i];
            }
            peer.blocksBuffer.status[begin / this->blockSize] = true;

            if (peer.blocksBuffer.num == blocks)
            {
              std::vector<char> vPieceHash(20);
              for (int i = 0; i < 20; i++)
                vPieceHash[i] = this->piecesHash[(index * 20) + i];
              std::string _tmp(peer.blocksBuffer.data.begin(), peer.blocksBuffer.data.end());
              std::string downloadPieceHash = doSha1(_tmp);
              std::string pieceHash(vPieceHash.begin(), vPieceHash.end());
              if (downloadPieceHash == pieceHash)
              {
                bool _break = false;
                co_await logger.log(DEBUG, std::to_string(peer.port) + "--- Found piece " + std::to_string(peer.pieceIndex), printStd);
                co_await boost::asio::post(this->pStatusStd, boost::asio::use_awaitable);
                rcvPiecesNum++;
                piecesStatus[peer.pieceIndex] = static_cast<std::byte>(DOWNLOADED);
                co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
                
                 co_await logger.log(DEBUG, peer.ip + ":" + std::to_string(peer.port) + "peer.blocksBuffer.data size is" + std::to_string(peer.blocksBuffer.data.size()) , printStd);
                 if (peer.blocksBuffer.data.size() > 262144)
                 {
                    co_await logger.log(DEBUG, peer.ip + ":" + std::to_string(peer.port) + "PANIC on peer.blocksBuffer.data size is" + std::to_string(peer.blocksBuffer.data.size()) , printStd);
                 }

                PieceBuffer pieceBuffer(peer.pieceIndex, peer.blocksBuffer.data);
                pieceBuffer.ip = peer.ip;
                pieceBuffer.port = peer.port;
                
                //-------------------------
                co_await queueCheck();
                co_await boost::asio::post(pQueueStd, boost::asio::use_awaitable);
                if (peer.blocksBuffer.data.size() > 262144)
                {
                  std::cout << "PANIC on peer.blocksBuffer.data size is" << std::to_string(peer.blocksBuffer.data.size()) << std::endl;
                }
                if (pieceBuffer.data.size() > 262144 )
                {
                  std::cout << "PANIC pieceBuffer.data.size size is" << std::to_string(pieceBuffer.data.size()) << std::endl;
                }
                //----------------------------

                this->piecesQueue.push(pieceBuffer);
                co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
                if (rcvPiecesNum == piecesNum)
                {
                  co_await boost::asio::post(this->statusStd, boost::asio::use_awaitable);
                  status = DOWNLOADED;
                  co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
                  _break = true;
                }
                if (_break)
                  break;
              }
              else
              {
                co_await logger.log(DEBUG, std::to_string(peer.port) + "--- HASH ERROR " + std::to_string(peer.pieceIndex), printStd);
                co_await boost::asio::post(this->pStatusStd, boost::asio::use_awaitable);
                piecesStatus[peer.pieceIndex] = std::byte(NOT_DOWNLOADED);
                co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
              }
              peer.blocksBuffer.num = 0;
              peer.pieceIndex = (++peer.pieceIndex) % piecesNum;
              availablePieces = co_await selectPeace(peerSession, peer);
            }
            else
            {
              peer.blocksBuffer.index++;
              if (peer.blocksBuffer.index >= blocks)
              {
                peer.blocksBuffer.index = 0;
              }
              if (peer.status == UNCHOKED && peer.blocksBuffer.status[peer.blocksBuffer.index] == false)
              {
                Message request;
                request.id = REQUEST;
                request.length = 13;
                request.payload = Message::makeRequestPayload(peer.pieceIndex, (peer.blocksBuffer.index * this->blockSize), this->blockSize);
                co_await logger.log(DEBUG, peer.ip + ":" + std::to_string(peer.port) + "--- Sending2 index=" + std::to_string(peer.pieceIndex) + " begin=" + std::to_string((peer.blocksBuffer.index * this->blockSize)), printStd);
                co_await peerSession.send(request);
              }
            }
          }
          break;
        }
      }
    }
    co_await logger.log(DEBUG, "Exit for thread" + peer.ip + ":" + std::to_string(peer.port), printStd);
    peerSession.close();
  }
  catch (TorrentException &ex)
  {
    exceptionNum = 1;
  }
  catch (const std::exception &ex)
  {
     exceptionNum = 2;
  }
  catch (...)
  {
     exceptionNum = 1;
  }

  switch (exceptionNum)
  {
    case 1:
    {
      co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
      co_await logger.log(DEBUG, "Exception-1 on peer" + peer.ip + ":" + std::to_string(peer.port) + stacktrace, printStd);
      break;
    }
    case 2:
    {
      co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
      co_await logger.log(DEBUG, "Exception-2 on peer" + peer.ip + ":" + std::to_string(peer.port), printStd);
      break;  
    }
    case 3:
    {
      co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
      co_await logger.log(DEBUG, "Exception-3 on peer" + peer.ip + ":" + std::to_string(peer.port), printStd);
      break;
    }
  }
  
  co_await releasePendingPieces(peer);
  co_await boost::asio::post(threadMapStd, boost::asio::use_awaitable);
  auto key = ip + ":" + std::to_string(port);
  threadsMap.erase(key);
  co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
  std::cout << "Exiting from" << ip + ":" << std::to_string(port) << std::endl;
  co_return;
}

boost::asio::awaitable<void> Torrent::releasePendingPieces(Peer &peer)
{
  auto defaultExecutor = co_await boost::asio::this_coro::executor;
  bool isLog = false;
  co_await boost::asio::post(this->pStatusStd, boost::asio::use_awaitable);
  int selectedPiece = peer.blocksBuffer.selectedPiece;
  if (selectedPiece != -1)
  {
    piecesStatus[selectedPiece] = std::byte(NOT_DOWNLOADED);
    peer.blocksBuffer.selectedPiece = -1;
    isLog = true;
  }
  co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
  if (isLog)
  {
    co_await logger.log(DEBUG, "releasing piece" + std::to_string(selectedPiece), printStd);
  }
  co_return;
}

//RACE CONDITION SU STRAND
//https://gemini.google.com/app/dce2b18a0278ab8b
//IMPORTANTE std::ofstream e' bloccante
// https://gemini.google.com/app/287a5c6b411d297f
boost::asio::awaitable<void> Torrent::storePieces(std::string downloadedFilePath)
{
  auto defaultExecutor = co_await boost::asio::this_coro::executor;
  unsigned int storedPieces = 0;
  std::string fileName = downloadedFilePath + "/" + name;

  std::ofstream fileDesc{fileName, std::ios::binary};
  if (!fileDesc)
  {
    throw TorrentException(HANDSHAKE_ERROR);
  }

  while (storedPieces < this->piecesNum)
  {
    co_await boost::asio::post(this->pQueueStd, boost::asio::use_awaitable);
    if (!this->piecesQueue.empty())
    {
      //PieceBuffer pieceBuffer = std::move(this->piecesQueue.front());
      PieceBuffer pieceBuffer = this->piecesQueue.front();
      this->piecesQueue.pop();
      co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
      fileDesc.seekp(pieceBuffer.index * this->pieceLength);
      fileDesc.write(pieceBuffer.data.data(), pieceBuffer.data.size());
      storedPieces++;
      co_await logger.log(ERROR, "Storing piece:" + std::to_string(pieceBuffer.index), printStd);
    }
    else
    {
      co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
    }
  }
  fileDesc.close();
  co_await logger.log(DEBUG, "Store completed!!!", printStd);
  co_return;
}

boost::asio::awaitable<void> Torrent::queueCheck()
{
  auto defaultExecutor = co_await boost::asio::this_coro::executor;
  std::queue<PieceBuffer> temp;

  co_await boost::asio::post(this->pStatusStd, boost::asio::use_awaitable);
  auto now = std::chrono::system_clock::now();
  std::cout << std::format("{:%F %T}", now) << "Entering queueCheck" << std::endl;
  while (!this->piecesQueue.empty()) 
  {
    PieceBuffer pieceBuffer = this->piecesQueue.front();
    if (pieceBuffer.data.size() > 262144)
    {
      std::cout << "PANIC pieceBuffer.data.size size is" << std::to_string(pieceBuffer.data.size()) << std::endl;
    }
    temp.push(pieceBuffer);
    this->piecesQueue.pop();
  }
  this->piecesQueue.swap(temp);
  auto now2 = std::chrono::system_clock::now();
  std::cout << std::format("{:%F %T}", now2) << "Leaving queueCheck" << std::endl;
  co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
}


