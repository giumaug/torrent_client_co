#include "Common.hpp"
#include "Message.hpp"
#include "iostream"

void PeerSession::open(std::string ip, unsigned short port)
{
  try
  {
    bool timeout_occurred = false;
    boost::asio::steady_timer timer(io_context);    
    this->socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(ip), port),
    [&timeout_occurred, &timer](const boost::system::error_code& ec) 
      {
        if (!ec && !timeout_occurred) 
        {
          timer.cancel();
        }
      });

    timer.expires_after(std::chrono::milliseconds(500));
    timer.async_wait([&](const boost::system::error_code& ec) 
    {
      if (!ec) 
      {
        timeout_occurred = true;
        socket.cancel();
      }
    });

    std::thread run_thread([&]() {
      io_context.restart();
      io_context.run();
      });
    run_thread.join();
  } 
  catch (std::exception &e)
  {
    throw TorrentException(OPEN_SESSION_FAILED);
  }
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

void PeerSession::send(Message message)
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
    this->socket.send(boost::asio::buffer(buffer), 0);
  }
  catch (std::exception &e)
  {
    throw TorrentException(OPEN_SESSION_FAILED);
  }
}

Message PeerSession::receive()
{
  std::vector<unsigned char> headerBuffer(5);
  Message message;

  try
  {
    std::size_t read_data = async_receive(headerBuffer);
    if (read_data > 0)
    {
      int msgLength = (headerBuffer[0] << 24) + (headerBuffer[1] << 16) + (headerBuffer[2] << 8) + headerBuffer[3];
      message.length = msgLength;
      message.id = static_cast<MESSAGE_ID>(headerBuffer[4]);
      if (msgLength > 1)
      {
        std::vector<unsigned char> payloadBuffer(msgLength - 1);
        std::size_t read_data = async_receive(payloadBuffer);
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
  return message;
}

bool PeerSession::handshake(std::string infoHash, std::string peerId)
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
    
    write_data = async_write(wBuffer);
    if (write_data > 0)
    {
      read_data = async_receive(rBuffer);
    }
    return (read_data > 0) && (write_data > 0);
  }
  catch (std::exception &e)
  {
    throw TorrentException(HANDSHAKE_ERROR);
  }
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

std::size_t PeerSession::async_receive(std::vector<unsigned char> &buffer)
{
  boost::asio::steady_timer timer(io_context);  
  bool timeout_occurred = false;
  std::size_t read_data = 0;
  
  boost::asio::async_read(socket,
    boost::asio::buffer(buffer),
      [&timeout_occurred, &read_data, &timer](const boost::system::error_code& ec, std::size_t bytes_recvd) 
      {
        if (!ec && !timeout_occurred) 
        {
          read_data = bytes_recvd;
          timer.cancel();
        }
      });

  timer.expires_after(std::chrono::milliseconds(500));
  timer.async_wait([&](const boost::system::error_code& ec) 
  {
    if (!ec) 
    {
      timeout_occurred = true;
      socket.cancel();
    }
  });

  std::thread run_thread([&]() {
    io_context.restart();
    io_context.run();
  });
  run_thread.join();
  return read_data;
}

std::size_t PeerSession::async_write(std::vector<unsigned char> &buffer)
{
  boost::asio::steady_timer timer(io_context);  
  bool timeout_occurred = false;
  std::size_t read_data = 0;
  
  boost::asio::async_write(socket,
    boost::asio::buffer(buffer),
      [&timeout_occurred, &read_data, &timer](const boost::system::error_code& ec, std::size_t bytes_recvd) 
      {
        if (!ec && !timeout_occurred) 
        {
          read_data = bytes_recvd;
          timer.cancel();
        }
      });

  timer.expires_after(std::chrono::milliseconds(500));
  timer.async_wait([&](const boost::system::error_code& ec) 
  {
    if (!ec) 
    {
      timeout_occurred = true;
      socket.cancel();
    }
  });

  std::thread run_thread([&]() {
    io_context.restart();
    io_context.run();
  });
  run_thread.join();
  return read_data;
}
