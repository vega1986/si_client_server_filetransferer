# CMakeList.txt: проект CMake для si_client_server_filetransferer; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#

# Добавьте источник в исполняемый файл этого проекта.
add_executable (siServer
	"../dumpedFile.cpp"
	"../dumpManager.cpp"
	"../projectServer.cpp"
	"../rawMemoryProcessor.cpp"
	"../sharedMemoryClientServerBase.cpp"
	"../sharedMemoryServer.cpp"
	"../common.h"
	"../dumpedFile.h"
	"../dumpManager.h"
	"../rawMemoryProcessor.h"
	"../sharedMemoryClientServerBase.h"
	"../sharedMemoryServer.h"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET siServer PROPERTY CXX_STANDARD 20)
endif()

# TODO: Добавьте тесты и целевые объекты, если это необходимо.
