// fake_i2c.h — host-side scripted I2cBus implementation for the T13 host tests.
//
// Host-only test helper (uses std::vector/std::map freely; it is not firmware). It models a
// scripted register file per device address:
//   * a device only answers when addDevice() was called for its address (otherwise every
//     transfer fails, like a missing/NAKed part);
//   * reads return bytes from the register file, or the next entry of the per-register read
//     queue when one was queued with queueRead();
//   * unmapped registers read back as 0x00;
//   * every transfer is recorded so tests can assert exact register sequences.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "i2c_bus.h"

namespace esp_node {
namespace test {

class FakeI2c : public I2cBus {
 public:
  struct Op {
    bool is_write = false;
    uint8_t addr = 0;
    uint8_t reg = 0;
    std::vector<uint8_t> data;
    bool ok = false;
  };

  // ---- scripting -----------------------------------------------------------

  void addDevice(uint8_t addr) { present_[addr] = true; }
  void removeDevice(uint8_t addr) { present_[addr] = false; }

  void setReg(uint8_t addr, uint8_t reg, uint8_t value) {
    regs_[Key(addr, reg)] = std::vector<uint8_t>{value};
  }

  void setRegBytes(uint8_t addr, uint8_t reg, const std::vector<uint8_t>& bytes) {
    regs_[Key(addr, reg)] = bytes;
  }

  // Next read of (addr, reg) returns these bytes once; the queue falls back to the register
  // file when empty.
  void queueRead(uint8_t addr, uint8_t reg, const std::vector<uint8_t>& bytes) {
    queue_[Key(addr, reg)].push_back(bytes);
  }

  void setReadsFail(bool fail) { reads_fail_ = fail; }
  void setWritesFail(bool fail) { writes_fail_ = fail; }
  void failNextRead(uint8_t addr, uint8_t reg) { fail_next_read_[Key(addr, reg)] += 1; }

  // ---- observation ---------------------------------------------------------

  const std::vector<Op>& ops() const { return ops_; }
  void clearOps() { ops_.clear(); }

  // Single-byte register writes as (register, value) pairs, in order.
  std::vector<std::pair<uint8_t, uint8_t>> writtenRegs() const {
    std::vector<std::pair<uint8_t, uint8_t>> out;
    for (const Op& op : ops_) {
      if (op.is_write && op.ok && op.data.size() == 1) {
        out.emplace_back(op.reg, op.data[0]);
      }
    }
    return out;
  }

  // Command bytes of zero-length writes (MS5611 reset / conversion commands), in order.
  std::vector<uint8_t> commands() const {
    std::vector<uint8_t> out;
    for (const Op& op : ops_) {
      if (op.is_write && op.ok && op.data.empty()) {
        out.push_back(op.reg);
      }
    }
    return out;
  }

  // Register bytes of successful reads, in order.
  std::vector<uint8_t> readRegsOk() const {
    std::vector<uint8_t> out;
    for (const Op& op : ops_) {
      if (!op.is_write && op.ok) {
        out.push_back(op.reg);
      }
    }
    return out;
  }

  size_t readCount(uint8_t addr, uint8_t reg) const {
    size_t n = 0;
    for (const Op& op : ops_) {
      if (!op.is_write && op.addr == addr && op.reg == reg) {
        ++n;
      }
    }
    return n;
  }

  std::string dumpOps() const {
    std::string out;
    for (const Op& op : ops_) {
      out += (op.is_write ? "W " : "R ");
      out += Hex(op.addr);
      out += ' ';
      out += Hex(op.reg);
      if (op.ok) {
        out += " -> ";
        for (uint8_t b : op.data) {
          out += Hex(b);
          out += ' ';
        }
      } else {
        out += " -> FAIL";
      }
      out += '\n';
    }
    return out;
  }

  // ---- I2cBus --------------------------------------------------------------

  bool writeReg(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) override {
    Op op;
    op.is_write = true;
    op.addr = addr;
    op.reg = reg;
    if (data != nullptr && len > 0) {
      op.data.assign(data, data + len);
    }
    op.ok = IsPresent(addr) && !writes_fail_;
    if (data == nullptr && len > 0) {
      op.ok = false;
    }
    ops_.push_back(op);
    return op.ok;
  }

  bool readReg(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) override {
    Op op;
    op.is_write = false;
    op.addr = addr;
    op.reg = reg;

    bool ok = IsPresent(addr) && !reads_fail_ && data != nullptr && len > 0;
    const uint16_t key = Key(addr, reg);
    const auto fail = fail_next_read_.find(key);
    if (ok && fail != fail_next_read_.end() && fail->second > 0) {
      fail->second -= 1;
      ok = false;
    }

    std::vector<uint8_t> bytes;
    if (ok) {
      const auto queued = queue_.find(key);
      if (queued != queue_.end() && !queued->second.empty()) {
        bytes = queued->second.front();
        if (bytes.size() >= len) {
          bytes.resize(len);
          queued->second.erase(queued->second.begin());
        } else {
          ok = false;
        }
      } else {
        // Register-file fallback: bytes written with setRegBytes() start at their register
        // and cover the following registers (a real burst read behaves this way).
        bytes.resize(len, 0x00);
        for (size_t i = 0; i < len; ++i) {
          bytes[i] = LookupByte(addr, static_cast<uint8_t>(reg + i));
        }
      }
    }

    if (ok) {
      std::memcpy(data, bytes.data(), len);
      op.data = bytes;
    }
    op.ok = ok;
    ops_.push_back(op);
    return ok;
  }

 private:
  static uint16_t Key(uint8_t addr, uint8_t reg) {
    return static_cast<uint16_t>((static_cast<uint16_t>(addr) << 8) | reg);
  }

  static std::string Hex(uint8_t value) {
    static const char* digits = "0123456789abcdef";
    std::string out = "0x";
    out += digits[(value >> 4) & 0x0F];
    out += digits[value & 0x0F];
    return out;
  }

  bool IsPresent(uint8_t addr) const {
    const auto it = present_.find(addr);
    return it != present_.end() && it->second;
  }

  // Byte at (addr, reg) from the register file: the entry whose start register is the
  // greatest one <= reg (same address) supplies it when its byte range covers the offset.
  uint8_t LookupByte(uint8_t addr, uint8_t reg) const {
    const auto it = regs_.upper_bound(Key(addr, reg));
    if (it == regs_.begin()) {
      return 0x00;
    }
    const auto prev = std::prev(it);
    if (static_cast<uint8_t>(prev->first >> 8) != addr) {
      return 0x00;
    }
    const int offset = static_cast<int>(reg) - static_cast<int>(prev->first & 0x00FF);
    if (offset < 0 || static_cast<size_t>(offset) >= prev->second.size()) {
      return 0x00;
    }
    return prev->second[static_cast<size_t>(offset)];
  }

  std::map<uint8_t, bool> present_;
  std::map<uint16_t, std::vector<uint8_t>> regs_;
  std::map<uint16_t, std::vector<std::vector<uint8_t>>> queue_;
  std::map<uint16_t, int> fail_next_read_;
  std::vector<Op> ops_;
  bool reads_fail_ = false;
  bool writes_fail_ = false;
};

}  // namespace test
}  // namespace esp_node
