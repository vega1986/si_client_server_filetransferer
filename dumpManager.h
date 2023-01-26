#pragma once
#include <stdexcept>
#include <tuple>
#include <Windows.h>

#include "dumpedFile.h"

namespace si
{
  
  class dumpManager
  {
  public:

    template<typename T>
    T getValue(size_t shift, T defaultValue)
    {
      T value(defaultValue);
      auto pointerToValue = m_ptr;
      pointerToValue += shift;
      memcpy(& value, pointerToValue, sizeof(T));
      return value;
    }

    template<typename T>
    void setValue(size_t shift, T value)
    {
      auto addressForWriting = m_ptr;
      addressForWriting += shift;
      memcpy(addressForWriting, & value, sizeof(T));
    }
  private:
    unsigned char* getPtr(size_t shift)
    {
      auto ptr = m_ptr;
      ptr += shift;
      return ptr;
    }

  public:
    dumpManager (const HANDLE handle, const size_t size, const bool readonly = false);
    
    void forceAttach ();

    void forceDetach ();

    void initialize ();

    void free ();

    size_t getFilesCount();

    void setFilesCount(const size_t newNf);

    dumpedFile acquireFile(const size_t fileSize);

    void killFile(const dumpedFile signature);

    void optimaizeExpired();

    void writeFile(const dumpedFile signature, unsigned char * ptrData, size_t nBytes);

    void readFile(const dumpedFile signature, unsigned char* ptrData, size_t nBytes);

    dumpedFile getPrepared();

  private:
    const         HANDLE m_handle   = 0;       // 
    const         size_t m_size     = 0;       // размер дампа в байтах
    unsigned char *      m_ptr      = nullptr; // указатель на начало дампа
    const bool           m_readOnly = false;
  };

}