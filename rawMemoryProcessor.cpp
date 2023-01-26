#include "rawMemoryProcessor.h"

std::pair<void*, size_t> si::rawMemoryProcessor::get(size_t ith) const
{
  auto ptrMem = reinterpret_cast<unsigned char*>(m_ptrDump);
  ptrMem += sizeof(size_t);

  if ((numberOfFiles == 0) or (ith >= numberOfFiles))
  {
    return { nullptr, 0 };
  }

  size_t fileSize = 0;
  memcpy(&fileSize, ptrMem, sizeof(size_t));
  ptrMem += sizeof(fileSize);

  for (size_t j = 0; j < ith; ++j)
  {
    ptrMem += fileSize;
    memcpy(&fileSize, ptrMem, sizeof(size_t));
    ptrMem += sizeof(size_t);
  }
  return { ptrMem, fileSize };
}

std::pair<void*, size_t> si::rawMemoryProcessor::getLast() const
{
  return get(numberOfFiles - 1);
}

bool si::rawMemoryProcessor::push(const void* rawDump, const size_t bytesCount)
{
  size_t bytesOccupated = 0;
  auto ptrMem = reinterpret_cast<unsigned char*>(m_ptrDump);
  ptrMem += sizeof(size_t);
  bytesOccupated += sizeof(size_t);

  for (size_t j = 0; j < numberOfFiles; ++j)
  {
    size_t fileSize = 0;
    memcpy(&fileSize, ptrMem, sizeof(size_t));

    ptrMem += sizeof(size_t) + fileSize;
    bytesOccupated += sizeof(size_t) + fileSize;
  }
  auto bytesVacation = m_size - bytesOccupated;
  if (bytesVacation < sizeof(size_t) + bytesCount)
  {
    return false;
  }
  memcpy(ptrMem, &bytesCount, sizeof(size_t));
  ptrMem += sizeof(size_t);

  memcpy(ptrMem, rawDump, bytesCount);
  // возвращаемся на начало блока разделяемой памяти
  ptrMem = reinterpret_cast<unsigned char*>(m_ptrDump);
  ++numberOfFiles;
  memcpy(ptrMem, &numberOfFiles, sizeof(size_t));
  return true;
}
