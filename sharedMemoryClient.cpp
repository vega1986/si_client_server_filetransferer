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
  // семафор, блокирующий работу с разделяемой памятью (запись или чтение оглавления, но не самих данных)
  m_semMemoryAccessible = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semMemoryAccessibleName.c_str())), handleLiberator());
  if (*m_semMemoryAccessible == NULL)
  {
    throw std::logic_error("m_semMemoryPrepared is NULL");
  }

  // семафор, который сигнализирует о том, что память была уменьшена или только что создана
  m_semConsumed = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semConsumed.c_str())), handleLiberator());
  if (*m_semConsumed == NULL)
  {
    throw std::logic_error("m_semConsumed is NULL");
  }

  // семафор, который сигнализирует о том, что данные помещены в дамп разделяемой памяти
  m_semProduced = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, common::semProducedMaxCount, common::semProduced.c_str())), handleLiberator());
  if (*m_semProduced == NULL)
  {
    throw std::logic_error("m_semProduced is NULL");
  }

  // часть файла была считана потоком сервера
  m_semConsumedPart = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semConsumedPart.c_str())));
  if (*m_semConsumedPart == NULL)
  {
    throw std::logic_error("m_semConsumedPart is NULL");
  }

  // часть файла была записана клиентом
  m_semProducedPart = HandleOwner(new HANDLE(CreateSemaphoreA(NULL, 0, 1, common::semProducedPart.c_str())));
  if (*m_semProducedPart == NULL)
  {
    throw std::logic_error("m_semProducedPart is NULL");
  }
}

//***********************************************************************************************************************************************************

void si::sharedMemoryClient::run()
{ 
  // собственно, открываем разделяемую память
  m_handle = HandleOwner(new HANDLE(OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, common::ramMappingFileName.c_str())), handleLiberator());
  if (*m_handle == NULL)
  {
    throw std::logic_error("m_handle not opened at the side of client");
  }

  static std::vector<unsigned char> fileByteContent; // данные файла, которые хотим передать
  fileByteContent.reserve(m_size);
  // запрашиваем у пользователя файлы для передачи
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
    // запрашиваем доступ к разделяемой памяти на изменение оглавления
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
        // отцепляем память
        dm.forceDetach();
        // делаем её доступной на запись и чтение оглавления
        ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
        
        // ждём когда память будет потреблена потоком сервером и отдана для записи и чтения оглавления
        HANDLE handlesForWait[2];
        handlesForWait[0] = *m_semMemoryAccessible;
        handlesForWait[1] = *m_semConsumed;
        WaitForMultipleObjects(2, handlesForWait, true, INFINITE);
        // переподключаемся к памяти
        dm.forceAttach();
      }
      // записываем файл в дамп
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
          // отцепляем память
          dm.forceDetach();
          // даём право поработать с памятью серверу и/или другим клиентам
          ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
          // после записи 1-ого транша сообщаем серверу, что можно заводить поток для работы с файлом
          if (k == 0)
          {
            ReleaseSemaphore(*m_semProduced, 1, NULL);
          }
          // даём команду серверу о том, что был произведён транш очередной порции файла
          ReleaseSemaphore(*m_semProducedPart, 1, NULL);
          // ждём, когда сервер обработает этот транш
          WaitForSingleObject(*m_semConsumedPart, INFINITE);
          // блолкируем память на запись
          WaitForSingleObject(*m_semMemoryAccessible, INFINITE);
          // мапим память в адресное пространство клиента для последующего транша
          dm.forceAttach();
          ptr += dumpFileCap;
        }
      }
    }
    // Сигнализируем о том, что можно писать в дамп а так же читать оглавление (структуру)
    ReleaseSemaphore(*m_semMemoryAccessible, 1, NULL);
    if (not fileIsLarge)
    {
      // сигнализируем о том, что в разделяемую память был произведён пуш
      ReleaseSemaphore(*m_semProduced, 1, NULL);
    }
  }
}

//auto memStart = holder.get();

//rawMemoryProcessor memProc(memStart, m_size);
//while (not memProc.push(fileByteContent.data(), fileByteContent.size()))
//{
//  // отстёгиваем разделяемую память, чтобы ей могли воспользоваться другие процессы/потоки
//  // такой детач происходит при разрушении memoryHolder, если это необходимо
//  holder.forceDetach();
//  // захватываем семафор m_semConsumed - он должен будет освободиться серверм после извлечения данных из разделяемой памяти
//  // этот семафор захватывается только если кому-то из клиентов не хватает места для пуша в дамп
//  // по умолчанию этот семафор сигналит, так что захватываем
//  WaitForSingleObject(*m_semConsumed, INFINITE);
//  // естественно, освобождаем лоступ к разделяемой памяти
//  ReleaseSemaphore(*m_semMemoryPrepared, 1, NULL);
//  // ждём события когда память будет не занята и сервер сбросит семафор m_semConsumed. WaitForMultipleObjects - чтобы нас никто не опередил пушем
//  HANDLE handlesForWait[2];
//  handlesForWait[0] = *m_semMemoryPrepared;
//  handlesForWait[1] = *m_semConsumed;
//  WaitForMultipleObjects(2, handlesForWait, true, INFINITE);
//  // итак, дождались, что сервер срезал дамп - попытаемся подключиться к нему снова для новой попытки
//  holder.forceAttach();
//  // освобождаем семафор для возможной следующей итерации и/или для других клиентов
//  ReleaseSemaphore(*m_semConsumed, 1, NULL);
//}
