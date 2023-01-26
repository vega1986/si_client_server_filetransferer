#include <iostream>
#include <fstream>
#include <vector>
#include "sharedMemoryClient.h"
#include "common.h"
#include "dumpManager.h"
//#include "rawMemoryProcessor.h"

//***********************************************************************************************************************************************************

si::sharedMemoryClient::sharedMemoryClient(const std::string& sharedMemoryName, const size_t sharedMemorySize)
  : sharedMemoryClientServerBase(sharedMemoryName, sharedMemorySize)
{
  // �������, ����������� ������ � ����������� ������� (������ ��� ������ ����������, �� �� ����� ������)
  m_semMemoryAccessible = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semMemoryAccessibleName.c_str())), handleLiberator());
  if (*m_semMemoryAccessible == NULL)
  {
    throw std::logic_error("m_semMemoryPrepared is NULL");
  }

  // �������, ������� ������������� � ���, ��� ������ ���� ��������� ��� ������ ��� �������
  m_semConsumed = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semConsumed.c_str())), handleLiberator());
  if (*m_semConsumed == NULL)
  {
    throw std::logic_error("m_semConsumed is NULL");
  }

  // �������, ������� ������������� � ���, ��� ������ �������� � ���� ����������� ������
  m_semProduced = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, common::semProducedMaxCount, common::semProduced.c_str())), handleLiberator());
  if (*m_semProduced == NULL)
  {
    throw std::logic_error("m_semProduced is NULL");
  }

  // ����� ����� ���� ������� ������� �������
  m_semConsumedPart = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semConsumedPart.c_str())));
  if (*m_semConsumedPart == NULL)
  {
    throw std::logic_error("m_semConsumedPart is NULL");
  }

  // ����� ����� ���� �������� ��������
  m_semProducedPart = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semProducedPart.c_str())));
  if (*m_semProducedPart == NULL)
  {
    throw std::logic_error("m_semProducedPart is NULL");
  }
}

//***********************************************************************************************************************************************************

void si::sharedMemoryClient::run()
{ 
  // ����������, ��������� ����������� ������
  m_handle = HandleOwner(new HANDLE(OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, common::ramMappingFileName.c_str())), handleLiberator());
  if (*m_handle == NULL)
  {
    throw std::logic_error("m_handle not opened at the side of client");
  }

  static std::vector<unsigned char> fileByteContent; // ������ �����, ������� ����� ��������
  fileByteContent.reserve(m_size);
  // ����������� � ������������ ����� ��� ��������
  while (std::cin)
  {
    std::cout << "enter file name for transfer $ " << std::endl;
    char sym = 0;
    std::cin >> sym;
    if (sym == '.')
    {
      break;
    }
    std::cin.unget();
    std::string fileName;
    std::cin >> fileName;
    std::ifstream ist(fileName, std::ios_base::binary);
    if (not ist.good())
    {
      std::cout << "   (!) file " << fileName << " has not been opened" << std::endl;
      continue;
    }
    //
    {
      ist.seekg(0, std::ios_base::end);
      const size_t bytesCount = ist.tellg();

      ist.seekg(0, std::ios_base::beg);
      fileByteContent.resize(bytesCount);
      ist.read(reinterpret_cast<char*>(fileByteContent.data()), bytesCount);
    }
    //
    ist.close();
    ist.clear();
    bool fileIsLarge = false;
    // ����������� ������ � ����������� ������ �� ��������� ����������
    WaitForSingleObject(*m_semMemoryAccessible, INFINITE);
    {
      dumpManager dm(*m_handle, m_size);
      dumpedFile df;

      while (true)
      {
        df = dm.acquireFile(fileByteContent.size());
        if (not (df.isNull()))
        {
          break;
        }
        // ��������� ������
        dm.forceDetach();
        // ������ � ��������� �� ������ � ������ ����������
        ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
        
        // ��� ����� ������ ����� ���������� ������� �������� � ������ ��� ������ � ������ ����������
        HANDLE handlesForWait[2];
        handlesForWait[0] = *m_semMemoryAccessible;
        handlesForWait[1] = *m_semConsumed;
        WaitForMultipleObjects(2, handlesForWait, true, INFINITE);
        // ���������������� � ������
        dm.forceAttach();
      }
      // ���������� ���� � ����
      const auto dumpFileCap = dm.getValue(df.shift_fc, size_t{ 0 });
      const auto dumpFileSize = dm.getValue(df.shift_fs, size_t{ 0 });
      if (dumpFileSize != fileByteContent.size())
      {
        throw std::logic_error("impossible: dumpFileSize != fileByteContent.size()!");
      }

      if (dumpFileSize <= dumpFileCap)
      {
        fileIsLarge = false;
        dm.writeFile(df, fileByteContent.data(), fileByteContent.size());
      }
      else
      {
        fileIsLarge = true;
        const size_t tailSize = dumpFileSize % dumpFileCap;
        const size_t fullStepsCount = (tailSize > 0) ? ((dumpFileSize / dumpFileCap) + 1) : (dumpFileSize / dumpFileCap);
        auto ptr = fileByteContent.data();
        for (size_t k = 0; k < fullStepsCount; ++k)
        {
          const auto writeSize = ((tailSize > 0) and (k == (fullStepsCount - 1))) ? tailSize : dumpFileCap;
          dm.writeFile(df, ptr, writeSize);
          // ��������� ������
          dm.forceDetach();
          // ��� ����� ���������� � ������� ������� �/��� ������ ��������
          ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
          // ����� ������ 1-��� ������ �������� �������, ��� ����� �������� ����� ��� ������ � ������
          if (k == 0)
          {
            ReleaseSemaphore(*m_semProduced, 1, NULL);
          }
          // ��� ������� ������� � ���, ��� ��� ��������� ����� ��������� ������ �����
          ReleaseSemaphore(*m_semProducedPart, 1, NULL);
          // ���, ����� ������ ���������� ���� �����
          WaitForSingleObject(*m_semConsumedPart, INFINITE);
          // ���������� ������ �� ������
          WaitForSingleObject(*m_semMemoryAccessible, INFINITE);
          // ����� ������ � �������� ������������ ������� ��� ������������ ������
          dm.forceAttach();
          ptr += dumpFileCap;
        }
      }
    }
    // ������������� � ���, ��� ����� ������ � ���� � ��� �� ������ ���������� (���������)
    ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
    if (not fileIsLarge)
    {
      // ������������� � ���, ��� � ����������� ������ ��� ��������� ���
      ReleaseSemaphore(*m_semProduced, 1, NULL);
    }
  }
}

//auto memStart = holder.get();

//rawMemoryProcessor memProc(memStart, m_size);
//while (not memProc.push(fileByteContent.data(), fileByteContent.size()))
//{
//  // ���������� ����������� ������, ����� �� ����� ��������������� ������ ��������/������
//  // ����� ����� ���������� ��� ���������� memoryHolder, ���� ��� ����������
//  holder.forceDetach();
//  // ����������� ������� m_semConsumed - �� ������ ����� ������������ ������� ����� ���������� ������ �� ����������� ������
//  // ���� ������� ������������� ������ ���� ����-�� �� �������� �� ������� ����� ��� ���� � ����
//  // �� ��������� ���� ������� ��������, ��� ��� �����������
//  WaitForSingleObject(*m_semConsumed, INFINITE);
//  // �����������, ����������� ������ � ����������� ������
//  ReleaseSemaphore(*m_semMemoryPrepared, 1, NULL);
//  // ��� ������� ����� ������ ����� �� ������ � ������ ������� ������� m_semConsumed. WaitForMultipleObjects - ����� ��� ����� �� �������� �����
//  HANDLE handlesForWait[2];
//  handlesForWait[0] = *m_semMemoryPrepared;
//  handlesForWait[1] = *m_semConsumed;
//  WaitForMultipleObjects(2, handlesForWait, true, INFINITE);
//  // ����, ���������, ��� ������ ������ ���� - ���������� ������������ � ���� ����� ��� ����� �������
//  holder.forceAttach();
//  // ����������� ������� ��� ��������� ��������� �������� �/��� ��� ������ ��������
//  ReleaseSemaphore(*m_semConsumed, 1, NULL);
//}
