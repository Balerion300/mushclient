#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "WorkspaceAppearance.h"

namespace workspace_spawn {
struct Preferences {
  std::wstring mention;
  int priority = 0;
  unsigned short shortcut = 0; // Low byte virtual key; high byte HOTKEYF_*.
  bool appearanceOverride = false;
  Appearance appearance;
};
inline bool ShortcutValid(unsigned short chord) {
  if (!chord) return true;
  unsigned key = chord & 255, modifiers = chord >> 8;
  if (modifiers & ~7U || !(modifiers & 6)) return false;
  if (key >= 0x70 && key <= 0x7b) { // F1..F12
    // Native close/switch/maximize gestures can bypass HACCEL tables.
    if (key == 0x73 || key == 0x75 || key == 0x79) return false; // F4/F6/F10.
    if (modifiers == 3 && key <= 0x78) return false; // Existing pane slots.
    return true;
  }
  // Keep ordinary editing chords and menu mnemonics out of focus bindings.
  return (modifiers & 6) == 6 &&
         ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'));
}
inline bool Valid(const Preferences &p) {
  if(p.appearanceOverride && !ValidAppearance(p.appearance))return false;
  if (p.mention.size() > 128 || p.priority < -100 || p.priority > 100 || !ShortcutValid(p.shortcut))
    return false;
  for (size_t i = 0; i < p.mention.size(); ++i) {
    auto ch = p.mention[i]; if (ch < 32 || ch == 127) return false;
    if (ch >= 0xd800 && ch <= 0xdbff) {
      if (++i == p.mention.size() || p.mention[i] < 0xdc00 || p.mention[i] > 0xdfff) return false;
    } else if (ch >= 0xdc00 && ch <= 0xdfff) return false;
  }
  return true;
}
struct Recipient { std::string name; bool hide = false; };
class ImageAssociation {
  struct Pending { uint64_t started = 0, sequence = 0; unsigned remaining = 4; std::vector<Recipient> recipients; };
  std::map<std::string, Pending> pending_;
public:
  void Clear(const std::string &connection) { pending_.erase(connection); }
  void Start(const std::string &connection, uint64_t sequence, uint64_t now,
             const std::vector<Recipient> &recipients) {
    Clear(connection);
    if (connection.empty() || !sequence || recipients.empty()) return;
    if (pending_.size() >= 128) pending_.clear(); // Optional associations fail closed.
    Pending next; next.started = now; next.sequence = sequence; next.recipients = recipients;
    if (next.recipients.size() > 64) return;
    pending_[connection] = next;
  }
  std::vector<Recipient> Take(const std::string &connection, uint64_t sequence, uint64_t now) {
    auto found = pending_.find(connection);
    if (found == pending_.end()) return {};
    auto &p = found->second;
    if (now < p.started || now - p.started > 2000 || !sequence || sequence != p.sequence + 1 || !p.remaining) {
      Clear(connection); return {};
    }
    auto result = p.recipients;
    p.sequence = sequence;
    if (!--p.remaining) Clear(connection);
    return result;
  }
};
} // namespace workspace_spawn
