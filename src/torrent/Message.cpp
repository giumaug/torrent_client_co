#include "Common.hpp"
#include "Message.hpp"
#include "iostream"
#include <boost/asio/cancel_after.hpp>

PeerSession::~PeerSession()
{
  boost::system::error_code ec;
  socket.close(ec);
}

PeerSession::PeerSession(boost::asio::any_io_executor defaultExecutor, 
  boost::asio::io_context& _context) : 
  socket{defaultExecutor}, 
  context{_context}
{}

boost::asio::awaitable<void> PeerSession::open(std::string ip, unsigned short port)
{
  try
  {
    co_await this->socket.async_connect(
      boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(ip), port),
      boost::asio::cancel_after(std::chrono::milliseconds(500), boost::asio::use_awaitable));
  } 
  catch (std::exception &e)
  {
    throw TorrentException(OPEN_SESSION_FAILED);
  }
  co_return;
}

void PeerSession::close()
{
  try
  {
    this->socket.close();
  }
  catch (std::exception &e)
  {
    throw TorrentException(CLOSE_SESSION_FAILED);
  }
}

boost::asio::awaitable<void> PeerSession::send(Message message)
{
  try
  {
    std::vector<unsigned char> buffer(message.length + 4);
    buffer[0] = message.length >> 24;
    buffer[1] = (message.length >> 16) & 0xff;
    buffer[2] = (message.length >> 8) & 0xff;
    buffer[3] = message.length & 0xff;
    buffer[4] = static_cast<int>(message.id);
    if (message.length > 5)
    {
      for (int i = 0; i < message.length - 1; i++)
        buffer[i + 5] = message.payload[i];
    }
    co_await this->socket.async_send(boost::asio::buffer(buffer), 
    boost::asio::cancel_after(std::chrono::milliseconds(5000), boost::asio::use_awaitable));
  }
  catch (std::exception &e)
  {
    throw TorrentException(OPEN_SESSION_FAILED);
  }
  catch (...)
  {
    throw TorrentException(OPEN_SESSION_FAILED);
  }
  co_return;
}

boost::asio::awaitable<Message> PeerSession::receive()
{
  std::vector<unsigned char> headerBuffer(5);
  Message message;

  try
  {
    std::size_t read_data = co_await async_read(socket,
      boost::asio::buffer(headerBuffer),
      boost::asio::cancel_after(std::chrono::milliseconds(5000), boost::asio::use_awaitable));
    if (read_data > 0)
    {
      int msgLength = (headerBuffer[0] << 24) + (headerBuffer[1] << 16) + (headerBuffer[2] << 8) + headerBuffer[3];
      message.length = msgLength;
      message.id = static_cast<MESSAGE_ID>(headerBuffer[4]);
      if (msgLength > 1)
      {
        std::vector<unsigned char> payloadBuffer(msgLength - 1);
        std::size_t read_data = co_await async_read(socket,
          boost::asio::buffer(payloadBuffer),
          boost::asio::cancel_after(std::chrono::milliseconds(5000), boost::asio::use_awaitable));
        if (read_data > 0)
        {
          message.payload = std::move(payloadBuffer);
        }
        else
        {
          message.id = EMPTY;
        }
      }
    }
    else
    {
      message.id = EMPTY;
    }
  }
  catch (std::exception &e)
  {
    throw TorrentException(READ_ERROR);
  }
  catch (...)
  {
    throw TorrentException(READ_ERROR);
  }
  co_return message;
}

 boost::asio::awaitable<bool> PeerSession::handshake(std::string infoHash, std::string peerId)
{
  try
  {
    std::size_t write_data = 0;
    std::size_t read_data = 0;
    std::vector<unsigned char> rBuffer(68);
    std::vector<unsigned char> wBuffer;
    wBuffer.push_back(char(0x13));
    for (auto v : {'B', 'i', 't', 'T', 'o', 'r', 'r', 'e', 'n', 't', ' ', 'p', 'r', 'o', 't', 'o', 'c', 'o','l'}) wBuffer.push_back(v);
    for (auto v : {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}) wBuffer.push_back(char(v));
    for (auto v : infoHash) wBuffer.push_back(v);
    for (auto v : peerId) wBuffer.push_back(v);

    write_data = co_await boost::asio::async_write(socket,
    boost::asio::buffer(wBuffer),
    boost::asio::cancel_after(std::chrono::milliseconds(5000), boost::asio::use_awaitable));
    
    if (write_data > 0)
    {
      read_data = co_await async_read(socket,
        boost::asio::buffer(rBuffer),
        boost::asio::cancel_after(std::chrono::milliseconds(500), boost::asio::use_awaitable));
    }
    co_return (read_data > 0) && (write_data > 0);
  }
  catch (std::exception &e)
  {
    throw TorrentException(HANDSHAKE_ERROR);
  }
  co_return false;
}

std::vector<unsigned char> Message::makeRequestPayload(unsigned int index, unsigned int begin, unsigned int blockSize)
{
  std::vector<unsigned char> payload(12);

  payload[0] = index >> 24;
  payload[1] = (index >> 16) & 0xff;
  payload[2] = (index >> 8) & 0xff;
  payload[3] = index & 0xff;
  payload[4] = begin >> 24;
  payload[5] = (begin >> 16) & 0xff;
  payload[6] = (begin >> 8) & 0xff;
  payload[7] = begin & 0xff;
  payload[8] = blockSize >> 24;
  payload[9] = (blockSize >> 16) & 0xff;
  payload[10] = (blockSize >> 8) & 0xff;
  payload[11] = blockSize & 0xff;
  return payload;
}
