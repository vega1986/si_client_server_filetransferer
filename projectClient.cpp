#include "sharedMemoryClient.h"
#include "common.h"

int main()
{
  si::sharedMemoryClient client(si::common::ramMappingFileName, si::common::ramMappingFileSize);
  client.run();
  return 0;
}