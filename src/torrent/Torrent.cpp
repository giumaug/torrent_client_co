#include <string>
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <openssl/sha.h>
#include "bencoding/bencode.hpp"
#include <cpr/cpr.h>
#include "Common.hpp"
#include "Torrent.hpp"
#include "Utils.hpp"
#include "Dht.hpp"

PieceBuffer::PieceBuffer(unsigned int _index, std::vector<char> _data) : index{_index}, data{_data}
{}

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

//void Torrent::announce(std::string torrentURL, std::string ip, std::string port, std::string event)
//{
//  try
//  {
//    std::string trackerURL;
//    std::ifstream in(torrentURL);
//    auto decodedTorrent = bencode::decode(in, bencode::no_check_eof);
//    auto keys = std::get<bencode::dict>(decodedTorrent);
//
//    for (auto it = keys.begin(); it != keys.end(); ++it)
//    {
//      if (it->first == "announce")
//      {
//        trackerURL = std::get<bencode::string>(decodedTorrent["announce"]);
//        break;
//      }
//      if (it->first == "announce-list")
//      {
//        // manage announce-list
//      }
//    }
//
//    auto info = decodedTorrent["info"];
//    auto encodedInfo = bencode::encode(info);
//    unsigned int length = std::get<bencode::integer>(info["length"]);
//    this->name = std::get<bencode::string>(info["name"]);
//    this->pieceLength = std::get<bencode::integer>(info["piece length"]);
//    this->infoHash = doSha1(encodedInfo);
//    this->piecesNum = length / this->pieceLength;
//    piecesStatus.resize(this->piecesNum);
//    auto _tmp = std::get<bencode::string>(info["pieces"]);
//    std::vector<char> pieceHash(_tmp.begin(), _tmp.end());
//    this->piecesHash = pieceHash;
//    for (auto i = 0; i < this->piecesNum; i++)
//      piecesStatus[i] = static_cast<std::byte>(NOT_DOWNLOADED);
//
//    cpr::Response r = cpr::Get(cpr::Url{trackerURL},
//                               cpr::Parameters{{"info_hash", infoHash},
//                                               {"peer_id", this->peerId},
//                                               {"ip", ip},
//                                               {"port", port},
//                                               {"uploaded", "0"},
//                                               {"downloaded", "0"},
//                                               {"left", std::to_string(length)},
//                                               {"event", event}});
//
//    std::cout << r.text << std::endl;
//    auto announceResponse = bencode::decode(r.text);
//    this->interval = std::get<bencode::integer>(announceResponse["interval"]);
//    auto peers = std::get<bencode::list>(announceResponse["peers"]);
//
//    for (auto peerIt = peers.begin(); peerIt != peers.end(); ++peerIt)
//    {
//      auto _peerData = std::get<bencode::dict>(*peerIt);
//      std::string _ip = std::get<bencode::string>(_peerData["ip"]);
//      unsigned int _port = (unsigned short)std::get<bencode::integer>(_peerData["port"]);
//
//      this->peers.emplace(std::piecewise_construct,
//                          std::forward_as_tuple(_ip + ":" + std::to_string(_port)),
//                          std::forward_as_tuple(_ip, _port, this->pieceLength, (this->pieceLength / this->blockSize)));
//    }
//  }
//  catch (std::exception &e)
//  {
//    e.what();
//    throw TorrentException(ANNOUNCE_FAILED);
//  }
//}

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
  this->peerId = randomSequence(20, 33, 126);
}

void Torrent::upload()
{
}

void Torrent::download(std::string torrentURL, std::string dhtBoostrapNodeIp, uint16_t dhtBoostrapNodePort, std::string downloadedFilePath)
{
  std::vector<std::thread> threads;
  std::stack<DhtNode> dhtNodeStack;
  parseTorrentFile(torrentURL);
  std::thread store(&Torrent::storePieces, this, downloadedFilePath);
  std::string nodeId = makeNodeId();
  DhtNode dhtNode(std::move(dhtBoostrapNodeIp), dhtBoostrapNodePort, std::move(nodeId));
  
  do
  {
    dhtNodeStack.push(std::move(dhtNode));
    do
    {
      DhtNode dhtNode = std::move(dhtNodeStack.top());
      dhtNodeStack.pop();
      NodePeer nodePeer = findPeers(this->infoHash, dhtNode.id, dhtNode.ip, dhtNode.port);
      int counter = 0;
      if (nodePeer.peers.size() > 0)
      {
        logger.log(DEBUG, "Found peer list with size" + std::to_string(nodePeer.peers.size()));
        bool inserted = false;
        for (auto &_peer : nodePeer.peers)
        {
          threads.emplace_back(&Torrent::downloadFromPeer, this, _peer.ip, _peer.port, this->pieceLength, (this->pieceLength / this->blockSize));
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

  for (auto &t : threads)
  {
    t.join();
  }
  logger.log(DEBUG, "All threads completed!!!");
  store.join();
}

bool Torrent::selectPeace(PeerSession &peerSession, Peer &peer)
{
  bool pieceFound = false;
  bool endSearch = false;
  unsigned int blocksCount = this->pieceLength / this->blockSize;
  unsigned int lastBlocksCount = this->lastPieceLength / this->blockSize;
  unsigned startIndex = peer.pieceIndex;

  while (!this->pStatusMtx.try_lock());
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
  this->pStatusMtx.unlock();

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
      
      logger.log(DEBUG, (peer.ip + ":" + std::to_string(peer.port) + "---" + "Sending1 index=" + std::to_string(peer.pieceIndex) + " begin=" + std::to_string((i * this->blockSize))));
      peer.blocksBuffer.status[i] = true;
      peerSession.send(request);
    }
    peer.blocksBuffer.index = reqsNum - 1;
  }
  else // to delete
  {
    logger.log(DEBUG, std::to_string(peer.port) + "---" + "No piece found");
  }
  return pieceFound;
}

void Torrent::downloadFromPeer(std::string ip, unsigned short port, unsigned int pieceLength, unsigned int blocksCount)
{
  Peer peer(ip, port, pieceLength, blocksCount);
  try
  {
    peer.threadRunning = true;
    bool availablePieces = false;
    PeerSession peerSession;
    unsigned int blocksCount = this->pieceLength / this->blockSize;
    unsigned int lastPieceBlocksCount = this->lastPieceLength / this->blockSize;
    logger.log(DEBUG, "calling " + peer.ip + ":" + std::to_string(peer.port));
    peerSession.open(peer.ip, peer.port);
    if (!peerSession.handshake(this->infoHash, this->peerId))
      throw TorrentException(HANDSHAKE_ERROR);
    while (!this->statusMtx.try_lock());
    if (status != DOWNLOADING && status != DOWNLOADED)
      status = DOWNLOADING;
    this->statusMtx.unlock();  

    Message bitfield = peerSession.receive();
    if (bitfield.id != BITFIELD)
      throw TorrentException(READ_ERROR);
    peer.availablePieces = std::move(bitfield.payload);

    Message interested;
    interested.id = INTERESTED;
    interested.length = 1;
    peerSession.send(interested);

    logger.log(DEBUG, "Thread" + peer.ip + ":" + std::to_string(peer.port) + "started");
    bool exit = false;
    while (status == DOWNLOADING && !exit)
    {
      Message message = peerSession.receive();
      switch (message.id)
      {
        case UNCHOKE:
        {
          logger.log(DEBUG, std::to_string(peer.port) + "--- UNCHOKE");

          if (peer.status == CHOKED)
          {
            availablePieces = selectPeace(peerSession, peer);
            peer.status = UNCHOKED;
          }
          break;
        }
        case EMPTY:
        {
          logger.log(DEBUG, std::to_string(peer.port) + "--- EMPTY");
          while (!this->pStatusMtx.try_lock());
          int selectedPiece = peer.blocksBuffer.selectedPiece;
          if (selectedPiece != -1)
          {
            piecesStatus[selectedPiece] = std::byte(NOT_DOWNLOADED);
            peer.blocksBuffer.selectedPiece = -1;
            logger.log(DEBUG, "releasing piece" + std::to_string(selectedPiece));
          }
          this->pStatusMtx.unlock();
          exit = true;
          break;
        }
        case CHOKE:
        {
          logger.log(DEBUG, std::to_string(peer.port) + "--- CHOKE");
          if (peer.status == UNCHOKED)
          {
            peer.status = CHOKED;
            while (!this->pStatusMtx.try_lock());
            int selectedPiece = peer.blocksBuffer.selectedPiece;
            if (selectedPiece != -1)
            {
             piecesStatus[selectedPiece] = std::byte(NOT_DOWNLOADED);
             peer.blocksBuffer.selectedPiece = -1;
             logger.log(DEBUG, "releasing piece" + std::to_string(peer.pieceIndex));
            }
            this->pStatusMtx.unlock();
          }
          break;
        }
        case HAVE:
        {
          unsigned int newPieceIndex = (message.payload[0] << 24) + (message.payload[1] << 16) + (message.payload[2] << 8) + message.payload[3];
          unsigned int newPieceByte = newPieceIndex / 8;
          unsigned int newPieceBit = newPieceIndex % 8;

          logger.log(DEBUG, std::to_string(peer.port) + "--- HAVE");
          peer.availablePieces[newPieceByte] &= (1 << newPieceBit);
          if (availablePieces == false && peer.status == UNCHOKED)
            availablePieces = selectPeace(peerSession, peer);
          break;
        }
        case PIECE:
        {
          unsigned int blocksCount = this->pieceLength / this->blockSize;
          unsigned int lastBlocksCount = this->lastPieceLength / this->blockSize;
          int blocks = peer.pieceIndex == (piecesNum - 1) ? lastBlocksCount : blocksCount;
          unsigned int index = (message.payload[0] << 24) + (message.payload[1] << 16) + (message.payload[2] << 8) + (message.payload[3]);
          unsigned int begin = (message.payload[4] << 24) + (message.payload[5] << 16) + (message.payload[6] << 8) + message.payload[7];
          
          logger.log(DEBUG, peer.ip + ":" + std::to_string(peer.port) + "--- Received index=" + std::to_string(index) + " begin=" + std::to_string(begin));
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
                while (!this->pStatusMtx.try_lock())
                ;
                rcvPiecesNum++;
                
                logger.log(DEBUG, std::to_string(peer.port) + "--- Found piece " + std::to_string(peer.pieceIndex));
                piecesStatus[peer.pieceIndex] = static_cast<std::byte>(DOWNLOADED);
                this->pStatusMtx.unlock();
                while (!pQueueMtx.try_lock())
                  ;
                PieceBuffer pieceBuffer(peer.pieceIndex, peer.blocksBuffer.data);
                this->piecesQueue.push(pieceBuffer);
                if (rcvPiecesNum == piecesNum)
                {
                  while (!this->statusMtx.try_lock());
                  status = DOWNLOADED;
                  this->statusMtx.unlock(); 
                  _break = true;
                }
                pQueueMtx.unlock();
                if (_break)
                  break;
              }
              else
              {
                logger.log(DEBUG, std::to_string(peer.port) + "--- HASH ERROR " + std::to_string(peer.pieceIndex));
                while (!this->pStatusMtx.try_lock())
                  ;
                piecesStatus[peer.pieceIndex] = std::byte(NOT_DOWNLOADED);
                this->pStatusMtx.unlock();
              }
              peer.blocksBuffer.num = 0;
              peer.pieceIndex = (++peer.pieceIndex) % piecesNum;
              availablePieces = selectPeace(peerSession, peer);
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
                logger.log(DEBUG, peer.ip + ":" + std::to_string(peer.port) + "--- Sending2 index=" + std::to_string(peer.pieceIndex) + " begin=" + std::to_string((peer.blocksBuffer.index * this->blockSize)));
                peerSession.send(request);
              }
            }
          }
          break;
        }
      }
    }
  } 
  catch (TorrentException &ex)
  {
    logger.log(DEBUG, "Exception-1 on peer" + peer.ip + ":" + std::to_string(peer.port));
  }
  catch (const std::exception &ex)
  {
    logger.log(DEBUG, "Exception-2 on peer" + peer.ip + ":" + std::to_string(peer.port));
  }
  catch (...)
  {
    logger.log(DEBUG, "Exception-3 on peer" + peer.ip + ":" + std::to_string(peer.port));
  }
  logger.log(DEBUG, "Thread" + peer.ip + ":" + std::to_string(peer.port) + "completed");
}

void Torrent::storePieces(std::string downloadedFilePath)
{
  unsigned int storedPieces = 0;
  std::string fileName = downloadedFilePath + "/" + name;

  std::ofstream fileDesc{fileName, std::ios::binary};
  if (!fileDesc)
  {
    throw TorrentException(HANDSHAKE_ERROR);
  }

  while (storedPieces < this->piecesNum)
  {
    while (!pQueueMtx.try_lock())
      ;
    if (!this->piecesQueue.empty())
    {
      PieceBuffer pieceBuffer = std::move(this->piecesQueue.front());
      this->piecesQueue.pop();
      logger.log(DEBUG, "Storing piece:" + std::to_string(pieceBuffer.index));

      pQueueMtx.unlock();
      fileDesc.seekp(pieceBuffer.index * this->pieceLength);
      fileDesc.write(pieceBuffer.data.data(), pieceBuffer.data.size());
      storedPieces++;
    }
    else
    {
      pQueueMtx.unlock();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  fileDesc.close();
  logger.log(DEBUG, "Store completed!!!");
}
