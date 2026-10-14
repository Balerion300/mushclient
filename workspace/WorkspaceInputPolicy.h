#pragma once
#include <cstddef>
#include <deque>
#include <string>

namespace workspace_input {
struct PreparedInput {
  bool ok = false;
  std::string command, error;
};
// resolvedEndpoint is the transport's resolved original connection identity,
// never the currently focused world. Prefix is concatenated exactly as entered.
PreparedInput PrepareHumanInput(const std::string &originalEndpoint,
                                const std::string &resolvedEndpoint,
                                bool connected, const std::string &prefix,
                                const std::string &draft,
                                size_t maxBytes = 4096);
class InputHistory {
public:
  explicit InputHistory(size_t maxEntries = 100, size_t maxBytes = 65536);
  std::string Up(const std::string &currentDraft);
  std::string Down(const std::string &currentDraft);
  // Call with raw draft after the original transport callback. Failed sends
  // leave history/navigation untouched. Only successful, valid inputs enter it.
  bool Sent(const std::string &rawDraft, bool success);
  size_t Size() const { return entries_.size(); }
  size_t Bytes() const { return bytes_; }
  bool Browsing() const { return browsing_; }

private:
  std::deque<std::string> entries_;
  std::string unsent_;
  size_t position_ = 0, bytes_ = 0, maxEntries_, maxBytes_;
  bool browsing_ = false;
};
} // namespace workspace_input
