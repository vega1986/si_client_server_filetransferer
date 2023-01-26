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
		HandleOwner m_semMemoryAccessible; // можем писать в дамп и извлекать информацию о сигнатурах (оглавление, журнал)
		
		
		HandleOwner m_semConsumed;         // сигнализирует о том, что память была извлечена (занимаемая память уменьшилась),
		                                   // в сигнальном состоянии при создании
		HandleOwner m_semProduced;         // сигнализирует о том, что файл был помещён в память

		HandleOwner m_semConsumedPart;    // Кусок большого файла был считан потоком сервера
		HandleOwner m_semProducedPart;    // Кусок большого файла был помещён клиентом в дамп
	};

}
