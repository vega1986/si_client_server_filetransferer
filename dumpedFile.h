#pragma once

namespace si
{
  struct dumpedFile
  {
    size_t shift_fc = 0; // shift of data: file capacity (size_t)
    size_t shift_fs = 0; // shift of data: file size (actual employed part of capacity) (size_t)
    size_t shift_expired = 0; // shift of expired flag (bool)
    size_t shift_wip = 0; // shift of wip flag (bool)
    size_t shift_ptr = 0; // shift of memory start pointer

    bool isNull() const
    {
      return (shift_fc == 0);
    }

  };
}
