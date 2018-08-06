// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_SCANNER_CHARACTER_STREAMS_H_
#define V8_PARSING_SCANNER_CHARACTER_STREAMS_H_

#include <algorithm>

#include "include/v8.h"  // for v8::ScriptCompiler
#include "src/globals.h"
#include "src/unicode-decoder.h"


#include <iostream>

namespace v8 {
namespace internal {

template <typename T>
class Handle;
template <typename Char>
class CharacterStream;
class RuntimeCallStats;
class String;

template <typename Char>
struct Range {
  using range_type = const Char*;

  Range() = default;
  Range(const Char* start, const Char* end) : start_(start), end_(end) {}

  const Char* start_;
  const Char* end_;

  const Char *begin() const { return start_; }
  const Char *end() const { return end_; }

  inline bool is_one_byte() const {
    if (checked_one_byte_) {
      return is_one_byte_;
    }
    checked_one_byte_ = true;

    // std::cout << "check\n";
    // TODO: do we need magic constants? and what should be their value
    if (length() < 256) {
      is_one_byte_ = has_only_one_byte_chars();
      return is_one_byte_;
    }

    size_t Step = 1024;//65536;//1024;// 65536;
    auto current_start = begin();
    const Char* next_end = current_start;

    do {
      next_end = next_end + Step;
      if (next_end > end()) {
        // If we reach the end return result of last subrange.
        is_one_byte_ = sub_range_has_only_one_byte_chars(current_start, end());
        // is_one_byte_ = sub_range_has_only_one_byte_chars(current_start, end());
        return is_one_byte_;
      }
      // Repeat until we find a subrange that has a multi byte
    } while (sub_range_has_only_one_byte_chars(current_start, next_end));
    return false;
  }

  inline bool has_surrogate() const {
    if (checked_surrogate_) {
      return has_surrogate_;
    }
    checked_surrogate_ = true;

    for (auto iter = begin(); iter != end(); ++iter) {
      if (*iter > static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
        has_surrogate_ = true;
        return has_surrogate_;
      }
    }

    return false;
  }

  inline void set_has_surroagte() {
    checked_surrogate_ = true;
    has_surrogate_ = true;
  }

  size_t length() const { return static_cast<size_t>(end_ - start_); }
  bool unaligned_start() const {
    return reinterpret_cast<intptr_t>(start_) % sizeof(Char) == 1;
  }

private:
  // Use local bools to cache state of the range
  mutable bool is_one_byte_ = false;
  mutable bool checked_one_byte_ = false;
  mutable bool has_surrogate_ = false;
  mutable bool checked_surrogate_ = false;

  bool sub_range_has_only_one_byte_chars(const Char* begin,
                                         const Char* end) const {
    int res = 0;
    constexpr int32_t mask = ~static_cast<int32_t>(0xff);
    for (auto iter = begin; iter != end; ++iter) {
      res ^= (static_cast<int32_t>(*iter) & mask);
    }
    if (res != 0) {
      return false;
    }
    return true;
  }

  bool has_only_one_byte_chars() const {
    for (auto iter = begin(); iter != end(); ++iter) {
      if (static_cast<uc32>(*iter) <=
          static_cast<uc32>(unibrow::Latin1::kMaxChar)) {
        return false;
      }
    }
    return true;
  }
};

class V8_EXPORT_PRIVATE ScannerStream {
 public:
  static const uc32 kEndOfInput = -1;

  static ScannerStream* For(Isolate* isolate, Handle<String> data);
  static ScannerStream* For(Isolate* isolate, Handle<String> data,
                            int start_pos, int end_pos);
  static ScannerStream* For(ScriptCompiler::ExternalSourceStream* source_stream,
                            ScriptCompiler::StreamedSource::Encoding encoding,
                            RuntimeCallStats* stats);

  // For testing:
  static std::unique_ptr<CharacterStream<uint16_t>> ForTesting(
      const char* data);
  static std::unique_ptr<CharacterStream<uint16_t>> ForTesting(const char* data,
                                                               size_t length);

  // Returns true if the stream could access the V8 heap after construction.
  virtual bool can_access_heap() = 0;
  virtual uc32 Advance() = 0;
  virtual void Seek(size_t pos) = 0;
  virtual size_t pos() const = 0;
  virtual void Back() = 0;
  virtual bool isBuffered() const { return true; }

  virtual ~ScannerStream() {}
};

template <typename Char>
class CharacterStream : public ScannerStream {
 public:
  // Returns and advances past the next UTF-16 code unit in the input
  // stream. If there are no more code units it returns kEndOfInput.
  inline uc32 Advance() final {
    if (V8_LIKELY(buffer_cursor_ < buffer_end_)) {
      return static_cast<uc32>(*(buffer_cursor_++));
    } else if (ReadBlockChecked()) {
      return static_cast<uc32>(*(buffer_cursor_++));
    } else {
      // Note: currently the following increment is necessary to avoid a
      // parser problem! The scanner treats the final kEndOfInput as
      // a code unit with a position, and does math relative to that
      // position.
      buffer_cursor_++;
      return kEndOfInput;
    }
  }

  // Returns and advances past the next UTF-16 code unit in the input stream
  // that meets the checks requirement. If there are no more code units it
  // returns kEndOfInput.
  template <typename FunctionType>
  V8_INLINE uc32 AdvanceUntil(FunctionType check) {
    while (true) {
      auto next_cursor_pos =
          std::find_if(buffer_cursor_, buffer_end_, [&check](Char raw_c0) {
            uc32 c0 = static_cast<uc32>(raw_c0);
            return check(c0);
          });

      if (next_cursor_pos == buffer_end_) {
        buffer_cursor_ = buffer_end_;
        if (!ReadBlockChecked()) {
          buffer_cursor_++;
          return kEndOfInput;
        }
      } else {
        buffer_cursor_ = next_cursor_pos + 1;
        return static_cast<uc32>(*next_cursor_pos);
      }
    }
  }

  template <typename FunctionType>
  V8_INLINE std::pair<uc32, Range<uint16_t>> AdvanceUntilRange(
      FunctionType check) {
    Range<uint16_t> R;
    R.start_ = buffer_cursor_;
    // bool saw_surrogate = false;
    while (true) {
      // TODO (sattlerf): remove _
      auto next_cursor_pos = std::find_if(
          buffer_cursor_, buffer_end_,
          [&check](uint16_t raw_c0_) {
            uc32 c0_ = static_cast<uc32>(raw_c0_);
            // saw_surrogate |=
            //     (c0_ >
            //      static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode));
            return check(c0_);
          });

      // if (saw_surrogate) {
      //   R.set_has_surroagte();
      // }

      if (next_cursor_pos == buffer_end_) {
        buffer_cursor_ = buffer_end_;
        if (!ReadBlockChecked()) {
          buffer_cursor_++;
          R.end_ = buffer_cursor_;
          return {kEndOfInput, R};
        }
      } else {
        buffer_cursor_ = next_cursor_pos + 1;
        R.end_ = next_cursor_pos;
        return {static_cast<uc32>(*next_cursor_pos), R};
      }
    }
  }

  // Go back one by one character in the input stream.
  // This undoes the most recent Advance().
  inline void Back() final {
    // The common case - if the previous character is within
    // buffer_start_ .. buffer_end_ will be handles locally.
    // Otherwise, a new block is requested.
    if (V8_LIKELY(buffer_cursor_ > buffer_start_)) {
      buffer_cursor_--;
    } else {
      ReadBlockAt(pos() - 1);
    }
  }

  // Go back one by two characters in the input stream. (This is the same as
  // calling Back() twice. But Back() may - in some instances - do substantial
  // work. Back2() guarantees this work will be done only once.)
  inline void Back2() {
    if (V8_LIKELY(buffer_cursor_ - 2 >= buffer_start_)) {
      buffer_cursor_ -= 2;
    } else {
      ReadBlockAt(pos() - 2);
    }
  }

  inline size_t pos() const final {
    return buffer_pos_ + (buffer_cursor_ - buffer_start_);
  }

  inline void Seek(size_t pos) final {
    if (V8_LIKELY(pos >= buffer_pos_ &&
                  pos < (buffer_pos_ + (buffer_end_ - buffer_start_)))) {
      buffer_cursor_ = buffer_start_ + (pos - buffer_pos_);
    } else {
      ReadBlockAt(pos);
    }
  }

  // Returns true if the stream could access the V8 heap after construction.
  virtual bool can_access_heap() = 0;

 protected:
  CharacterStream(const uint16_t* buffer_start, const uint16_t* buffer_cursor,
                  const uint16_t* buffer_end, size_t buffer_pos)
      : buffer_start_(buffer_start),
        buffer_cursor_(buffer_cursor),
        buffer_end_(buffer_end),
        buffer_pos_(buffer_pos) {}
  CharacterStream() : CharacterStream(nullptr, nullptr, nullptr, 0) {}

  bool ReadBlockChecked() {
    size_t position = pos();
    USE(position);
    bool success = ReadBlock();

    // Post-conditions: 1, We should always be at the right position.
    //                  2, Cursor should be inside the buffer.
    //                  3, We should have more characters available iff success.
    DCHECK_EQ(pos(), position);
    DCHECK_LE(buffer_cursor_, buffer_end_);
    DCHECK_LE(buffer_start_, buffer_cursor_);
    DCHECK_EQ(success, buffer_cursor_ < buffer_end_);
    return success;
  }

  void ReadBlockAt(size_t new_pos) {
    // The callers of this method (Back/Back2/Seek) should handle the easy
    // case (seeking within the current buffer), and we should only get here
    // if we actually require new data.
    // (This is really an efficiency check, not a correctness invariant.)
    DCHECK(new_pos < buffer_pos_ ||
           new_pos >= buffer_pos_ + (buffer_end_ - buffer_start_));

    // Change pos() to point to new_pos.
    buffer_pos_ = new_pos;
    buffer_cursor_ = buffer_start_;
    DCHECK_EQ(pos(), new_pos);
    ReadBlockChecked();
  }

  // Read more data, and update buffer_*_ to point to it.
  // Returns true if more data was available.
  //
  // ReadBlock() may modify any of the buffer_*_ members, but must sure that
  // the result of pos() remains unaffected.
  //
  // Examples:
  // - a stream could either fill a separate buffer. Then buffer_start_ and
  //   buffer_cursor_ would point to the beginning of the buffer, and
  //   buffer_pos would be the old pos().
  // - a stream with existing buffer chunks would set buffer_start_ and
  //   buffer_end_ to cover the full chunk, and then buffer_cursor_ would
  //   point into the middle of the buffer, while buffer_pos_ would describe
  //   the start of the buffer.
  virtual bool ReadBlock() = 0;

  const Char* buffer_start_;
  const Char* buffer_cursor_;
  const Char* buffer_end_;
  size_t buffer_pos_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_SCANNER_CHARACTER_STREAMS_H_
