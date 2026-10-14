#pragma once
#include <string>
#include <vector>

// Data-only wire format shared with scripted_log_start.decode_configuration.
// Hex encoding preserves the legacy script interface's local-code-page bytes
// without inserting user text into Lua code.
struct LogNameField {
  std::string name, regex, capture = "1", transform;
  bool required = true;
};
struct LogNameConfiguration {
  std::string folder, filename = "{world}-{real_date}-{real_time}-{number}.txt";
  std::string request, start, end;
  bool header = false;
  std::vector<LogNameField> fields;
};
inline std::string EncodeLogNames(const LogNameConfiguration &config) {
  std::string data = "SLN1\n";
  auto value = [&](const std::string &text) {
    const char hex[] = "0123456789abcdef";
    for (unsigned char ch : text) {
      data += hex[ch >> 4];
      data += hex[ch & 15];
    }
    data += '\n';
  };
  value(config.folder); value(config.filename); value(config.request);
  value(config.start); value(config.end); value(config.header ? "1" : "0");
  value(std::to_string(config.fields.size()));
  for (const auto &field : config.fields) {
    value(field.name); value(field.regex); value(field.capture);
    value(field.transform); value(field.required ? "1" : "0");
  }
  return data;
}
inline bool DecodeLogNames(const std::string &input,
                           LogNameConfiguration &config) {
  if (input.size() > 65536)
    return false;
  // Native XML variables restore LF as CRLF. Normalize only structural line
  // endings; literal setting bytes are hex encoded and remain unchanged.
  std::string data;
  data.reserve(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '\r' && i + 1 < input.size() && input[i + 1] == '\n')
      continue;
    data += input[i];
  }
  if (data.compare(0, 5, "SLN1\n") != 0)
    return false;
  size_t offset = 5;
  auto value = [&](std::string &text) {
    size_t end = data.find('\n', offset);
    if (end == std::string::npos || (end - offset) % 2 || end - offset > 8192)
      return false;
    std::string decoded;
    auto digit = [](char ch) {
      if (ch >= '0' && ch <= '9') return ch - '0';
      if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
      return -1;
    };
    for (size_t i = offset; i < end; i += 2) {
      int a = digit(data[i]), b = digit(data[i + 1]);
      if (a < 0 || b < 0 || !(a || b)) return false;
      decoded += static_cast<char>(a * 16 + b);
    }
    offset = end + 1;
    text = decoded;
    return true;
  };
  LogNameConfiguration result;
  std::string header, count;
  if (!value(result.folder) || !value(result.filename) ||
      !value(result.request) || !value(result.start) || !value(result.end) ||
      !value(header) || !value(count) || (header != "0" && header != "1") ||
      count.empty() || count.size() > 2 ||
      count.find_first_not_of("0123456789") != std::string::npos)
    return false;
  int n = std::stoi(count);
  if (n > 16) return false;
  result.header = header == "1";
  for (int i = 0; i < n; ++i) {
    LogNameField field;
    std::string required;
    if (!value(field.name) || !value(field.regex) || !value(field.capture) ||
        !value(field.transform) || !value(required) ||
        (required != "0" && required != "1")) return false;
    field.required = required == "1";
    result.fields.push_back(field);
  }
  if (offset != data.size()) return false;
  config = result;
  return true;
}
