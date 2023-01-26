#include <vector>
#include <iostream>
#include "dumpManager.h"

//************************************************************************************************************

si::dumpManager::dumpManager(const HANDLE handle, const size_t size, const bool readonly)
  :
    m_handle(handle),
    m_size(size),
    m_ptr(nullptr),
    m_readOnly(readonly)
{
  forceAttach();
}

//************************************************************************************************************

void si::dumpManager::forceAttach()
{
  if (m_ptr == nullptr)
  {
    if (m_readOnly)
    {
      m_ptr = reinterpret_cast<unsigned char*>(MapViewOfFile(m_handle, FILE_MAP_READ, 0, 0, m_size));
    }
    else
    {
      m_ptr = reinterpret_cast<unsigned char*>(MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, m_size));
    }
    if (m_ptr == nullptr)
    {
      throw std::logic_error("si::dumpManager::forceAttach failed");
    }
  }
}

//************************************************************************************************************

void si::dumpManager::forceDetach()
{
  if (m_ptr == nullptr)
  {
    return;
  }

  UnmapViewOfFile(m_ptr);
  m_ptr = nullptr;
}

//************************************************************************************************************

void si::dumpManager::initialize()
{
  if (m_ptr == nullptr)
  {
    throw std::logic_error("si::dumpManager::initialize: m_ptr == nullptr");
  }

  size_t nf = 0; // количество файлов, одновременно хранящихся в дампе и помещённых туда клиентами
  memcpy(m_ptr, &nf, sizeof(size_t));
}

//************************************************************************************************************

void si::dumpManager::free()
{
  initialize();
}

//************************************************************************************************************

size_t si::dumpManager::getFilesCount()
{
  if (m_ptr == nullptr)
  {
    throw std::logic_error("si::dumpManager::getFilesCount: m_ptr == nullptr");
  }

  size_t nf = 0; // количество файлов, одновременно хранящихся в дампе и помещённых туда клиентами
  memcpy(&nf, m_ptr, sizeof(size_t));
  return nf;
}

//************************************************************************************************************

void si::dumpManager::setFilesCount(const size_t newNf)
{
  if (m_ptr == nullptr)
  {
    throw std::logic_error("si::dumpManager::setFilesCount: m_ptr == nullptr");
  }

  memcpy(m_ptr, &newNf, sizeof(size_t));
}

//************************************************************************************************************

si::dumpedFile si::dumpManager::acquireFile(const size_t acquiredFileSize)
{
  if (m_ptr == nullptr)
  {
    throw std::logic_error("si::dumpManager::acquireFile: m_ptr == nullptr");
  }

  // сначала проходимся по уже имеющимся файлам, и если хватает места то переиспользуем
  // уже имеющийся саб-дамп
  // эта процедура должна вызываться НЕ конкурентно
  // Здесь мы уверены что другие клиенты сюда не придут
  // Поэтому делаем всё что хотим с теми файлами, которые не используются

  const auto endOfDump = m_ptr + m_size; // address of the next byte after last byte in the dump
  size_t shift = 0;
  const auto nf = getFilesCount();
  shift += sizeof(size_t);

  for (size_t j = 0; j < nf; ++j)
  {
    dumpedFile signature;
    
    signature.shift_fc = shift;
    const auto capacity = getValue(shift, size_t(0));
    shift += sizeof(size_t);

    signature.shift_fs = shift;
    const auto employedSize = getValue(shift, size_t(0));
    shift += sizeof(size_t);

    signature.shift_expired = shift;
    const auto isExpired = getValue(shift, bool(false));
    shift += sizeof(bool);

    // unused
    signature.shift_wip = shift;
    const auto isWip = getValue(shift, bool(false));
    shift += sizeof(bool);

    signature.shift_ptr = shift;
    const auto beginOfUsefulBytes = getPtr(shift);

    // смещаеся на потенциально новый файл (на будущее - для следующей итерации или по выходе из цикла)
    shift += capacity;

    if (not isExpired)
    {
      continue;
    }
    
    if (acquiredFileSize <= capacity)
    {
      setValue(signature.shift_fs, acquiredFileSize);
      setValue(signature.shift_expired, false);
      setValue(signature.shift_wip, false);

      return signature;
    }

    // файл целиком не помещается
    // если это не последний файл, тогда тут делать нечего, ищем дальше
    if (j < nf - 1)
    {
      continue;
    }

    // это последний файл и он точно не используется (is expired)
    // перенастраиваем его capacity, чтобы хватило для fileSize байт или до конца файла
    const size_t availableCapacity = endOfDump - beginOfUsefulBytes;
    if (availableCapacity == 0)
    {
      throw std::logic_error("impossible situation");
    }

    const size_t newCapacity = (acquiredFileSize > availableCapacity) ? availableCapacity : acquiredFileSize;
    //const size_t newSize = acquiredFileSize;
    //const bool expired = false;

    setValue(signature.shift_fc,      newCapacity);
    setValue(signature.shift_fs,      acquiredFileSize);
    setValue(signature.shift_expired, false);
    setValue(signature.shift_wip,     false);

    return signature;
  }
  
  // места не нашли в уже имеющихся файлах
  // shift - первый свободный байт
  // сколько у нас осталось байт свободных?
  const auto beginOfFree = m_ptr + shift;
  const size_t totalFreeBytesCount = endOfDump - beginOfFree;
  const size_t sizeOfHead = 2 * sizeof(size_t) + 2 * sizeof(bool);
  if (totalFreeBytesCount <= sizeOfHead)
  {
    return dumpedFile();
  }
  const size_t maxUsefulBytesCount = totalFreeBytesCount - sizeOfHead;

  const size_t newCapacity = (acquiredFileSize > maxUsefulBytesCount) ? maxUsefulBytesCount : acquiredFileSize;
  
  dumpedFile signature;

  signature.shift_fc = shift;
  shift += sizeof(size_t);
  
  signature.shift_fs = shift;
  shift += sizeof(size_t);

  signature.shift_expired = shift;
  shift += sizeof(bool);

  signature.shift_wip = shift;
  shift += sizeof(bool);

  signature.shift_ptr = shift;

  setValue(signature.shift_fc,      newCapacity);
  setValue(signature.shift_fs,      acquiredFileSize);
  setValue(signature.shift_expired, false);
  setValue(signature.shift_wip,     false);
  // разумеется увеличиваем количество файлов на 1
  this->setFilesCount(nf + 1);

  return signature;
}

//************************************************************************************************************

void si::dumpManager::killFile(const dumpedFile signature)
{
  setValue(signature.shift_expired, bool(true));
}

//************************************************************************************************************

void si::dumpManager::optimaizeExpired()
{
  static constexpr size_t approxMaxFilesCount = 128;

  size_t shift = 0;
  const auto nf = getFilesCount();
  auto remainedNf = nf;
  shift += sizeof(size_t);

  bool prevWasExpired = false;
  dumpedFile startExpired;
  for (size_t j = 0; j < nf; ++j)
  {   
    const auto fc = getValue(shift, size_t{ 0 });
    shift += sizeof(size_t);

    const auto fs = getValue(shift, size_t{ 0 });
    shift += sizeof(size_t);

    const auto expired = getValue(shift, bool(false));
    shift += sizeof(bool);

    const auto wip = getValue(shift, bool(false));
    shift += sizeof(bool);

    shift += fc;

    if (not expired)
    {
      prevWasExpired = false;
      continue;
    }

    if (prevWasExpired)
    {
      // необходима оптимизация
      size_t enlargedFileCapacity = getValue(startExpired.shift_fc, size_t{0});
      enlargedFileCapacity += 2 * sizeof(size_t) + 2 * sizeof(bool) + fc;
      setValue(startExpired.shift_fc, enlargedFileCapacity);
      --remainedNf;
    }
    else
    {
      startExpired = dumpedFile{ fc, fs, expired, wip, shift };
      prevWasExpired = true;
      continue;
    }
  }
  setFilesCount(remainedNf);
  // теперь не могут идти более 1 неиспользуемого блока подряд
  // если последний не используется удаляем его

  shift = 0;
  const auto newNf = remainedNf;
  shift += sizeof(size_t);

  for (size_t j = 0; j < newNf; ++j)
  {
    const auto fc = getValue(shift, size_t{ 0 });
    shift += sizeof(size_t);

    const auto fs = getValue(shift, size_t{ 0 });
    shift += sizeof(size_t);

    const auto expired = getValue(shift, bool(false));
    shift += sizeof(bool);

    const auto wip = getValue(shift, bool(false));
    shift += sizeof(bool);

    shift += fc;

    if (j == newNf - 1)
    {
      if (expired)
      {
        setFilesCount(newNf - 1);
      }
    }
    else
    {
      continue;
    }
  }
}

void si::dumpManager::writeFile(const dumpedFile signature, unsigned char* ptrData, size_t nBytes)
{
  if (m_ptr == nullptr)
  {
    throw std::logic_error("si::dumpManager::writeFile: dump not viewed");
  }
  if (signature.shift_ptr == 0)
  {
    throw std::logic_error("si::dumpManager::writeFile: trying to write into void dump");
  }
  if (nBytes > getValue(signature.shift_fc, size_t{0}))
  {
    throw std::logic_error("si::dumpManager::writeFile: try to write a lot of bytes");
  }

  auto ptrForWriting = getPtr(signature.shift_ptr);
  memcpy(ptrForWriting, ptrData, nBytes);
}

void si::dumpManager::readFile(const dumpedFile signature, unsigned char* ptrData, size_t nBytes)
{
  if (m_ptr == nullptr)
  {
    throw std::logic_error("si::dumpManager::readFile: dump not viewed");
  }
  if (signature.shift_ptr == 0)
  {
    throw std::logic_error("si::dumpManager::readFile: trying to read from void dump");
  }
  if (nBytes > getValue(signature.shift_fc, size_t{ 0 }))
  {
    throw std::logic_error("si::dumpManager::readFile: try to read a lot of bytes");
  }

  auto ptrForReading = getPtr(signature.shift_ptr);
  memcpy(ptrData, ptrForReading, nBytes);
}

si::dumpedFile si::dumpManager::getPrepared()
{
  if (m_ptr == nullptr)
  {
    throw std::logic_error("si::dumpManager::getPrepared: dump is not viewed");
  }
  size_t shift = 0;

  const auto nf = getFilesCount();
  shift += sizeof(size_t);
  
  //std::cout << " * nf = " << nf << std::endl;

  for (size_t j = 0; j < nf; ++j)
  {
    dumpedFile df;

    df.shift_fc = shift;
    const auto fc = getValue(shift, size_t{ 0 });
    shift += sizeof(size_t);

    df.shift_fs = shift;
    const auto fs = getValue(shift, size_t{ 0 });
    shift += sizeof(size_t);

    df.shift_expired = shift;
    const auto expired = getValue(shift, bool(false));
    shift += sizeof(bool);

    df.shift_wip = shift;
    const auto wip = getValue(shift, bool(false));
    shift += sizeof(bool);

    df.shift_ptr = shift;

    shift += fc;

    if (expired or wip)
    {
      continue;
    }
    return df;
  }

  return dumpedFile();
}

//************************************************************************************************************
