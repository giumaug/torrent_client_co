#include "Logger.hpp"
#include <iostream>
#include <boost/asio.hpp>

Logger::Logger(LogLevel _level) : level{_level}
{}

boost::asio::awaitable<void> Logger::log(LogLevel level, std::string logString, boost::asio::strand<boost::asio::io_context::executor_type> &printStd)
{
  auto defaultExecutor = co_await boost::asio::this_coro::executor;
  if (level >= this->level)
  {
    auto now = std::chrono::system_clock::now();
    co_await boost::asio::dispatch(bind_executor(printStd, boost::asio::use_awaitable)); 
    std::cout << std::format("{:%F %T}", now) << logString << std::endl;
    co_await boost::asio::post(defaultExecutor, boost::asio::use_awaitable);
  }
  co_return;
}

void Logger::log(LogLevel level, std::string logString)
{
  if (level >= this->level)
  {
    std::cout << logString << std::endl;
  }
}
