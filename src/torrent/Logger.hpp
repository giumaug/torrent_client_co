#include <boost/asio.hpp>
#include <string>
#include <mutex>

enum LogLevel
{
  ALL,
  DEBUG,
  INFO,
  WARN,
  ERROR,
  OFF
};

 //ALL < DEBUG < INFO < WARN < ERROR < FATAL < OFF. 

class Logger
{
  public:
    Logger(LogLevel _level);
    boost::asio::awaitable<void> log(LogLevel level, std::string logString, boost::asio::strand<boost::asio::io_context::executor_type> &printStd);
    void log(LogLevel level, std::string logString);
  private:
    //std::mutex mtx;
    LogLevel level;
};