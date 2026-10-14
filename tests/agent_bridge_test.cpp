#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include "../AgentWorkspaceCore.h"
#include "../AgentBridgeHost.h"
#include "../AgentBridgeProtocol.h"
#include "../agent_companion/vendor/json.hpp"
#include <fstream>
#include <thread>
#include <atomic>
#include <cassert>
#include <iostream>
using namespace agent_workspace;
using Json=nlohmann::json;
std::string exchange(unsigned short port,std::string request){SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);sockaddr_in a={};a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(connect(s,reinterpret_cast<sockaddr*>(&a),sizeof(a))!=0){std::cerr<<"connect failed "<<WSAGetLastError()<<" port "<<port<<"\n";closesocket(s);return "{}";}request+='\n';send(s,request.data(),static_cast<int>(request.size()),0);std::string result;char buffer[4096];int n=0;while((n=recv(s,buffer,sizeof(buffer),0))>0){result.append(buffer,n);if(result.find('\n')!=std::string::npos)break;}closesocket(s);return result;}
int main(){Store store;store.addWindow("main","fictional","Main");store.append("main","completed output");Counter grant=store.enable("Performer",{"main"},true);
 Json request={{"token","test token"},{"tool","read_output"},{"arguments",{{"window_id","main"}}}};
 Json result=Json::parse(dispatch(store,"Performer",grant,request.dump()));assert(result.at("ok")==true);assert(result["result"]["lines"][0]["text"]=="completed output");
 request["arguments"]["limit"]=true;assert(Json::parse(dispatch(store,"Performer",grant,request.dump()))["ok"]==false);
 request["tool"]="send";request["arguments"]=Json::object();assert(Json::parse(dispatch(store,"Performer",grant,request.dump()))["ok"]==false);
 std::set<std::string> exercised;
 auto call=[&](const std::string& tool,const Json& args){Json req={{"token","fixture"},{"tool",tool},{"arguments",args}};Json response=Json::parse(dispatch(store,"Performer",grant,req.dump()));assert(response["ok"]==true);exercised.insert(tool);return response["result"];};
 call("list_windows",Json::object());call("read_output",{{"window_id","main"}});call("search_output",{{"window_id","main"},{"text","completed"}});call("wait_for_output",{{"window_id","main"},{"revision",0},{"timeout_ms",0}});
 assert(call("prepare_draft",{{"window_id","main"},{"request_id","test-draft"},{"text","unsent"}})["sent"]==false);
 call("get_agent_workspace",Json::object());call("post_agent_message",{{"text","hello"}});call("read_agent_messages",Json::object());call("wait_for_agent_events",{{"revision",0},{"timeout_ms",0}});
 std::string context=store.attachSelection("Performer",grant,"main","snapshot",1);call("read_agent_context",{{"context_id",context}});
 Editor editor;editor.id="fixture-editor";editor.window="main";editor.text="source";editor.revision=4294967296ULL;editor.uploading=false;store.putEditor(editor);store.shareEditor("Performer",grant,editor.id,true);call("read_shared_editor",{{"editor_id",editor.id}});assert(call("propose_editor_edit",{{"editor_id",editor.id},{"revision",editor.revision},{"text","proposed"}})["applied"]==false);
 Json task=call("start_agent_task",{{"title","offline rehearsal"}});call("heartbeat_agent_task",{{"task_id",task["task_id"]}});call("update_agent_task",{{"task_id",task["task_id"]},{"status","waiting"},{"progress","waiting for human"}});call("get_agent_tasks",Json::object());
 std::string capture=store.startCapture("Performer",grant,"main","fixture");store.append("main","capture text");call("list_debug_captures",Json::object());call("read_debug_capture",{{"capture_id",capture}});call("search_debug_capture",{{"capture_id",capture},{"text","capture"}});
 std::string scene=store.createScene("fictional scene");Npc npc;npc.id="npc";npc.agent="Performer";npc.window="main";npc.name="Guard";npc.brief="private";npc.posePrefix=":";npc.dialoguePrefix="\"";store.assignNpc(scene,npc);store.directScene(scene,"shared location","human direction",npc.id,"running");call("list_agent_scenes",Json::object());Json s=call("read_agent_scene",{{"scene_id",scene}});assert(call("propose_npc_action",{{"scene_id",scene},{"npc_id",npc.id},{"revision",s["revision"]},{"kind","pose"},{"text","waits."},{"request_id","action-request"}})["sent"]==false);
 assert(exercised.size()==22);
 std::cerr<<"protocol passed\n";
 BridgeHost host;std::wstring path=host.start(store,"Performer",grant);Json endpoint;std::ifstream file(path);file>>endpoint;file.close();assert(endpoint.at("host")=="127.0.0.1");
 std::cerr<<"host started\n";
 std::atomic<bool> done(false);std::string reply,bad,duplicate;
 std::thread client([&]{Json req={{"token",endpoint.at("token")},{"tool","list_windows"},{"arguments",Json::object()}};reply=::exchange(endpoint.at("port"),req.dump());req["token"]="wrong";bad=::exchange(endpoint.at("port"),req.dump());duplicate=::exchange(endpoint.at("port"),"{\"token\":\"wrong\",\"token\":\""+endpoint.at("token").get<std::string>()+"\",\"tool\":\"list_windows\",\"arguments\":{}}");done=true;});
 while(!done){MSG msg;while(PeekMessageW(&msg,0,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(1);}client.join();
 std::cerr<<"replies: "<<reply<<bad<<duplicate;
 assert(Json::parse(reply)["ok"]==true);assert(Json::parse(reply)["result"]["windows"].size()==1);assert(Json::parse(bad)["ok"]==false);assert(Json::parse(duplicate)["ok"]==false);
 host.stop();assert(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES);assert(!host.running());
 std::cout<<"agent native host: authenticated loopback, UI dispatch, strict host schema, duplicate-key rejection, revoke cleanup passed\n";
}
