// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RIEGELI_BYTES_VARINT_READING_H_
#define RIEGELI_BYTES_VARINT_READING_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/base/optimization.h"
#include "absl/types/optional.h"
#include "riegeli/bytes/reader.h"
#include "riegeli/bytes/varint.h"

namespace riegeli {

// Unless stated otherwise, reading a varint tolerates representations
// which are not the shortest, but rejects representations longer than
// `kMaxLengthVarint{32,64}` bytes or with bits set outside the range of
// possible values.
//
// Warning: unless stated otherwise, reading a varint may read ahead more than
// eventually needed. If reading ahead only as much as needed is required, e.g.
// when communicating with another process, use `StreamingReadVarint{32,64}()`
// instead. It is slower.

// Reads a varint.
//
// Returns `absl::nullopt` on failure, with the current position unchanged.
//
// Warning: the proto library writes values of type `int32` (not `sint32`) by
// casting them to `uint64`, not `uint32` (negative values take 10 bytes, not
// 5), hence they must be read with `ReadVarint64()`, not `ReadVarint32()`, if
// negative values are possible.
absl::optional<uint32_t> ReadVarint32(Reader* src);
absl::optional<uint64_t> ReadVarint64(Reader* src);

// Reads a varint.
//
// Accepts only the canonical representation, i.e. the shortest: rejecting a
// trailing zero byte, except for 0 itself.
//
// Returns `absl::nullopt` on failure, with the current position unchanged.
absl::optional<uint32_t> ReadCanonicalVarint32(Reader* src);
absl::optional<uint64_t> ReadCanonicalVarint64(Reader* src);

// Reads a varint.
//
// Reads ahead only as much as needed, which can be required e.g. when
// communicating with another process. This is slower than
// `ReadVarint{32,64}()`.
//
// Returns `absl::nullopt` on failure, with the current position unchanged.
absl::optional<uint32_t> StreamingReadVarint32(Reader* src);
absl::optional<uint64_t> StreamingReadVarint64(Reader* src);

// Reads a varint from an array.
//
// Returns `absl::nullopt` on failure.

template <typename T>
struct ReadFromStringResult {
  T value;             // Parsed value.
  const char* cursor;  // Pointer to remaining data.
};

absl::optional<ReadFromStringResult<uint32_t>> ReadVarint32(const char* src,
                                                            const char* limit);
absl::optional<ReadFromStringResult<uint64_t>> ReadVarint64(const char* src,
                                                            const char* limit);

// Copies a varint to an array.
//
// Writes up to `kMaxLengthVarint{32,64}` bytes to `dest[]`.
//
// Returns the updated `dest` after the copied value, or `absl::nullopt` on
// failure, with the current position unchanged.
absl::optional<char*> CopyVarint32(Reader* src, char* dest);
absl::optional<char*> CopyVarint64(Reader* src, char* dest);

// Copies a varint from an array to an array.
//
// Writes up to `kMaxLengthVarint{32,64}` bytes to `dest[]`.
//
// Returns the updated `dest` after the copied value, or `absl::nullopt` on
// failure.
absl::optional<ReadFromStringResult<char*>> CopyVarint32(const char* src,
                                                         const char* limit,
                                                         char* dest);
absl::optional<ReadFromStringResult<char*>> CopyVarint64(const char* src,
                                                         const char* limit,
                                                         char* dest);

// Implementation details follow.

inline absl::optional<uint32_t> ReadVarint32(Reader* src) {
  src->Pull(kMaxLengthVarint32);
  const absl::optional<ReadFromStringResult<uint32_t>> result =
      ReadVarint32(src->cursor(), src->limit());
  if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
  src->set_cursor(result->cursor);
  return result->value;
}

inline absl::optional<uint64_t> ReadVarint64(Reader* src) {
  src->Pull(kMaxLengthVarint64);
  const absl::optional<ReadFromStringResult<uint64_t>> result =
      ReadVarint64(src->cursor(), src->limit());
  if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
  src->set_cursor(result->cursor);
  return result->value;
}

inline absl::optional<uint32_t> ReadCanonicalVarint32(Reader* src) {
  src->Pull(kMaxLengthVarint32);
  if (ABSL_PREDICT_FALSE(src->cursor() == src->limit())) return absl::nullopt;
  const uint8_t first_byte = static_cast<uint8_t>(*src->cursor());
  if ((first_byte & 0x80) == 0) {
    // Any byte with the highest bit clear is accepted as the only byte,
    // including 0 itself.
    src->move_cursor(size_t{1});
    return first_byte;
  }
  const absl::optional<ReadFromStringResult<uint32_t>> result =
      ReadVarint32(src->cursor(), src->limit());
  if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
  if (ABSL_PREDICT_FALSE(result->cursor[-1] == 0)) return absl::nullopt;
  src->set_cursor(result->cursor);
  return result->value;
}

inline absl::optional<uint64_t> ReadCanonicalVarint64(Reader* src) {
  src->Pull(kMaxLengthVarint64);
  if (ABSL_PREDICT_FALSE(src->cursor() == src->limit())) return absl::nullopt;
  const uint8_t first_byte = static_cast<uint8_t>(*src->cursor());
  if ((first_byte & 0x80) == 0) {
    // Any byte with the highest bit clear is accepted as the only byte,
    // including 0 itself.
    src->move_cursor(size_t{1});
    return first_byte;
  }
  const absl::optional<ReadFromStringResult<uint64_t>> result =
      ReadVarint64(src->cursor(), src->limit());
  if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
  if (ABSL_PREDICT_FALSE(result->cursor[-1] == 0)) return absl::nullopt;
  src->set_cursor(result->cursor);
  return result->value;
}

namespace internal {

absl::optional<uint32_t> StreamingReadVarint32Slow(Reader* src);
absl::optional<uint64_t> StreamingReadVarint64Slow(Reader* src);

}  // namespace internal

inline absl::optional<uint32_t> StreamingReadVarint32(Reader* src) {
  if (ABSL_PREDICT_FALSE(!src->Pull(1, kMaxLengthVarint32))) {
    return absl::nullopt;
  }
  if (ABSL_PREDICT_TRUE(src->available() >= kMaxLengthVarint32)) {
    const absl::optional<ReadFromStringResult<uint32_t>> result =
        ReadVarint32(src->cursor(), src->limit());
    if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
    src->set_cursor(result->cursor);
    return result->value;
  }
  return internal::StreamingReadVarint32Slow(src);
}

inline absl::optional<uint64_t> StreamingReadVarint64(Reader* src) {
  if (ABSL_PREDICT_FALSE(!src->Pull(1, kMaxLengthVarint64))) {
    return absl::nullopt;
  }
  if (ABSL_PREDICT_TRUE(src->available() >= kMaxLengthVarint64)) {
    const absl::optional<ReadFromStringResult<uint64_t>> result =
        ReadVarint64(src->cursor(), src->limit());
    if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
    src->set_cursor(result->cursor);
    return result->value;
  }
  return internal::StreamingReadVarint64Slow(src);
}

inline absl::optional<ReadFromStringResult<uint32_t>> ReadVarint32(
    const char* src, const char* limit) {
  uint32_t result = 0;
  size_t shift = 0;
  uint8_t byte;
  do {
    if (ABSL_PREDICT_FALSE(src == limit)) return absl::nullopt;
    byte = static_cast<uint8_t>(*src++);
    result |= (uint32_t{byte} & 0x7f) << shift;
    if (ABSL_PREDICT_FALSE(shift == (kMaxLengthVarint32 - 1) * 7)) {
      // Last possible byte.
      if (ABSL_PREDICT_FALSE(
              byte >= uint8_t{1} << (32 - (kMaxLengthVarint32 - 1) * 7))) {
        // The representation is longer than `kMaxLengthVarint32`
        // or the represented value does not fit in `uint32_t`.
        return absl::nullopt;
      }
      break;
    }
    shift += 7;
  } while ((byte & 0x80) != 0);
  return ReadFromStringResult<uint32_t>{result, src};
}

inline absl::optional<ReadFromStringResult<uint64_t>> ReadVarint64(
    const char* src, const char* limit) {
  uint64_t result = 0;
  size_t shift = 0;
  uint8_t byte;
  do {
    if (ABSL_PREDICT_FALSE(src == limit)) return absl::nullopt;
    byte = static_cast<uint8_t>(*src++);
    result |= (uint64_t{byte} & 0x7f) << shift;
    if (ABSL_PREDICT_FALSE(shift == (kMaxLengthVarint64 - 1) * 7)) {
      // Last possible byte.
      if (ABSL_PREDICT_FALSE(
              byte >= uint8_t{1} << (64 - (kMaxLengthVarint64 - 1) * 7))) {
        // The representation is longer than `kMaxLengthVarint64`
        // or the represented value does not fit in `uint64_t`.
        return absl::nullopt;
      }
      break;
    }
    shift += 7;
  } while ((byte & 0x80) != 0);
  return ReadFromStringResult<uint64_t>{result, src};
}

inline absl::optional<char*> CopyVarint32(Reader* src, char* dest) {
  src->Pull(kMaxLengthVarint32);
  const absl::optional<ReadFromStringResult<char*>> result =
      CopyVarint32(src->cursor(), src->limit(), dest);
  if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
  src->set_cursor(result->cursor);
  return result->value;
}

inline absl::optional<char*> CopyVarint64(Reader* src, char* dest) {
  src->Pull(kMaxLengthVarint64);
  const absl::optional<ReadFromStringResult<char*>> result =
      CopyVarint64(src->cursor(), src->limit(), dest);
  if (ABSL_PREDICT_FALSE(result == absl::nullopt)) return absl::nullopt;
  src->set_cursor(result->cursor);
  return result->value;
}

inline absl::optional<ReadFromStringResult<char*>> CopyVarint32(
    const char* src, const char* limit, char* dest) {
  int remaining = kMaxLengthVarint32;
  uint8_t byte;
  do {
    if (ABSL_PREDICT_FALSE(src == limit)) return absl::nullopt;
    byte = static_cast<uint8_t>(*src++);
    *dest++ = static_cast<char>(byte);
    if (ABSL_PREDICT_FALSE(--remaining == 0)) {
      // Last possible byte.
      if (ABSL_PREDICT_FALSE(
              byte >= uint8_t{1} << (32 - (kMaxLengthVarint32 - 1) * 7))) {
        // The representation is longer than `kMaxLengthVarint32`
        // or the represented value does not fit in `uint32_t`.
        return absl::nullopt;
      }
      break;
    }
  } while ((byte & 0x80) != 0);
  return ReadFromStringResult<char*>{dest, src};
}

inline absl::optional<ReadFromStringResult<char*>> CopyVarint64(
    const char* src, const char* limit, char* dest) {
  int remaining = kMaxLengthVarint64;
  uint8_t byte;
  do {
    if (ABSL_PREDICT_FALSE(src == limit)) return absl::nullopt;
    byte = static_cast<uint8_t>(*src++);
    *dest++ = static_cast<char>(byte);
    if (ABSL_PREDICT_FALSE(--remaining == 0)) {
      // Last possible byte.
      if (ABSL_PREDICT_FALSE(
              byte >= uint8_t{1} << (64 - (kMaxLengthVarint64 - 1) * 7))) {
        // The representation is longer than `kMaxLengthVarint64`
        // or the represented value does not fit in `uint64_t`.
        return absl::nullopt;
      }
      break;
    }
  } while ((byte & 0x80) != 0);
  return ReadFromStringResult<char*>{dest, src};
}

}  // namespace riegeli

#endif  // RIEGELI_BYTES_VARINT_READING_H_
