// Copyright 2019 Google LLC
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

#include "riegeli/bytes/snappy_writer.h"

#include <stddef.h>

#include <limits>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "riegeli/base/base.h"
#include "riegeli/base/chain.h"
#include "riegeli/base/status.h"
#include "riegeli/bytes/chain_reader.h"
#include "riegeli/bytes/snappy_streams.h"
#include "riegeli/bytes/writer.h"
#include "snappy.h"

namespace riegeli {

// Before C++17 if a constexpr static data member is ODR-used, its definition at
// namespace scope is required. Since C++17 these definitions are deprecated:
// http://en.cppreference.com/w/cpp/language/static
#if __cplusplus < 201703
constexpr size_t SnappyWriterBase::kBlockSize;
#endif

void SnappyWriterBase::Done() {
  if (ABSL_PREDICT_TRUE(healthy())) SyncBuffer();
  Writer::Done();
  if (ABSL_PREDICT_TRUE(healthy())) {
    Writer* const dest = dest_writer();
    {
      absl::Status status = SnappyCompress<ChainReader<>>(
          std::forward_as_tuple(&uncompressed_), dest);
      if (ABSL_PREDICT_FALSE(!status.ok())) {
        Fail(std::move(status));
      }
    }
  }
}

bool SnappyWriterBase::Fail(absl::Status status) {
  RIEGELI_ASSERT(!status.ok())
      << "Failed precondition of Object::Fail(): status not failed";
  RIEGELI_ASSERT(!closed())
      << "Failed precondition of Object::Fail(): Object closed";
  return FailWithoutAnnotation(
      Annotate(status, absl::StrCat("at uncompressed byte ", pos())));
}

bool SnappyWriterBase::PushSlow(size_t min_length, size_t recommended_length) {
  RIEGELI_ASSERT_GT(min_length, available())
      << "Failed precondition of Writer::PushSlow(): "
         "length too small, use Push() instead";
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  if (ABSL_PREDICT_FALSE(min_length > std::numeric_limits<size_t>::max() -
                                          uncompressed_.size())) {
    return FailOverflow();
  }
  SyncBuffer();
  MakeBuffer(min_length);
  return true;
}

bool SnappyWriterBase::WriteSlow(const Chain& src) {
  RIEGELI_ASSERT_GT(src.size(), UnsignedMin(available(), kMaxBytesToCopy))
      << "Failed precondition of Writer::WriteSlow(Chain): "
         "length too small, use Write(Chain) instead";
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  if (ABSL_PREDICT_FALSE(src.size() > std::numeric_limits<size_t>::max() -
                                          IntCast<size_t>(pos()))) {
    return FailOverflow();
  }
  SyncBuffer();
  move_start_pos(src.size());
  uncompressed_.Append(src, options_);
  MakeBuffer();
  return true;
}

bool SnappyWriterBase::WriteSlow(Chain&& src) {
  RIEGELI_ASSERT_GT(src.size(), UnsignedMin(available(), kMaxBytesToCopy))
      << "Failed precondition of Writer::WriteSlow(Chain&&): "
         "length too small, use Write(Chain&&) instead";
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  if (ABSL_PREDICT_FALSE(src.size() > std::numeric_limits<size_t>::max() -
                                          IntCast<size_t>(pos()))) {
    return FailOverflow();
  }
  SyncBuffer();
  move_start_pos(src.size());
  uncompressed_.Append(std::move(src), options_);
  MakeBuffer();
  return true;
}

bool SnappyWriterBase::WriteSlow(const absl::Cord& src) {
  RIEGELI_ASSERT_GT(src.size(), UnsignedMin(available(), kMaxBytesToCopy))
      << "Failed precondition of Writer::WriteSlow(Cord): "
         "length too small, use Write(Cord) instead";
  if (ABSL_PREDICT_FALSE(!healthy())) return false;
  if (ABSL_PREDICT_FALSE(src.size() > std::numeric_limits<size_t>::max() -
                                          IntCast<size_t>(pos()))) {
    return FailOverflow();
  }
  SyncBuffer();
  move_start_pos(src.size());
  uncompressed_.Append(src, options_);
  MakeBuffer();
  return true;
}

bool SnappyWriterBase::Flush(FlushType flush_type) { return healthy(); }

inline void SnappyWriterBase::SyncBuffer() {
  set_start_pos(pos());
  uncompressed_.RemoveSuffix(available());
  set_buffer();
}

inline void SnappyWriterBase::MakeBuffer(size_t min_length) {
  const absl::Span<char> buffer = uncompressed_.AppendFixedBuffer(
      BufferLength(
          min_length,
          min_length + -(uncompressed_.size() + min_length) % kBlockSize,
          options_.size_hint(), uncompressed_.size()),
      options_);
  set_buffer(buffer.data(), buffer.size());
}

namespace internal {

absl::Status SnappyCompressImpl(Reader* src, Writer* dest) {
  ReaderSnappySource source(src);
  WriterSnappySink sink(dest);
  snappy::Compress(&source, &sink);
  if (ABSL_PREDICT_FALSE(!dest->healthy())) return dest->status();
  if (ABSL_PREDICT_FALSE(!src->healthy())) return src->status();
  return absl::OkStatus();
}

}  // namespace internal

size_t SnappyMaxCompressedSize(size_t uncompressed_size) {
  return snappy::MaxCompressedLength(uncompressed_size);
}

}  // namespace riegeli
