#include "WorkspaceSharedHistory.h"
#include <cassert>
#include <iostream>
using namespace workspace_history;
int main(){
  Session s;s.Sent("look");s.Sent("+public Hello");
  assert(s.Recall(1,true,"Public draft")=="+public Hello");assert(s.Full(1));
  assert(s.Recall(2,true,"Roleplay draft")=="+public Hello");assert(s.Full(2));
  assert(s.Recall(1,false,"")=="Public draft");assert(!s.Full(1));
  assert(s.Recall(2,false,"")=="Roleplay draft");
  s.Recall(1,true,"saved");s.Edited(1,false);assert(s.Full(1));s.Edited(1,true);assert(!s.Full(1));
  const auto before=s.commands.size();s.Sent("connect Name secret");s.Sent(" @PASSWORD old=new");s.Sent("create New secret");assert(s.commands.size()==before);
  int doc,other;
  {Capture a(&doc,"alpha");Sent(&doc,"expanded canonical");Sent(&other,"wrong character");a.keep=true;}
  assert(For("alpha").commands.back()=="expanded canonical");
  {Capture a(&doc,"alpha");Sent(&doc,"secret omitted by plugin");}
  assert(For("alpha").commands.size()==1);
  {Capture a(&doc,"alpha",true);Sent(&doc,"bare-password");a.keep=true;}assert(For("alpha").commands.size()==1);
  Sent(&doc,"outside human send scope");assert(For("alpha").commands.size()==1);
  {Capture a(&doc,"closed");Sent(&doc,"closed command");a.keep=true;Sessions().erase("closed");}
  assert(!Sessions().count("closed"));
  For("alpha").Recall(123,true,"scratch");Transfer(123,456);assert(!For("alpha").cursors.count(123) && For("alpha").Full(456));Forget(456);assert(!For("alpha").cursors.count(456));
  for(int i=0;i<2000;++i)s.Sent(std::to_string(i));assert(s.commands.size()==1000);
  assert(s.bytes<=262144);assert(For("beta").commands.empty());
  std::cout<<"PASS shared canonical history, pane drafts, full-mode safety, omitted commands, connection isolation, cleanup and limits\n";
}
