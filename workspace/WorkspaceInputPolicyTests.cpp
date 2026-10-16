#include "WorkspaceInputPolicy.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace workspace_input;
  auto input = PrepareHumanInput("world-a", "world-a", true, "+pub ",
                                 "dragon \xF0\x9F\x90\x89");
  assert(input.ok && input.command == "+pub dragon \xF0\x9F\x90\x89");
  assert(PrepareHumanInput("world-a", "world-a", true, "page Alice=", "hello")
             .command == "page Alice=hello");
  assert(
      PrepareHumanInput("world-a", "world-a", true, "+pub", "hello").command ==
      "+pub hello");
  assert(PrepareHumanInput("a","a",true,"+staff","Hi").command=="+staff Hi");
  assert(PrepareHumanInput("a","a",true,"+staff"," Hi").command=="+staff Hi");
  assert(PrepareHumanInput("a","a",true,"+staff ","Hi").command=="+staff Hi");
  assert(PrepareHumanInput("a","a",true,":","smiles").command==":smiles");
  assert(PrepareHumanInput("a","a",true,"page Person=","Hi").command=="page Person=Hi");
  assert(PrepareHumanInput("a","a",true,"+channel/switch","Hi").command=="+channel/switch Hi");
  assert(!PrepareHumanInput("a","a",true,"+staff","Hi",8).ok);
  assert(!PrepareHumanInput("world-a", "world-b", true, "", "look").ok);
  assert(!PrepareHumanInput("world-a", "world-a", false, "", "look").ok);
  assert(!PrepareHumanInput("", "", true, "", "look").ok);
  assert(!PrepareHumanInput("world-a", "world-a", true, "", "   ").ok);
  for (auto bad : {std::string("one\ntwo"), std::string("one\rtwo"),
                   std::string("one\ttwo"), std::string("one\x1b"),
                   std::string("x\0y", 3), std::string("x\x7f")}) {
    assert(!PrepareHumanInput("a", "a", true, "", bad).ok);
    assert(!PrepareHumanInput("a", "a", true, bad, "text").ok);
  }
  assert(!PrepareHumanInput("a", "a", true, "", std::string("\xED\xA0\x80", 3))
              .ok);
  assert(!PrepareHumanInput("a", "a", true, "", std::string("\xC0\xAF", 2)).ok);
  assert(
      !PrepareHumanInput("a", "a", true, "", std::string("\xF4\x90\x80\x80", 4))
           .ok);
  assert(!PrepareHumanInput("a", "a", true, "\xF0\x9F\x90\x89", "x", 4).ok);
  assert(PrepareHumanInput("a", "a", true, "", "\xF0\x9F\x90\x89", 4).ok);
  InputHistory history(3, 64);
  assert(history.Up("unsent") == "unsent");
  assert(history.Down("new text") == "new text");
  assert(history.Sent("first", true));
  assert(history.Sent("second", true));
  assert(history.Up("unfinished") == "second");
  assert(history.Up("second") == "first");
  assert(history.Up("first") == "first");
  assert(history.Down("first") == "second");
  assert(history.Down("second") == "unfinished");
  assert(history.Down("edited afterward") == "edited afterward");
  assert(!history.Sent("failed command", false));
  assert(history.Size() == 2);
  assert(history.Up("failed command") == "second");
  assert(!history.Sent("failed while browsing", false));
  assert(history.Browsing());
  assert(history.Down("second") == "failed command");
  assert(history.Sent("second", true) && history.Size() == 2);
  assert(history.Sent("third", true));
  assert(history.Sent("fourth", true) && history.Size() == 3);
  assert(history.Up("new") == "fourth");
  assert(history.Up("fourth") == "third");
  assert(history.Up("third") == "second");
  InputHistory bounded(100, 12);
  assert(bounded.Sent("dragon", true));
  assert(bounded.Sent("second", true));
  assert(bounded.Sent("third", true));
  assert(bounded.Size() == 2 && bounded.Bytes() == 11);
  assert(!bounded.Sent(std::string(13, 'x'), true));
  assert(!bounded.Sent("bad\ninput", true));
  assert(bounded.Size() == 2);
  assert(bounded.Up("\xF0\x9F\x90\x89 unsent") == "third");
  assert(bounded.Down("third") == "\xF0\x9F\x90\x89 unsent");
  std::cout
      << "Workspace human input policy tests passed: exact prefix/Unicode, "
         "original endpoint, disconnect/control/byte bounds, successful "
         "history, failed-send preservation, draft restoration and eviction\n";
}
