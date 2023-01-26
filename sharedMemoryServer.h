#pragma once

#include <string>
#include <list>

#include "sharedMemoryClientServerBase.h"
#include "dumpedFile.h"

namespace si
{
	struct TheConsumerData
	{
		HANDLE shMemHandle = 0;
		size_t memSize = 0;
		HANDLE semConsumed = 0;
		HANDLE semConsumedPart = 0;
		HANDLE semProducedPart = 0;
		HANDLE semMemoryAccessible = 0;
		size_t fileId = 0;
		dumpedFile df{};
	};

	DWORD WINAPI TheConsumerExecutor(void* ptrData);

	class sharedMemoryServer : private sharedMemoryClientServerBase
	{
	public:
		sharedMemoryServer(const std::string& sharedMemoryName, const size_t sharedMemorySize);
		~sharedMemoryServer()
		{}
		void run();
	private:
		std::list<std::pair<HANDLE, TheConsumerData>> consumers;
	};
}
