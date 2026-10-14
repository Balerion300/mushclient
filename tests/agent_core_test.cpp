#include "../AgentWorkspaceCore.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace agent_workspace;
void denied(const std::function<void()>& f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}assert(failed);}
int main(){
 Store store;store.addWindow("one","world-a","Main");store.addWindow("two","world-b","Main");
 denied([&]{store.listWindows("Alice",1);});
 Counter alice=store.enable("Alice",{"one"},true),bob=store.enable("Bob",{"two"},false);
 assert(store.listWindows("Alice",alice).size()==1);
 denied([&]{store.read("Alice",alice,"two");});
 store.append("one","a completed line",1234);
 Output out=store.read("Alice",alice,"one");assert(out.lines.size()==1&&out.lines[0].receivedAt==1234);
 Cursor cursor=out.cursor;store.append("one","NEXT");out=store.read("Alice",alice,"one",&cursor);assert(out.lines.size()==1&&!out.reset);
 assert(store.search("Alice",alice,"one","next").lines.size()==1);
 store.resetWindow("one");out=store.read("Alice",alice,"one",&cursor);assert(out.reset);
 for(int i=0;i<10010;++i)store.append("one","x");
 cursor.generation=out.cursor.generation;cursor.next=0;out=store.read("Alice",alice,"one",&cursor);assert(out.reset&&out.lines.size()==100);
 denied([&]{store.read("Alice",alice,"one",0,201);});
 store.append("one",std::string(9000,'x'));assert(store.read("Alice",alice,"one").lines.back().text.size()==4096);
 Draft d=store.prepareDraft("Alice",alice,"one","req","unsent");assert(!d.sent&&!d.opened&&d.reviewRequired);
 assert(store.prepareDraft("Alice",alice,"one","req","unsent").id==d.id);
 denied([&]{store.prepareDraft("Alice",alice,"one","req","different");});
 denied([&]{store.prepareDraft("Bob",bob,"two","req","draft");});
 store.postHuman("Alice","question");store.postMessage("Alice",alice,"reply");assert(store.messages("Alice",alice,0).size()==2&&store.messages("Bob",bob,0).empty());
 std::string context=store.attachSelection("Alice",alice,"one","selected only",123);
 denied([&]{store.context("Bob",bob,context);});
 Editor e;e.id="editor";e.window="one";e.text="source";e.revision=1;e.uploading=false;store.putEditor(e);
 denied([&]{store.readEditor("Alice",alice,e.id);});store.shareEditor("Alice",alice,e.id,true);
 Proposal p=store.proposeEdit("Alice",alice,e.id,1,"change","why");e.revision=2;store.putEditor(e);
 denied([&]{store.acceptEdit(p.id);});p=store.proposeEdit("Alice",alice,e.id,2,"change","why");e=store.acceptEdit(p.id);assert(e.text=="change"&&e.revision==3);
 p=store.proposeEdit("Alice",alice,e.id,3,"more","why");store.shareEditor("Alice",alice,e.id,false);denied([&]{store.acceptEdit(p.id);});
 Task task=store.startTask("Alice",alice,"running outside",100);store.cancelTask("Alice",task.id);assert(store.authorize("Alice",alice).tasks.at(task.id).cancellationRequested);
 assert(store.updateTask("Alice",alice,task.id,"cancelled","",101).status=="cancelled");
 std::string cap=store.startCapture("Alice",alice,"one","future only");assert(store.readCapture("Alice",alice,cap,0).lines.empty());store.append("one","capture me",999);store.stopCapture(cap);store.append("one","later");assert(store.readCapture("Alice",alice,cap,0).lines.size()==1);denied([&]{store.readCapture("Bob",bob,cap,0);});
 std::string scene=store.createScene("Offline rehearsal");Npc npc;npc.id="guard";npc.agent="Alice";npc.window="one";npc.name="Guard";npc.brief="private-a";npc.knowledge="secret-a";npc.posePrefix=":";npc.dialoguePrefix="\"";store.assignNpc(scene,npc);
 npc.id="merchant";npc.agent="Bob";npc.window="two";npc.name="Merchant";npc.brief="private-b";npc.knowledge="secret-b";store.assignNpc(scene,npc);
 assert(store.scene("Alice",alice,scene).npcs.at("merchant").brief.empty());
 Counter revision=store.scene("Alice",alice,scene).revision;
 denied([&]{store.proposeAction("Alice",alice,scene,"guard",revision,"pose","waits","p1");});
 store.directScene(scene,"location","direction","guard","running");revision=store.scene("Alice",alice,scene).revision;
 Action a=store.proposeAction("Alice",alice,scene,"guard",revision,"pose","waits","p1");assert(store.proposeAction("Alice",alice,scene,"guard",revision,"pose","waits","p1").id==a.id);
 store.editAction(scene,a.id,"human revised prose");assert(store.proposeAction("Alice",alice,scene,"guard",revision,"pose","waits","p1").id==a.id);store.editAction(scene,a.id,"waits");Draft staged=store.stageAction(scene,a.id);assert(staged.text==":waits"&&store.validateStagedDraft(staged.id)&&store.hasSceneBinding(staged.id));assert(store.validateStagedDraft(staged.id,":waits")&&!store.validateStagedDraft(staged.id,"@emit altered"));denied([&]{store.editAction(scene,a.id,"changed after staging");});assert(store.scene("Bob",bob,scene).actions.empty());
 store.directScene(scene,"location","new direction","guard","running");assert(!store.validateStagedDraft(staged.id)&&store.hasSceneBinding(staged.id));denied([&]{store.stageAction(scene,a.id);});
 store.expireWindow("one");denied([&]{store.read("Alice",alice,"one");});denied([&]{store.readCapture("Alice",alice,cap,0);});
 store.revoke("Alice");denied([&]{store.authorize("Alice",alice);});assert(store.listWindows("Bob",bob).size()==1);assert(store.drafts().size()==2);
 Counter replacement=store.enable("Bob",{"two"},true);assert(replacement!=bob);denied([&]{store.authorize("Bob",bob);});store.revokeAll();denied([&]{store.authorize("Bob",replacement);});
 assert(store.humanWorkspace("Alice").session==0&&store.humanWorkspace("Alice").messages.size()==2);
 Store limits;limits.addWindow("w","fictional","Main");std::vector<Counter> sessions;for(int i=0;i<8;++i)sessions.push_back(limits.enable("agent"+std::to_string(i),{"w"},false));denied([&]{limits.enable("ninth",{"w"},false);});
 for(int i=0;i<200;++i)limits.append("w",std::string(4096,'x'));
 Output boundedRead=limits.read("agent0",sessions[0],"w",0,200);std::size_t returned=0;for(const auto& line:boundedRead.lines)returned+=line.text.size();assert(returned<=32768&&boundedRead.lines.size()<=200);
 std::string umlaut="\xC3\x84";limits.append("w",umlaut+"BC");assert(limits.search("agent0",sessions[0],"w","\xC3\xA4""bc").lines.size()==1);
 std::string longUnicode;for(int i=0;i<3000;++i)longUnicode+="\xE2\x82\xAC";limits.append("w",longUnicode);std::string clipped=limits.read("agent0",sessions[0],"w",0,1).lines.back().text;assert(clipped.size()==4095&&clipped.size()%3==0);
 for(int i=0;i<8;++i){limits.expireWindow("w");limits.addWindow("w","fictional","replacement");denied([&]{limits.read("agent0",sessions[0],"w");});sessions[0]=limits.enable("agent0",{"w"},false);}
 std::string bulkCapture=limits.startCapture("agent0",sessions[0],"w","no replay");limits.append("w","new arrival",10);Cursor bulkCursor=limits.read("agent0",sessions[0],"w").cursor;Line old;old.text="old retained history";old.receivedAt=1;limits.replaceWindowHistory("w",{old,old});assert(limits.read("agent0",sessions[0],"w",&bulkCursor).reset);assert(limits.readCapture("agent0",sessions[0],bulkCapture,0).lines.size()==1);assert(limits.captures("agent0",sessions[0])[0].gaps==1);limits.append("w","incoming after rebuild",11);assert(limits.readCapture("agent0",sessions[0],bulkCapture,0).lines.size()==2);
 Store memory;std::vector<std::string> memoryWindows;for(int i=0;i<16;++i){std::string key="memory-"+std::to_string(i);memory.addWindow(key,"fictional",key);memoryWindows.push_back(key);}Counter memoryGrant=memory.enable("memory",memoryWindows,false);
 memory.append("memory-0","cursor source");Cursor memoryCursor=memory.read("memory",memoryGrant,"memory-0").cursor;
 for(const auto& key:memoryWindows)for(int i=0;i<600;++i)memory.append(key,std::string(4096,'x'));
 std::size_t total=0;for(const auto& w:memory.humanWindows()){assert(w.lines.empty()&&w.retainedBytes<=2*1024*1024);total+=w.retainedBytes;}assert(total<=16*1024*1024&&total>15*1024*1024);
 assert(memory.read("memory",memoryGrant,"memory-0",&memoryCursor).reset);
 for(int i=0;i<1000;++i)memory.append("memory-15",std::string(4096,'x'));
 total=0;for(const auto& w:memory.listWindows("memory",memoryGrant))total+=w.retainedBytes;assert(total<=16*1024*1024);
 memory.resetWindow("memory-15");memory.expireWindow("memory-14");Line replacementLine;replacementLine.text=std::string(4096,'r');memory.replaceWindowHistory("memory-13",std::vector<Line>(10000,replacementLine));total=0;for(const auto& w:memory.humanWindows()){assert(w.retainedBytes<=2*1024*1024);total+=w.retainedBytes;}assert(total<=16*1024*1024);
 std::cout<<"agent core: isolation, cursors, bounds, drafts, editor revisions, task cancellation, captures, private scenes and revoke passed\n";
}
