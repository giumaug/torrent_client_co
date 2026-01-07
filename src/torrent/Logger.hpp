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
    Logger(LogLevel level);
    void log(LogLevel level, std::string logString);
  private:
    std::mutex mtx;
    LogLevel level;
};