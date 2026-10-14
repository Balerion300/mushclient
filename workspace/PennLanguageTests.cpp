#include "PennLanguage.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
int main() {
  PennIDE::HelpCatalog help;
  assert(
      PennIDE::ParseHelp("& GET()\n& XGET()\n  get(<object>/<attribute>)\n\n  "
                         "Reads without evaluation.\n& @EMIT\n  @emit text\n",
                         help));
  assert(help.at("get").signature == "get(<object>/<attribute>)");
  assert(help.at("xget").text == help.at("get").text);
  assert(PennIDE::CatalogCompletion(help, "XG") == "xget");
  assert(PennIDE::CompletionCandidates(help, "G", {"GET", "get_custom"}) ==
         "get get_custom");
  assert(PennIDE::CompletionCandidates(help, "@", {"@set", "bad name"}) ==
         "@emit @pemit @set");
  assert(PennIDE::CompletionCandidates({}, "STR") == "strcat");
  assert(PennIDE::CompletionCandidates(
             help, "custom", {"CUSTOM", "custom", "custom;bad"}) ==
         "custom");
  assert(PennIDE::ActiveFunctionName("[xget(#123,", 11) == "xget");
  auto originalCatalogSize = help.size();
  assert(!PennIDE::ParseHelp(std::string("& BAD()\na\0b", 12), help));
  assert(help.size() == originalCatalogSize);
  std::ifstream docs("third_party/pennmush-reference/pennfunc.hlp",
                     std::ios::binary);
  assert(docs);
  std::string upstream((std::istreambuf_iterator<char>(docs)),
                       std::istreambuf_iterator<char>());
  assert(PennIDE::ParseHelp(upstream, help));
  assert(help.size() > 500);
  assert(help.count("regeditall") && help.count("xget"));
  assert(PennIDE::CompletionCandidates(help, "REGEDIT") ==
         "regedit regeditall regeditalli regediti");
  assert(PennIDE::CompletionCandidates(help, "STR").find("strlen") !=
         std::string::npos);
  auto lua =
      PennIDE::AnalyzeLua("local value = 'hello' -- comment\nreturn value");
  assert(lua.styles[0] == PennIDE::Function);
  assert(lua.styles[14] == PennIDE::Escape);
  assert(lua.styles[24] == PennIDE::Substitution);
  auto luaLong =
      PennIDE::AnalyzeLua("--[=[ comment\n]=]\nlocal x=[==[ multi\nline ]==]");
  assert(luaLong.styles[5] == PennIDE::Substitution);
  assert(luaLong.styles[18] == PennIDE::Function);
  assert(luaLong.styles.back() == PennIDE::Escape);
  using namespace PennIDE;
  assert(ValidUtf8("hello"));
  assert(ValidUtf8("\xF0\x9F\x90\x89"));
  assert(!ValidUtf8("\xED\xA0\x80"));
  assert(!ValidUtf8("\xC0\xAF"));
  assert(!ValidUtf8(std::string("a\0b", 3)));
  Draft d;
  d.endpoint = "world-a/session-1";
  d.source = "a\xF0\x9F\x90\x89z";
  assert(!d.Apply(2, 3, "x", 1));
  assert(d.Apply(1, 5, "dragon", 1));
  assert(d.source == "adragonz");
  assert(!d.Apply(0, 1, "x", 1));
  auto a = Analyze("[add(1,2)] %0 \\[ custom(yes)");
  assert(a.diagnostics.empty());
  assert(a.styles[1] == Function);
  assert(a.styles[0] == Bracket);
  assert(!Analyze("[add(1,2)").diagnostics.empty());
  assert(Analyze(std::string(300000, 'x')).limited);
  assert(Analyze(std::string(300000, 'x')).styles.size() == 262144);
  assert(Completion("str") == "strcat");
  assert(ArgumentHint("[add(1,", 7).find("add(number") != std::string::npos);
  assert(ArgumentHint("custom(", 7).find("Custom function") !=
         std::string::npos);
  d.object = "#123";
  d.attribute = "CODE";
  Review r;
  std::string error;
  assert(PrepareReview(d, r, error));
  assert(r.command == "&CODE #123=adragonz");
  assert(ReviewCurrent(d, r, d.endpoint));
  assert(!ReviewCurrent(d, r, "world-b/session-1"));
  d.Replace("new");
  assert(!ReviewCurrent(d, r, d.endpoint));
  d.source = "line\nline";
  assert(!PrepareReview(d, r, error));
  d.source = "new";
  d.object = "#123;@emit";
  assert(!PrepareReview(d, r, error));
  d.object = "#123";
  d.attribute = "X=bad";
  assert(!PrepareReview(d, r, error));
  d.attribute = "CODE";
  d.source = "unsent";
  ReadRequest request;
  assert(!PrepareAttributeRead(d, "short", request));
  assert(PrepareAttributeRead(d, "0123456789abcdef", request));
  assert(request.command ==
         "think 0123456789abcdef [encode64(get(#123/CODE))]");
  assert(
      !AcceptAttributeRead(d, request, "wrong-world", request.nonce, "loaded"));
  assert(!AcceptAttributeRead(d, request, d.endpoint, request.nonce,
                              "#-1 PERMISSION DENIED"));
  assert(AcceptAttributeRead(d, request, d.endpoint, request.nonce, "loaded"));
  assert(!AcceptAttributeRead(d, request, d.endpoint, request.nonce,
                              "loaded again"));
  assert(PrepareAttributeRead(d, "0123456789abcdef", request));
  d.Replace("changed");
  assert(
      !AcceptAttributeRead(d, request, d.endpoint, request.nonce, "overwrite"));
  assert(PrepareDiscovery(d.endpoint, "0123456789abcdef", request));
  std::vector<std::string> names;
  assert(
      !AcceptDiscovery(request, "wrong", request.nonce, "ADD CUSTOM", names));
  assert(!AcceptDiscovery(request, d.endpoint, request.nonce, "ADD BAD;INJECT",
                          names));
  assert(AcceptDiscovery(request, d.endpoint, request.nonce, "ADD CUSTOM add",
                         names));
  assert(names.size() == 2 && names[0] == "add" && names[1] == "custom");
  assert(!AcceptDiscovery(request, d.endpoint, request.nonce, "OTHER", names));
  std::vector<std::string> drafts = {"one\nline", "unicode \xF0\x9F\x90\x89",
                                     ""},
                           restored;
  std::string recovery;
  assert(EncodeRecovery(drafts, recovery));
  assert(DecodeRecovery(recovery, restored));
  assert(restored == drafts);
  assert(!DecodeRecovery("PENNIDE1\n99999999999\nx", restored));
  assert(!DecodeRecovery("PENNIDE1\n2\na", restored));
  assert(!EncodeRecovery(std::vector<std::string>(33, "x"), recovery));
  std::vector<RecoveryEntry> entries = {{"draft-one", drafts[0]},
                                        {"draft-two", drafts[1]}}, decoded;
  assert(EncodeRecoveryEntries(entries, recovery));
  assert(DecodeRecoveryEntries(recovery, decoded));
  assert(decoded.size() == 2 && decoded[0].id == entries[0].id &&
         decoded[1].source == entries[1].source);
  assert(DecodeRecovery(recovery, restored));
  assert(restored == std::vector<std::string>({drafts[0], drafts[1]}));
  const auto validRecovery = recovery;
  assert(!EncodeRecoveryEntries({{"same", "A"}, {"same", "B"}}, recovery));
  assert(recovery == validRecovery);
  assert(!EncodeRecoveryEntries({{"bad\nid", "A"}}, recovery));
  assert(!DecodeRecoveryEntries("PENNIDE2\nsame\n1\nAsame\n1\nB", decoded));
  assert(!DecodeRecoveryEntries("PENNIDE2\nid\n9999999999\n", decoded));
  assert(!DecodeRecoveryEntries("PENNIDE2\nid\n2\na", decoded));
  assert(!DecodeRecoveryEntries("PENNIDE2\nid\n1\n\xff", decoded));
  assert(decoded.size() == 2 && decoded[0].id == entries[0].id);
  assert(EncodeRecovery(drafts, recovery));
  assert(DecodeRecoveryEntries(recovery, decoded));
  assert(decoded.size() == 3 && decoded[0].id == "legacy-0" &&
         decoded[0].source == drafts[0] && decoded[1].id == "legacy-1");
  assert(EncodeRecoveryEntries(decoded, recovery));
  assert(DecodeRecoveryEntries(recovery, entries));
  assert(entries[0].id == decoded[0].id && entries[0].source == drafts[0]);
  assert(!EncodeRecoveryEntries(std::vector<RecoveryEntry>(33, {"id", "x"}),
                                recovery));
  assert(!EncodeRecoveryEntries({{"a", std::string(4 * 1024 * 1024, 'x')},
                                 {"b", std::string(4 * 1024 * 1024, 'y')}},
                                recovery));
  Draft prepared;
  prepared.endpoint = "fixture";
  prepared.source = "say one\r\n\nsay two";
  std::vector<std::string> commands;
  assert(PrepareCommands(prepared, commands, error));
  assert(commands.size() == 2 && commands[0] == "say one" &&
         commands[1] == "say two");
  prepared.source = "say one\x1b";
  assert(!PrepareCommands(prepared, commands, error));
  prepared.source = std::string(4097, 'x');
  assert(!PrepareCommands(prepared, commands, error));
  prepared.source = "[add(1,2)]";
  prepared.object = "#123";
  prepared.attribute = "CODE";
  assert(PrepareCommands(prepared, commands, error));
  assert(commands.size() == 1 && commands[0] == "&CODE #123=[add(1,2)]");
  prepared.source = "multiline\ncode";
  assert(!PrepareCommands(prepared, commands, error));
  prepared.endpoint.clear();
  assert(!PrepareCommands(prepared, commands, error));
  std::cout << "PennLanguage tests passed\n";
}
