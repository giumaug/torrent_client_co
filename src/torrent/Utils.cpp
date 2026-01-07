#include "Utils.hpp"
#include <openssl/sha.h>
#include <random>
#include <iostream>
#include <iomanip>

std::string doSha1(std::string src)
{
  std::string dest = "XXXXXXXXXXXXXXXXXXXX";

  SHA1(reinterpret_cast<const unsigned char *>(src.c_str()),
       src.length(),
       reinterpret_cast<unsigned char *>(const_cast<char *>(dest.c_str())));
  //printSha1(dest);
  return dest;
}

std::string randomSequence(unsigned short size, unsigned short start, unsigned short end)
{
  std::string val = "";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distr(start, end);

  for (int i = 0; i < size; i++)
    val += distr(gen);
  return val;
}

//void printSha1(std::string val)
//{
//  for (int i = 0; i < 20; i++)
//  {
//    unsigned int hashVal = (unsigned char)val.at(i);
//    std::cout << std::setfill('0') << std::setw(2) << std::hex << hashVal;
//  }
//  std::cout << std::dec << std::endl;
//}