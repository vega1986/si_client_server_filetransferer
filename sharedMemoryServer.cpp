#include <iostream>
#include <vector>
#include <fstream>
#include "sharedMemoryServer.h"
#include "common.h"
#include "dumpManager.h"
//#include "rawMemoryProcessor.h"

//*******************************************************************************************************************************************************************************

si::sharedMemoryServer::sharedMemoryServer(const std::string& sharedMemoryName, const size_t sharedMemorySize)
  : sharedMemoryClientServerBase(sharedMemoryName, sharedMemorySize)
{
  // ���� ������ �� ������ � ������
  m_semMemoryAccessible = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semMemoryAccessibleName.c_str())), handleLiberator());
  if (*m_semMemoryAccessible == NULL)
  {
    throw std::logic_error("m_semMemoryAccessible is NULL");
  }


  // ���������������, ������
  m_handle = HandleOwner(new HANDLE(CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, static_cast<DWORD>(0), static_cast<DWORD>(m_size), m_name.c_str())),
    handleLiberator());
  if (*m_handle == NULL)
  {
    throw std::logic_error("m_handle is NULL");
  }
  // ���������� ���������� ������, ������� ���������� � ����������� ������ � ������ ������, �� ���� 0
  {
    dumpManager dm(*m_handle, m_size);
    dm.setFilesCount(0);
    // ���������� dm ����������� �� �����, ����� ��� ��������� ������ � ����
  }


  // ������ ��� �� ��������� �������, ��� ����������� ����, ��� ��� ���� ��������� ������������, ������� ��������
  m_semConsumed = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 1, 1, common::semConsumed.c_str())), handleLiberator());
  if (*m_semConsumed == NULL)
  {
    throw std::logic_error("m_semConsumed is NULL");
  }


  // � ������ ������ �� ������������ ��������
  m_semProduced = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, common::semProducedMaxCount, common::semProduced.c_str())), handleLiberator());
  if (*m_semProduced == NULL)
  {
    throw std::logic_error("m_semProduced is NULL");
  }

  //
  m_semConsumedPart = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semConsumedPart.c_str())), handleLiberator());
  if (*m_semConsumedPart == NULL)
  {
    throw std::logic_error("m_semConsumedPart is NULL");
  }

  //
  m_semProducedPart = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semProducedPart.c_str())), handleLiberator());
  if (*m_semProducedPart == NULL)
  {
    throw std::logic_error("m_semProducedPart is NULL");
  }

  // ������ � ������� ����� ��������, ����� ��������� �� �� ��������
  const bool semMemoryPreparedReleased = ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
  if (not semMemoryPreparedReleased)
  {
    throw std::logic_error("shared memory not prepared");
  }
}

//*******************************************************************************************************************************************************************************

void si::sharedMemoryServer::run()
{
  static std::vector<unsigned char> dump;
  static size_t file_index = 0;
  
  
  while (true)
  {
    const auto erasedCount =
      std::erase_if(
        consumers,
        [](auto& item)->bool
        {
          return (WaitForSingleObject(item.first, 0) == WAIT_OBJECT_0);
        });

    if (erasedCount > 0)
    {
      // ������������ ���� ���������, ��� �����
      // ��������� ����������� ������ ��� ������/������ ����������
      WaitForSingleObject(*m_semMemoryAccessible, INFINITE);
      {
        dumpManager dm(*m_handle, m_size);
        dm.optimaizeExpired();
      }
      // �������� ����, ������ ��� ������ ����������
      ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
    }

    //while (true)
    //{
    //  bool fulfiledTaskFound = false;
    //  for (auto it = consumers.begin(); it != consumers.end(); ++it)
    //  {
    //    //consumers.erase(it);it->first
    //    if (WaitForSingleObject(it->first, 0) == WAIT_OBJECT_0)
    //    {

    //      fulfiledTaskFound = true;
    //      break;
    //    }
    //  }
    //}

    // ������ ������ ��� ������� ������� produced
    WaitForSingleObject(*m_semProduced, INFINITE);

    dumpedFile df;
    bool breakSignal = false;

    // ��������� ����������� ������ ��� ������ / ������ ����������
    WaitForSingleObject(*m_semMemoryAccessible, INFINITE);
    {
      dumpManager dm(*m_handle, m_size);
      df = dm.getPrepared();

      //std::cout << "df.shift_fc = " << df.shift_fc << std::endl;

      if (df.isNull())
      {
        throw std::logic_error("si::sharedMemoryServer::run: impossible - df is NULL after client production");
      }
      dm.setValue(df.shift_wip, bool{ true });
      breakSignal = dm.getValue(df.shift_fs, size_t{ 0 }) == 0;
    }
    // ����������� ���������� �����
    ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
    //std::cout << " * breakSignal = " << static_cast<int>(breakSignal) << std::endl;
    if (breakSignal)
    {
      break;
    }

    // ������ ����� �� ������ �����
    consumers.push_back({
                            HANDLE {},
                            TheConsumerData
                            {
                              *m_handle,
                              m_size,
                              *m_semConsumed,
                              *m_semConsumedPart,
                              *m_semProducedPart,
                              *m_semMemoryAccessible,
                              ++file_index,
                              df
                            }
                          });
    TheConsumerData& refCD = consumers.back().second;
    // ����������� �������� � ������, ����� ������������� ����������� ������ ������
    consumers.back().first = CreateThread(NULL, 0, TheConsumerExecutor, &refCD, 0, NULL);
  }

  //while (true)
  //{
  //  // ������ ������ ��� ������� ������� produced
  //  WaitForSingleObject(*m_semProduced, INFINITE);

  //  // ��������� ����������� ������ ��� ������ / ������ ����������
  //  WaitForSingleObject(*m_semMemoryAccessible, INFINITE);
  //  {
  //    dumpManager dm(*m_handle, m_size);
  //    
  //    
  //    memoryHolder holder(*m_handle, m_size);
  //    auto memStart = holder.get();

  //    rawMemoryProcessor memProc(memStart, m_size);
  //    auto [lastFileAddress, lastFileSize] = memProc.getLast();

  //    // ��������� ���� �������� ������� - ���������� ������ �����������
  //    if (lastFileSize == 0)
  //    {
  //      dump.resize(0);
  //      // ����������� ������ �� ������, ������� �� � �� �����������
  //      // !!! ReleaseSemaphore(*m_semMemoryPrepared, 1, NULL);
  //      // ������������� � ���, ��� ������ ���������
  //      ReleaseSemaphore(*m_semConsumed, 1, NULL);
  //      break;
  //    }

  //    dump.resize(lastFileSize);
  //    CopyMemory(dump.data(), lastFileAddress, lastFileSize);

  //    // ��������� �������� ���������� ������ �� 1
  //    auto numOfFiles = memProc.getNumberOfFiles();
  //    --numOfFiles;
  //    CopyMemory(memStart, &numOfFiles, sizeof(size_t));
  //  }
  //  // ��������� ����������� ������
  //  ReleaseSemaphore(*m_semMemoryPrepared, 1, NULL);
  //  // ������������� � ���, ��� ������ ���������
  //  ReleaseSemaphore(*m_semConsumed, 1, NULL);

  //  std::ofstream ost("transfered_file_" + std::to_string(++file_index) + ".bin", std::ios_base::binary);
  //  ost.write(reinterpret_cast<char*>(dump.data()), dump.size() * sizeof(decltype(dump)::value_type));
  //  ost.close();
  //  ost.clear();

  //  std::cout << " (*) file " << file_index << " transfered" << std::endl;
  //}
}

DWORD WINAPI si::TheConsumerExecutor(void* ptrData)
{
  TheConsumerData* ptrCD = reinterpret_cast<TheConsumerData*>(ptrData);
  TheConsumerData& refCD = *ptrCD;

  const auto memHandle = refCD.shMemHandle;
  const auto memSize = refCD.memSize;
  const auto semConsumed = refCD.semConsumed;
  const auto semConsumedPart = refCD.semConsumedPart;
  const auto semProducedPart = refCD.semProducedPart;
  const auto semMemoryAccessible = refCD.semMemoryAccessible;
  const auto fileId = refCD.fileId;
  const std::string fileName = "transfered_file_" + std::to_string(fileId) + ".bin";
  const auto df = refCD.df;

  {
    dumpManager dm(memHandle, memSize, true); // ��������� �� ������
    const auto fc = dm.getValue(df.shift_fc, size_t{ 0 });
    const auto fs = dm.getValue(df.shift_fs, size_t{ 0 });
    std::vector<unsigned char> rawBytes;
    if (fs <= fc)
    {
      rawBytes.resize(fs);
      dm.readFile(df, rawBytes.data(), fs);
      // ���������� ������ � ����
      std::ofstream ost("transfered_file_" + std::to_string(fileId) + ".bin", std::ios_base::binary);
      ost.write(reinterpret_cast<char*>(rawBytes.data()), rawBytes.size() * sizeof(decltype(rawBytes)::value_type));
      ost.close();
      ost.clear();
    }
    else
    {
      dm.forceDetach(); // ����������� ���� ��� - ������ ��� ����������
      const size_t sizeOfLastTransfer = fs % fc;
      const size_t transferCount = (sizeOfLastTransfer > 0) ? ((fs / fc) + 1) : (fs / fc);

      for (size_t j = 0; j < transferCount; ++j)
      {
        // ��� ���� ������ ������� ��������� �����
        WaitForSingleObject(semProducedPart, INFINITE);
        // ������� - ��� �� ������� ��� ������
        dm.forceAttach();
        
        const size_t readBytesCount = (j == transferCount - 1) ? sizeOfLastTransfer : fc;
        rawBytes.resize(readBytesCount);
        dm.readFile(df, rawBytes.data(), readBytesCount);
        // ���������� ������ � ����
        const auto openMode = (j == 0) ? (std::ios_base::binary) : (std::ios_base::binary | std::ios_base::app);
        std::ofstream ost(fileName, openMode);
        ost.write(reinterpret_cast<char*>(rawBytes.data()), rawBytes.size() * sizeof(decltype(rawBytes)::value_type));
        ost.close();
        ost.clear();
        dm.forceDetach();
        ReleaseSemaphore(semConsumedPart, 1, NULL); // ��������, ��� ���������
      }
    }
  }
  // ������ ��� ����� �������� ������, ��� expired & not wip
  WaitForSingleObject(semMemoryAccessible, INFINITE);
  {
    dumpManager dm(memHandle, memSize); // ��������� �� ������
    dm.setValue(df.shift_expired, bool{ true });
    dm.setValue(df.shift_wip, bool{ false });
  }
  ReleaseSemaphore(semMemoryAccessible, 1, NULL);

  ReleaseSemaphore(semConsumed, 1, NULL);

  std::cout << " $ file " << fileName << " transfered!" << std::endl;

  return 0;
}
