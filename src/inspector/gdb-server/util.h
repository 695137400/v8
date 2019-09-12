// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_GDB_SERVER_UTIL_H_
#define V8_INSPECTOR_GDB_SERVER_UTIL_H_

#include <string>
#include <vector>

namespace v8_inspector {

// todo
#define GdbRemoteLog(level, msg)

typedef std::vector<std::string> stringvec;

// Convert from ASCII (0-9,a-f,A-F) to 4b unsigned or return
// false if the input char is unexpected.
bool NibbleToInt(char inChar, int* outInt);

// Convert from 0-15 to ASCII (0-9,a-f) or return false
// if the input is not a value from 0-15.
bool IntToNibble(int inInt, char* outChar);

// Convert a pair of nibbles to a value from 0-255 or return
// false if ethier input character is not a valid nibble.
bool NibblesToByte(const char* inStr, int* outInt);

stringvec StringSplit(const std::string& instr, const char* delim);

// Convert the memory pointed to by mem into hex.
std::string Mem2Hex(const uint8_t* mem, size_t count);
std::string Mem2Hex(const char* str);

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_GDB_SERVER_UTIL_H_
