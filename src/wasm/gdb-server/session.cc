// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/gdb-server/session.h"
#include "src/wasm/gdb-server/packet.h"
#include "src/wasm/gdb-server/transport.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

Session::Session(TransportBase* transport)
    : io_(transport), connected_(true), ack_enabled_(true) {}

void Session::WaitForDebugStubEvent() { io_->WaitForDebugStubEvent(); }

bool Session::SignalThreadEvent() { return io_->SignalThreadEvent(); }

bool Session::IsDataAvailable() const { return io_->IsDataAvailable(); }

bool Session::IsConnected() const { return connected_; }

void Session::Disconnect() {
  io_->Disconnect();
  connected_ = false;
}

bool Session::GetChar(char* ch) {
  if (!io_->Read(ch, 1)) {
    Disconnect();
    return false;
  }

  return true;
}

bool Session::SendPacket(Packet* pkt) {
  char ch;

  do {
    if (!SendPacketOnly(pkt)) return false;

    // If ACKs are off, we are done.
    if (!ack_enabled_) break;

    // Otherwise, poll for '+'
    if (!GetChar(&ch)) return false;

    // Retry if we didn't get a '+'
  } while (ch != '+');

  return true;
}

bool Session::SendPacketOnly(Packet* pkt) {
  char chars[2];
  std::stringstream outstr;

  const char* ptr = pkt->GetPayload();
  size_t size = pkt->GetPayloadSize();

  // Signal start of response
  outstr << '$';

  char run_xsum = 0;

  // If there is a sequence, send as two nibble 8bit value + ':'
  int32_t seq;
  if (pkt->GetSequence(&seq)) {
    UInt8ToHex(seq, chars, false);
    outstr << chars[0];
    run_xsum += chars[0];
    outstr << chars[1];
    run_xsum += chars[1];

    outstr << ':';
    run_xsum += ':';
  }

  // Send the main payload
  for (size_t offs = 0; offs < size; ++offs) {
    outstr << ptr[offs];
    run_xsum += ptr[offs];
  }

  TRACE_GDB_REMOTE("TX %s\n", outstr.str().c_str());

  // Send XSUM as two nibble 8bit value preceeded by '#'
  outstr << '#';
  UInt8ToHex(run_xsum, chars, false);
  outstr << chars[0];
  outstr << chars[1];

  return io_->Write(outstr.str().data(),
                    static_cast<int32_t>(outstr.str().length()));
}

bool Session::GetPacket(Packet* pkt) {
  uint8_t run_xsum, fin_xsum;
  char ch;
  std::string in;

  // Toss characters until we see a start of command
  do {
    if (!GetChar(&ch)) return false;
    in += ch;
  } while (ch != '$');

retry:
  // Clear the stream
  pkt->Clear();

  // Prepare XSUM calc
  run_xsum = 0;
  fin_xsum = 0;

  // Stream in the characters
  while (true) {
    if (!GetChar(&ch)) return false;

    // If we see a '#' we must be done with the data
    if (ch == '#') break;

    in += ch;

    // If we see a '$' we must have missed the last cmd
    if (ch == '$') {
      TRACE_GDB_REMOTE("RX Missing $, retry.\n");
      goto retry;
    }
    // Keep a running XSUM
    run_xsum += ch;
    pkt->AddRawChar(ch);
  }

  // Get two Nibble XSUM
  char chars[2];
  if (!GetChar(&chars[0])) return false;
  if (!GetChar(&chars[1])) return false;
  if (!HexToUInt8(chars, &fin_xsum)) return false;

  TRACE_GDB_REMOTE("RX %s\n", in.c_str());

  pkt->ParseSequence();

  // If ACKs are off, we are done.
  if (!ack_enabled_) return true;

  // If the XSUMs don't match, signal bad packet
  if (fin_xsum == run_xsum) {
    char out[3] = {'+', 0, 0};
    int32_t seq;

    // If we have a sequence number
    if (pkt->GetSequence(&seq)) {
      // Respond with Sequence number
      UInt8ToHex(seq, &out[1], false);
      return io_->Write(out, 3);
    }
    return io_->Write(out, 1);
  } else {
    // Resend a bad XSUM and look for retransmit
    io_->Write("-", 1);

    TRACE_GDB_REMOTE("RX Bad XSUM, retry\n");
    goto retry;
  }

  return true;
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
