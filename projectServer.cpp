#include "sharedMemoryServer.h"
#include "common.h"

int main()
{
  si::sharedMemoryServer server(si::common::ramMappingFileName, si::common::ramMappingFileSize);
  server.run();
  return 0;
}