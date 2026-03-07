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
  if (dest.size() > 20)
    {
      while(1)
      {
        std::cout << "PANIC!!!" << std::endl;
      }
    } 
  return dest;
}

std::string ___doSha1(const std::string src)
{
    // Ensure the string has exactly 20 bytes of space
    std::string dest(SHA_DIGEST_LENGTH, '\0');

    SHA1(reinterpret_cast<const unsigned char*>(src.data()),
         src.size(),
         reinterpret_cast<unsigned char*>(&dest[0])); // Modern, safe way to get non-const ptr    
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