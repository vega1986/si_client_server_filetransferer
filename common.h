#pragma once

#include <string>

namespace si
{
  struct common
  {
    static inline size_t ramMappingFileSize = 1024 * 256; // use 256K by default

    static inline std::string ramMappingFileName = "si_file_mapping"; // name of shred memory for employers

    static inline std::string semMemoryAccessibleName = "si_memory_accessible";

    static inline std::string semConsumed = "si_memory_consumed";
    static inline std::string semProduced = "si_memory_produced";

    static inline std::string semConsumedPart = "si_memory_consumed_part";
    static inline std::string semProducedPart = "si_memory_produced_part";

    static inline size_t semProducedMaxCount = 128;
  };
}