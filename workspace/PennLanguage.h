#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace PennIDE {
enum Style { Plain, Function, Command, Substitution, Bracket, Escape };
struct Diagnostic {
  size_t byteOffset;
  std::string message;
};
struct Analysis {
  std::vector<unsigned char> styles;
  std::vector<Diagnostic> diagnostics;
  bool limited;
};
struct Reference {
  const char *name;
  const char *signature;
  const char *description;
};
const std::vector<Reference> &References();
Analysis Analyze(const std::string &utf8, size_t limit = 262144);
Analysis AnalyzeLua(const std::string &utf8, size_t limit = 262144);
bool ValidUtf8(const std::string &text);
bool ByteBoundary(const std::string &text, size_t offset);
std::string Completion(const std::string &prefix);
std::string ArgumentHint(const std::string &source, size_t caret);
struct HelpEntry {
  std::string name, signature, text;
  bool callable = false, command = false;
};
using HelpCatalog = std::map<std::string, HelpEntry>;
bool ParseHelp(const std::string &text, HelpCatalog &catalog);
std::string CatalogCompletion(const HelpCatalog &catalog,
                              const std::string &prefix);
std::string ActiveFunctionName(const std::string &source, size_t caret);
struct Draft {
  std::string endpoint, object, attribute, source;
  uint64_t revision = 1;
  void Replace(const std::string &value) {
    source = value;
    ++revision;
  }
  bool Apply(size_t from, size_t to, const std::string &value,
             uint64_t expected);
};
struct Review {
  std::string endpoint, command;
  uint64_t revision;
};
bool PrepareReview(const Draft &draft, Review &review, std::string &error);
bool ReviewCurrent(const Draft &draft, const Review &review,
                   const std::string &endpoint);
bool PrepareCommands(const Draft &draft, std::vector<std::string> &commands,
                     std::string &error);
// Requests are previews only. Caller must obtain explicit human read approval
// and route a bounded response from the exact original connection.
struct ReadRequest {
  std::string endpoint, nonce, command, object, attribute;
  uint64_t revision = 0;
  bool discovery = false, consumed = false;
};
bool PrepareAttributeRead(const Draft &draft, const std::string &nonce,
                          ReadRequest &request);
bool PrepareDiscovery(const std::string &endpoint, const std::string &nonce,
                      ReadRequest &request);
bool AcceptAttributeRead(Draft &draft, ReadRequest &request,
                         const std::string &endpoint, const std::string &nonce,
                         const std::string &source);
bool AcceptDiscovery(ReadRequest &request, const std::string &endpoint,
                     const std::string &nonce, const std::string &names,
                     std::vector<std::string> &result);
bool EncodeRecovery(const std::vector<std::string> &sources,
                    std::string &bytes);
bool DecodeRecovery(const std::string &bytes,
                    std::vector<std::string> &sources);
} // namespace PennIDE
