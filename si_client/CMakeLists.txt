# CMakeList.txt: проект CMake для si_client_server_filetransferer; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#

# Добавьте источник в исполняемый файл этого проекта.
add_executable (
	siClient
	"../dumpManager.cpp"
	"../projectClient.cpp"
	"../rawMemoryProcessor.cpp"
	"../sharedMemoryClient.cpp"
	"../sharedMemoryClientServerBase.cpp"
	"../common.h"
	"../dumpManager.h"
	"../rawMemoryProcessor.h"
	"../sharedMemoryClient.h"
	"../sharedMemoryClientServerBase.h"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET siClient PROPERTY CXX_STANDARD 20)
endif()

# TODO: Добавьте тесты и целевые объекты, если это необходимо.
