#pragma once
#include <utility>
#include <stdexcept>
#include <cstring>

namespace si
{

  class rawMemoryProcessor
  {
  public:
    rawMemoryProcessor(void* ptrDump, size_t size)
      : m_ptrDump(ptrDump), m_size(size)
    {
      if (ptrDump == nullptr or size == 0)
      {
        throw std::logic_error("bad usage of rawMemoryProcessor");
      }
      memcpy(&numberOfFiles, m_ptrDump, sizeof(size_t));
    }

    std::pair<void*, size_t> get(size_t ith) const;
    std::pair<void*, size_t> getLast() const;
    size_t getNumberOfFiles() const
    {
      return numberOfFiles;
    }
    bool push(const void* rawDump, const size_t bytesCount);
  private:
    void* m_ptrDump = nullptr;
    size_t m_size = 0;
    size_t numberOfFiles = 0;
  };

}
