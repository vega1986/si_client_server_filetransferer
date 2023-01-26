#pragma once
#include "sharedMemoryClientServerBase.h"

namespace si
{

  class sharedMemoryClient : private sharedMemoryClientServerBase
  {
  public:
    sharedMemoryClient(const std::string& sharedMemoryName, const size_t sharedMemorySize);
    ~sharedMemoryClient()
    {}
    void run();
  };

}
