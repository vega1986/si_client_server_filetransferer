#pragma once
#include <string>
#include <memory>
#include <stdexcept>
#include <Windows.h>

namespace si
{
	struct handleLiberator
	{
		void operator()(HANDLE* ptr)
		{
			CloseHandle(*ptr);
			delete ptr;
		}
	};

	using HandleOwner = std::unique_ptr<HANDLE, handleLiberator>;

	struct sharedMemoryClientServerBase
	{
		sharedMemoryClientServerBase(const std::string& sharedMemoryName, const size_t sharedMemorySize)
			:
			m_name(sharedMemoryName),
			m_size(sharedMemorySize)
		{}

		const std::string m_name;
		const size_t m_size;

		HandleOwner m_handle;
		HandleOwner m_semMemoryAccessible; // ����� ������ � ���� � ��������� ���������� � ���������� (����������, ������)
		
		
		HandleOwner m_semConsumed;         // ������������� � ���, ��� ������ ���� ��������� (���������� ������ �����������),
		                                   // � ���������� ��������� ��� ��������
		HandleOwner m_semProduced;         // ������������� � ���, ��� ���� ��� ������� � ������

		HandleOwner m_semConsumedPart;    // ����� �������� ����� ��� ������ ������� �������
		HandleOwner m_semProducedPart;    // ����� �������� ����� ��� ������� �������� � ����
	};

}
