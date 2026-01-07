#include "Logger.hpp"
#include <iostream>

Logger::Logger(LogLevel _level) : level{_level}
{}
void Logger::log(LogLevel level, std::string logString)
{
  if (level >= this->level)
  {
    while (!this->mtx.try_lock());
    std::cout << logString << std::endl;
    this->mtx.unlock();
  }
}
