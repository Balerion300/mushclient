#include "PennLanguage.h"
#include <algorithm>
#include <cctype>
#include <set>
namespace PennIDE {
static bool Name(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '@';
}
static std::string Lower(std::string s) {
  for (auto &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
const std::vector<Reference> &References() {
  static const std::vector<Reference> refs = {
      {"add", "add(number, number, ...)", "Returns the sum of numbers."},
      {"cat", "cat(text, text, ...)", "Joins arguments with spaces."},
      {"if", "if(condition, true, false)",
       "Selects a result based on a condition."},
      {"switch", "switch(expression, pattern, result, ..., default)",
       "Selects a matching result."},
      {"get", "get(object/attribute)",
       "Reads an attribute when permitted by the game."},
      {"setq", "setq(register, value)", "Sets an evaluation register."},
      {"strcat", "strcat(text, text, ...)",
       "Concatenates arguments without spaces."},
      {"iter", "iter(list, expression, [delimiter], [separator])",
       "Evaluates an expression for list elements."},
      {"u", "u(object/attribute, argument, ...)",
       "Evaluates a user-defined attribute function."},
      {"@emit", "@emit text", "Emits text in the current location."},
      {"@pemit", "@pemit player=text", "Sends text to a player."},
      {"@set", "@set object=attribute:value", "Sets an attribute or flag."}};
  return refs;
}
bool ParseHelp(const std::string &text, HelpCatalog &catalog) {
  if (text.size() > 1024 * 1024 || !ValidUtf8(text))
    return false;
  HelpCatalog parsed = catalog;
  std::vector<std::string> names;
  std::string body;
  auto flush = [&]() {
    for (auto name : names) {
      if (parsed.size() >= 2048 && !parsed.count(Lower(name)))
        return false;
      HelpEntry entry;
      entry.callable =
          name.size() > 2 && name.compare(name.size() - 2, 2, "()") == 0;
      entry.command = !name.empty() && name[0] == '@';
      if (entry.callable)
        name.resize(name.size() - 2);
      entry.name = Lower(name);
      entry.text = body;
      size_t begin = body.find_first_not_of(" \r\n\t"),
             end = begin == std::string::npos ? begin : body.find('\n', begin);
      entry.signature =
          begin == std::string::npos
              ? ""
              : body.substr(begin,
                            end == std::string::npos ? end : end - begin);
      if (!entry.signature.empty() && entry.signature.back() == '\r')
        entry.signature.pop_back();
      parsed[entry.name] = entry;
    }
    names.clear();
    body.clear();
    return true;
  };
  for (size_t begin = 0; begin < text.size();) {
    size_t end = text.find('\n', begin);
    if (end == std::string::npos)
      end = text.size();
    auto line = text.substr(begin, end - begin);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.compare(0, 2, "& ") == 0) {
      if (!body.empty() && !flush())
        return false;
      auto name = line.substr(2);
      if (name.size() <= 128)
        names.push_back(name);
    } else if (!names.empty()) {
      if (body.size() + line.size() + 1 > 65536)
        return false;
      body += line + '\n';
    }
    begin = end + 1;
  }
  if (!flush())
    return false;
  catalog = parsed;
  return true;
}
std::string CatalogCompletion(const HelpCatalog &catalog,
                              const std::string &prefix) {
  std::string result;
  auto lower = Lower(prefix);
  for (auto &pair : catalog) {
    auto &r = pair.second;
    if ((r.callable || r.command) && pair.first.find(lower) == 0 &&
        pair.first.find_first_of(" \r\n\t") == std::string::npos) {
      if (!result.empty())
        result += ' ';
      result += pair.first;
    }
  }
  return result;
}
std::string CompletionCandidates(const HelpCatalog &catalog,
                                 const std::string &prefix,
                                 const std::vector<std::string> &discovered) {
  auto lower = Lower(prefix);
  std::set<std::string> names;
  auto add = [&](const std::string &name) {
    auto normalized = Lower(name);
    if (!normalized.empty() && normalized.find(lower) == 0 &&
        normalized.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyz0123456789_@/") == std::string::npos)
      names.insert(normalized);
  };
  for (const auto &reference : References())
    add(reference.name);
  for (const auto &pair : catalog)
    if (pair.second.callable || pair.second.command)
      add(pair.first);
  for (const auto &name : discovered)
    add(name);
  std::string result;
  for (const auto &name : names) {
    if (!result.empty())
      result += ' ';
    result += name;
  }
  return result;
}
std::string ActiveFunctionName(const std::string &s, size_t caret) {
  caret = std::min(caret, s.size());
  size_t begin = caret > 8192 ? caret - 8192 : 0;
  std::vector<size_t> stack;
  for (size_t i = begin; i < caret; ++i) {
    if (s[i] == '\\') {
      ++i;
      continue;
    }
    if (s[i] == '(')
      stack.push_back(i);
    else if (s[i] == ')' && !stack.empty())
      stack.pop_back();
  }
  if (stack.empty())
    return "";
  size_t p = stack.back(), b = p;
  while (b > begin && Name(s[b - 1]))
    --b;
  return Lower(s.substr(b, p - b));
}
Analysis AnalyzeLua(const std::string &s, size_t limit) {
  Analysis a;
  size_t n = std::min(s.size(), limit);
  a.limited = n < s.size();
  a.styles.assign(n, Plain);
  const std::string keywords =
      " and break do else elseif end false for function goto if in local nil "
      "not or repeat return then true until while ";
  auto longEnd = [&](size_t pos) -> size_t {
    if (pos >= n || s[pos] != '[')
      return pos;
    size_t equal = pos + 1;
    while (equal < n && s[equal] == '=' && equal - pos <= 64)
      ++equal;
    if (equal >= n || s[equal] != '[')
      return pos;
    std::string closing = "]" + std::string(equal - pos - 1, '=') + "]";
    auto found = std::search(s.begin() + equal + 1, s.begin() + n,
                             closing.begin(), closing.end());
    return found == s.begin() + n
               ? n
               : static_cast<size_t>(found - s.begin()) + closing.size();
  };
  for (size_t i = 0; i < n;) {
    size_t begin = i;
    if (s[i] == '-' && i + 1 < n && s[i + 1] == '-') {
      size_t end = longEnd(i + 2);
      if (end > i + 2)
        i = end;
      else
        while (i < n && s[i] != '\n')
          ++i;
      std::fill(a.styles.begin() + begin, a.styles.begin() + i, Substitution);
    } else if (longEnd(i) > i) {
      i = longEnd(i);
      std::fill(a.styles.begin() + begin, a.styles.begin() + i, Escape);
    } else if (s[i] == '\'' || s[i] == '"') {
      char quote = s[i++];
      while (i < n) {
        if (s[i] == '\\') {
          i = std::min(i + 2, n);
          continue;
        }
        if (s[i++] == quote)
          break;
      }
      std::fill(a.styles.begin() + begin, a.styles.begin() + i, Escape);
    } else if (Name(s[i])) {
      while (i < n && Name(s[i]))
        ++i;
      if (keywords.find(" " + s.substr(begin, i - begin) + " ") !=
          std::string::npos)
        std::fill(a.styles.begin() + begin, a.styles.begin() + i, Function);
    } else
      ++i;
  }
  return a;
}
bool ValidUtf8(const std::string &s) {
  for (size_t i = 0; i < s.size();) {
    unsigned char c = s[i++];
    if (c < 128) {
      if (!c)
        return false;
      continue;
    }
    unsigned n;
    unsigned cp;
    if (c >= 0xC2 && c <= 0xDF) {
      n = 1;
      cp = c & 31;
    } else if (c >= 0xE0 && c <= 0xEF) {
      n = 2;
      cp = c & 15;
    } else if (c >= 0xF0 && c <= 0xF4) {
      n = 3;
      cp = c & 7;
    } else
      return false;
    if (i + n > s.size())
      return false;
    for (unsigned j = 0; j < n; ++j) {
      unsigned char x = s[i++];
      if ((x & 0xC0) != 0x80)
        return false;
      cp = (cp << 6) | (x & 63);
    }
    if ((n == 1 && cp < 128) || (n == 2 && cp < 2048) ||
        (n == 3 && cp < 65536) || cp > 0x10FFFF ||
        (cp >= 0xD800 && cp <= 0xDFFF))
      return false;
  }
  return true;
}
bool ByteBoundary(const std::string &s, size_t p) {
  return p <= s.size() &&
         (p == s.size() || (static_cast<unsigned char>(s[p]) & 0xC0) != 0x80);
}
Analysis Analyze(const std::string &s, size_t limit) {
  Analysis a;
  size_t n = std::min(s.size(), limit);
  a.limited = n < s.size();
  a.styles.assign(n, Plain);
  std::vector<std::pair<char, size_t>> stack;
  for (size_t i = 0; i < n; ++i) {
    char c = s[i];
    if (c == '\\') {
      a.styles[i] = Escape;
      if (i + 1 < n)
        a.styles[++i] = Escape;
      continue;
    }
    if (c == '%') {
      a.styles[i] = Substitution;
      if (i + 1 < n)
        a.styles[++i] = Substitution;
      continue;
    }
    if (c == '[' || c == '(' || c == '{') {
      a.styles[i] = Bracket;
      stack.push_back({c, i});
    } else if (c == ']' || c == ')' || c == '}') {
      a.styles[i] = Bracket;
      char open = c == ']' ? '[' : c == ')' ? '(' : '{';
      if (stack.empty() || stack.back().first != open) {
        if (a.diagnostics.size() < 100)
          a.diagnostics.push_back(
              {i, "Possible unmatched closing bracket (advisory)."});
      } else
        stack.pop_back();
    } else if (Name(c) && (i == 0 || !Name(s[i - 1]))) {
      size_t end = i;
      while (end < n && Name(s[end]))
        ++end;
      size_t after = end;
      while (after < n && s[after] == ' ')
        ++after;
      Style style = c == '@'
                        ? Command
                        : (after < n && s[after] == '(' ? Function : Plain);
      std::fill(a.styles.begin() + i, a.styles.begin() + end, style);
      i = end - 1;
    }
  }
  if (!a.limited)
    for (auto b : stack) {
      if (a.diagnostics.size() < 100)
        a.diagnostics.push_back(
            {b.second, "Possible unclosed bracket (advisory)."});
    }
  return a;
}
std::string Completion(const std::string &prefix) {
  std::string result;
  for (auto &r : References())
    if (Lower(r.name).find(Lower(prefix)) == 0) {
      if (!result.empty())
        result += ' ';
      result += r.name;
    }
  return result;
}
std::string ArgumentHint(const std::string &s, size_t caret) {
  caret = std::min(caret, s.size());
  size_t begin = caret > 8192 ? caret - 8192 : 0;
  std::vector<size_t> stack;
  for (size_t i = begin; i < caret; ++i) {
    if (s[i] == '\\') {
      ++i;
      continue;
    }
    if (s[i] == '(')
      stack.push_back(i);
    else if (s[i] == ')' && !stack.empty())
      stack.pop_back();
  }
  if (stack.empty())
    return "";
  size_t p = stack.back(), b = p;
  while (b > begin && Name(s[b - 1]))
    --b;
  std::string name = Lower(s.substr(b, p - b));
  for (auto &r : References())
    if (name == r.name)
      return std::string(r.signature) + "  -  " + r.description;
  return "Custom function: " + name + " (unknown functions remain legal)";
}
bool Draft::Apply(size_t from, size_t to, const std::string &value,
                  uint64_t expected) {
  if (expected != revision || from > to || !ByteBoundary(source, from) ||
      !ByteBoundary(source, to) || !ValidUtf8(value) ||
      source.size() - (to - from) + value.size() > 4 * 1024 * 1024)
    return false;
  source.replace(from, to - from, value);
  ++revision;
  return true;
}
bool PrepareReview(const Draft &d, Review &r, std::string &error) {
  if (d.endpoint.empty()) {
    error = "Bind this draft to a destination first.";
    return false;
  }
  if (d.object.size() < 2 || d.object.size() > 11 || d.object[0] != '#' ||
      d.object.find_first_not_of("0123456789", 1) != std::string::npos) {
    error = "Use a numeric object reference, for example #123.";
    return false;
  }
  if (d.attribute.empty() || d.attribute.size() > 128 ||
      d.attribute.find_first_not_of(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") !=
          std::string::npos) {
    error = "Attribute names must use letters, digits and underscore.";
    return false;
  }
  if (!ValidUtf8(d.source) ||
      d.source.find_first_of("\r\n") != std::string::npos ||
      d.source.size() > 65536) {
    error = "Review requires valid UTF-8 and one command line, at most 64 KiB. "
            "Flatten multiline source explicitly first.";
    return false;
  }
  r.endpoint = d.endpoint;
  r.revision = d.revision;
  r.command = "&" + d.attribute + " " + d.object + "=" + d.source;
  error.clear();
  return true;
}
bool ReviewCurrent(const Draft &d, const Review &r,
                   const std::string &endpoint) {
  return endpoint == d.endpoint && r.endpoint == d.endpoint &&
         r.revision == d.revision;
}
bool PrepareCommands(const Draft &d, std::vector<std::string> &commands,
                     std::string &error) {
  if (d.endpoint.empty() || !ValidUtf8(d.source) || d.source.size() > 65536) {
    error = "A connected destination and valid UTF-8 source at most 64 KiB are "
            "required.";
    return false;
  }
  if (!d.object.empty() || !d.attribute.empty()) {
    Review review;
    if (!PrepareReview(d, review, error))
      return false;
    commands = {review.command};
    return true;
  }
  std::vector<std::string> lines;
  size_t begin = 0;
  while (begin < d.source.size()) {
    size_t end = d.source.find('\n', begin);
    if (end == std::string::npos)
      end = d.source.size();
    auto line = d.source.substr(begin, end - begin);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.size() > 4096 || lines.size() >= 128) {
      error = "Prepared commands are limited to 128 lines and 4096 UTF-8 bytes "
              "per line.";
      return false;
    }
    for (unsigned char c : line)
      if (c < 32 || c == 127) {
        error = "A prepared command contains a control character.";
        return false;
      }
    if (!line.empty())
      lines.push_back(line);
    begin = end + 1;
  }
  if (lines.empty()) {
    error = "There are no prepared command lines.";
    return false;
  }
  commands = lines;
  error.clear();
  return true;
}
static bool Nonce(const std::string &n) {
  return n.size() >= 16 && n.size() <= 64 &&
         n.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvw"
                             "xyz0123456789") == std::string::npos;
}
bool PrepareAttributeRead(const Draft &d, const std::string &nonce,
                          ReadRequest &request) {
  Draft blank = d;
  blank.source = "";
  Review r;
  std::string error;
  if (!Nonce(nonce) || !PrepareReview(blank, r, error))
    return false;
  request = ReadRequest();
  request.endpoint = d.endpoint;
  request.nonce = nonce;
  request.revision = d.revision;
  request.object = d.object;
  request.attribute = d.attribute;
  request.command = "think " + nonce + " [encode64(get(" + d.object + "/" +
                    d.attribute + "))]";
  return true;
}
bool PrepareDiscovery(const std::string &endpoint, const std::string &nonce,
                      ReadRequest &request) {
  if (endpoint.empty() || !Nonce(nonce))
    return false;
  request = ReadRequest();
  request.endpoint = endpoint;
  request.nonce = nonce;
  request.discovery = true;
  request.command = "think " + nonce + " [functions()]";
  return true;
}
bool AcceptAttributeRead(Draft &d, ReadRequest &r, const std::string &endpoint,
                         const std::string &nonce, const std::string &source) {
  if (r.consumed || r.discovery || endpoint != r.endpoint || nonce != r.nonce ||
      d.endpoint != endpoint || d.revision != r.revision ||
      d.object != r.object || d.attribute != r.attribute ||
      source.size() > 65536 || !ValidUtf8(source) ||
      source.compare(0, 3, "#-1") == 0)
    return false;
  d.Replace(source);
  r.consumed = true;
  return true;
}
bool EncodeRecovery(const std::vector<std::string> &sources,
                    std::string &bytes) {
  if (sources.size() > 32)
    return false;
  std::string result = "PENNIDE1\n";
  for (const auto &source : sources) {
    if (!ValidUtf8(source) || source.size() > 4 * 1024 * 1024)
      return false;
    result += std::to_string(source.size()) + "\n";
    result += source;
    if (result.size() > 8 * 1024 * 1024)
      return false;
  }
  bytes = result;
  return true;
}
bool DecodeRecovery(const std::string &bytes,
                    std::vector<std::string> &sources) {
  if (bytes.compare(0, 9, "PENNIDE2\n") == 0) {
    std::vector<RecoveryEntry> entries;
    if (!DecodeRecoveryEntries(bytes, entries))
      return false;
    std::vector<std::string> result;
    for (const auto &entry : entries)
      result.push_back(entry.source);
    sources = result;
    return true;
  }
  if (bytes.size() > 8 * 1024 * 1024 || bytes.compare(0, 9, "PENNIDE1\n") != 0)
    return false;
  std::vector<std::string> result;
  size_t i = 9;
  while (i < bytes.size()) {
    size_t end = bytes.find('\n', i);
    if (end == std::string::npos || end - i > 10 || end == i)
      return false;
    size_t n = 0;
    for (size_t p = i; p < end; ++p) {
      if (bytes[p] < '0' || bytes[p] > '9')
        return false;
      n = n * 10 + bytes[p] - '0';
      if (n > 4 * 1024 * 1024)
        return false;
    }
    i = end + 1;
    if (n > bytes.size() - i || result.size() >= 32)
      return false;
    auto source = bytes.substr(i, n);
    if (!ValidUtf8(source))
      return false;
    result.push_back(source);
    i += n;
  }
  sources = result;
  return true;
}
static bool RecoveryIdentity(const std::string &id) {
  return !id.empty() && id.size() <= 64 &&
         id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                              "vwxyz0123456789_-") == std::string::npos;
}
bool EncodeRecoveryEntries(const std::vector<RecoveryEntry> &entries,
                           std::string &bytes) {
  if (entries.size() > 32)
    return false;
  std::set<std::string> identities;
  std::string result = "PENNIDE2\n";
  for (const auto &entry : entries) {
    if (!RecoveryIdentity(entry.id) || !identities.insert(entry.id).second ||
        !ValidUtf8(entry.source) || entry.source.size() > 4 * 1024 * 1024)
      return false;
    result += entry.id + "\n" + std::to_string(entry.source.size()) + "\n";
    result += entry.source;
    if (result.size() > 8 * 1024 * 1024)
      return false;
  }
  bytes = result;
  return true;
}
bool DecodeRecoveryEntries(const std::string &bytes,
                           std::vector<RecoveryEntry> &entries) {
  std::vector<RecoveryEntry> result;
  if (bytes.compare(0, 9, "PENNIDE1\n") == 0) {
    std::vector<std::string> sources;
    if (!DecodeRecovery(bytes, sources))
      return false;
    for (size_t i = 0; i < sources.size(); ++i)
      result.push_back({"legacy-" + std::to_string(i), sources[i]});
  } else {
    if (bytes.size() > 8 * 1024 * 1024 ||
        bytes.compare(0, 9, "PENNIDE2\n") != 0)
      return false;
    std::set<std::string> identities;
    size_t i = 9;
    while (i < bytes.size()) {
      size_t end = bytes.find('\n', i);
      if (end == std::string::npos || end - i > 64)
        return false;
      auto id = bytes.substr(i, end - i);
      if (!RecoveryIdentity(id) || !identities.insert(id).second)
        return false;
      i = end + 1;
      end = bytes.find('\n', i);
      if (end == std::string::npos || end == i || end - i > 10)
        return false;
      size_t n = 0;
      for (size_t p = i; p < end; ++p) {
        if (bytes[p] < '0' || bytes[p] > '9')
          return false;
        n = n * 10 + bytes[p] - '0';
        if (n > 4 * 1024 * 1024)
          return false;
      }
      i = end + 1;
      if (n > bytes.size() - i || result.size() >= 32)
        return false;
      auto source = bytes.substr(i, n);
      if (!ValidUtf8(source))
        return false;
      result.push_back({id, source});
      i += n;
    }
  }
  entries = result;
  return true;
}
bool AcceptDiscovery(ReadRequest &r, const std::string &endpoint,
                     const std::string &nonce, const std::string &names,
                     std::vector<std::string> &result) {
  if (r.consumed || !r.discovery || endpoint != r.endpoint ||
      nonce != r.nonce || names.size() > 65536)
    return false;
  std::vector<std::string> parsed;
  for (size_t i = 0; i < names.size();) {
    while (i < names.size() && names[i] == ' ')
      ++i;
    if (i == names.size())
      break;
    size_t begin = i;
    while (i < names.size() && names[i] != ' ')
      ++i;
    auto name = names.substr(begin, i - begin);
    if (name.size() > 64 || name.empty() ||
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz0123456789_@") != std::string::npos ||
        parsed.size() >= 2048)
      return false;
    parsed.push_back(Lower(name));
  }
  std::sort(parsed.begin(), parsed.end());
  parsed.erase(std::unique(parsed.begin(), parsed.end()), parsed.end());
  result = parsed;
  r.consumed = true;
  return true;
}
} // namespace PennIDE
