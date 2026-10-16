#include "WorkspaceInputPolicy.h"
#include <cctype>
#include <algorithm>

namespace workspace_input {
namespace {
bool Utf8(const std::string &source) {
  for (size_t i = 0; i < source.size();) {
    unsigned char c = source[i++];
    if (c < 128)
      continue;
    unsigned remaining = 0, value = 0;
    if (c >= 0xC2 && c <= 0xDF) {
      remaining = 1;
      value = c & 31;
    } else if (c >= 0xE0 && c <= 0xEF) {
      remaining = 2;
      value = c & 15;
    } else if (c >= 0xF0 && c <= 0xF4) {
      remaining = 3;
      value = c & 7;
    } else
      return false;
    if (i + remaining > source.size())
      return false;
    for (unsigned j = 0; j < remaining; ++j) {
      unsigned char next = source[i++];
      if ((next & 0xC0) != 0x80)
        return false;
      value = (value << 6) | (next & 63);
    }
    if ((remaining == 1 && value < 128) || (remaining == 2 && value < 2048) ||
        (remaining == 3 && value < 65536) || value > 0x10FFFF ||
        (value >= 0xD800 && value <= 0xDFFF))
      return false;
  }
  return true;
}
bool Controls(const std::string &text) {
  for (unsigned char c : text)
    if (c < 32 || c == 127)
      return true;
  return false;
}
bool Content(const std::string &text) {
  return text.find_first_not_of(' ') != std::string::npos;
}
} // namespace
// Match the web client's bare-command prefix policy without a regex per send.
static bool PrefixSeparator(const std::string& prefix,const std::string& draft){
  if(prefix.empty() || draft.empty() || std::isspace((unsigned char)draft[0]))return false;
  size_t i=(prefix[0]=='+' || prefix[0]=='@')?1:0;
  auto alpha=[](unsigned char c){return (c>='A'&&c<='Z') || (c>='a'&&c<='z');};
  auto name=[&](unsigned char c){return alpha(c) || (c>='0'&&c<='9') || c=='_' || c=='-';};
  if(i>=prefix.size() || !alpha(prefix[i++]))return false;
  for(;i<prefix.size();++i){if(prefix[i]=='/'){if(i+1==prefix.size() || !name(prefix[i+1]))return false;}else if(!name(prefix[i]))return false;}
  return true;
}
PreparedInput PrepareHumanInput(const std::string &original,
                                const std::string &resolved, bool connected,
                                const std::string &prefix,
                                const std::string &draft, size_t maxBytes) {
  PreparedInput result;
  if (original.empty()) {
    result.error = "This transcript has no original world destination.";
    return result;
  }
  if (resolved != original) {
    result.error =
        "The original world destination changed. The draft was kept.";
    return result;
  }
  if (!connected) {
    result.error = "The original world is disconnected. Reconnect it before "
                   "sending; the draft was kept.";
    return result;
  }
  if (draft.empty() || !Content(draft)) {
    result.error = "Type a message or command before sending.";
    return result;
  }
  const size_t limit = std::min<size_t>(maxBytes, 65536);
  const size_t separator=PrefixSeparator(prefix,draft)?1:0;
  if (!limit || prefix.size() > limit || draft.size() > limit - prefix.size() || separator>limit-prefix.size()-draft.size()) {
    result.error = "The prefix and message exceed the command byte limit. "
                   "Shorten the draft; it will not be split.";
    return result;
  }
  if (!Utf8(prefix) || !Utf8(draft)) {
    result.error = "The prefix or draft contains an invalid Unicode character. "
                   "The draft was kept.";
    return result;
  }
  if (Controls(prefix) || Controls(draft)) {
    result.error = "Enter sends one command. Remove line breaks, tabs or "
                   "control characters; the draft was kept.";
    return result;
  }
  result.command = prefix + (separator?" ":"") + draft;
  result.ok = true;
  return result;
}
InputHistory::InputHistory(size_t entries, size_t bytes)
    : maxEntries_(std::max<size_t>(1, std::min<size_t>(100, entries))),
      maxBytes_(std::max<size_t>(1, std::min<size_t>(65536, bytes))) {}
std::string InputHistory::Up(const std::string &current) {
  if (entries_.empty() || current.size() > 65536)
    return current;
  if (!browsing_) {
    unsent_ = current;
    position_ = entries_.size();
    browsing_ = true;
  }
  if (position_)
    --position_;
  return entries_[position_];
}
std::string InputHistory::Down(const std::string &current) {
  if (!browsing_)
    return current;
  if (position_ + 1 < entries_.size())
    return entries_[++position_];
  position_ = entries_.size();
  browsing_ = false;
  return unsent_;
}
bool InputHistory::Sent(const std::string &raw, bool success) {
  if (!success)
    return false;
  if (raw.empty() || !Content(raw) || raw.size() > maxBytes_ || !Utf8(raw) ||
      Controls(raw))
    return false;
  if (entries_.empty() || entries_.back() != raw) {
    entries_.push_back(raw);
    bytes_ += raw.size();
    while (entries_.size() > maxEntries_ || bytes_ > maxBytes_) {
      bytes_ -= entries_.front().size();
      entries_.pop_front();
    }
  }
  position_ = entries_.size();
  unsent_.clear();
  browsing_ = false;
  return true;
}
} // namespace workspace_input
