// Windows loopback host; independently authored. No persisted grants.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sddl.h>
#include <bcrypt.h>
#include "AgentBridgeHost.h"
#include "AgentBridgeProtocol.h"
#include "agent_companion/vendor/json.hpp"
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <stdexcept>
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"bcrypt.lib")
#pragma comment(lib,"user32.lib")
namespace agent_workspace {
using Json=nlohmann::json;
namespace {
const UINT kDispatch=WM_APP+240;
struct Request {std::string input,output;HANDLE done;std::atomic<bool> abandoned;Request():done(CreateEventW(0,TRUE,FALSE,0)),abandoned(false){}~Request(){if(done)CloseHandle(done);}};
void require(bool b){if(!b)throw std::runtime_error("local bridge unavailable");}
std::string randomToken(){unsigned char data[32];require(BCryptGenRandom(0,data,sizeof(data),BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0);const char* hex="0123456789abcdef";std::string s;for(unsigned char c:data){s+=hex[c>>4];s+=hex[c&15];}return s;}
PSECURITY_DESCRIPTOR accountDescriptor(){HANDLE token=0;require(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)!=FALSE);DWORD length=0;GetTokenInformation(token,TokenUser,0,0,&length);std::vector<unsigned char> bytes(length);bool ok=GetTokenInformation(token,TokenUser,bytes.data(),length,&length)!=FALSE;CloseHandle(token);require(ok);LPWSTR sid=0;require(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&sid)!=FALSE);std::wstring sddl=L"D:P(A;;FA;;;";sddl+=sid;sddl+=L")";LocalFree(sid);PSECURITY_DESCRIPTOR descriptor=0;require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,0)!=FALSE);return descriptor;}
}
struct BridgeHost::State {
 Store* store;std::string agent,token;Counter session;std::atomic<SOCKET> listener,client;std::atomic<bool> stopping;std::atomic<HWND> window;std::wstring directory,path;
 std::mutex mutex;std::deque<std::shared_ptr<Request>> queue;HANDLE stopEvent;
 State():store(0),session(0),listener(INVALID_SOCKET),client(INVALID_SOCKET),stopping(false),window(0),stopEvent(CreateEventW(0,TRUE,FALSE,0)){}
 ~State(){if(stopEvent)CloseHandle(stopEvent);}
};
namespace {
LRESULT CALLBACK requestWindow(HWND window,UINT message,WPARAM wp,LPARAM lp){
 BridgeHost::State* state=reinterpret_cast<BridgeHost::State*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_NCCREATE){state=static_cast<BridgeHost::State*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}
 if(message==kDispatch&&state){std::deque<std::shared_ptr<Request>> requests;{std::lock_guard<std::mutex> lock(state->mutex);requests.swap(state->queue);}for(const auto& request:requests){if(state->stopping||request->abandoned)continue;request->output=dispatch(*state->store,state->agent,state->session,request->input);SetEvent(request->done);}return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
void worker(std::shared_ptr<BridgeHost::State> state){
 // One bounded request at a time per grant. No UI waits, no Store access here.
 ULONGLONG second=GetTickCount64();unsigned requests=0;
 while(!state->stopping){SOCKET client=accept(state->listener,0,0);if(client==INVALID_SOCKET)break;{std::lock_guard<std::mutex> lock(state->mutex);if(state->stopping){closesocket(client);break;}state->client=client;}
  DWORD timeout=1500;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
  std::string reply="{\"ok\":false,\"error\":\"Request rejected or access expired\"}\n";
  try{std::string input;char buffer[4096];while(input.find('\n')==std::string::npos){int count=recv(client,buffer,sizeof(buffer),0);require(count>0);input.append(buffer,count);require(input.size()<=131072);}require(input.find('\n')==input.size()-1);
   // Parser depth and duplicate-key rejection prevent ambiguous authorization.
   std::vector<std::set<std::string>> keys;auto callback=[&](int depth,Json::parse_event_t event,Json& value){require(depth<=16);if(event==Json::parse_event_t::object_start)keys.push_back({});else if(event==Json::parse_event_t::key){require(!keys.empty()&&keys.back().insert(value.get<std::string>()).second);}else if(event==Json::parse_event_t::object_end)keys.pop_back();return true;};
   Json req=Json::parse(input,callback);require(req.is_object()&&req.size()==3&&req.count("token")&&req.count("tool")&&req.count("arguments")&&req.at("token").is_string());
   const std::string token=req.at("token").get<std::string>();unsigned diff=static_cast<unsigned>(token.size()^state->token.size());for(std::size_t i=0;i<state->token.size();++i)diff|=static_cast<unsigned char>(state->token[i])^(i<token.size()?static_cast<unsigned char>(token[i]):0);require(diff==0);
   ULONGLONG tick=GetTickCount64();if(tick-second>=1000){second=tick;requests=0;}require(++requests<=20);
   auto request=std::make_shared<Request>();request->input=input;require(request->done!=0);{std::lock_guard<std::mutex> lock(state->mutex);require(!state->stopping&&state->queue.size()<8);state->queue.push_back(request);}require(PostMessageW(state->window,kDispatch,0,0)!=FALSE);
   HANDLE events[2]={state->stopEvent,request->done};DWORD waited=WaitForMultipleObjects(2,events,FALSE,1500);if(waited==WAIT_OBJECT_0+1)reply=request->output;else request->abandoned=true;
  }catch(const std::exception&){}
  std::size_t sent=0;while(sent<reply.size()&&!state->stopping){int count=send(client,reply.data()+sent,static_cast<int>(reply.size()-sent),0);if(count<=0)break;sent+=count;}
  // stop() may close the active socket; exchange prevents double-close reuse.
  {std::lock_guard<std::mutex> lock(state->mutex);SOCKET owned=state->client.exchange(INVALID_SOCKET);if(owned!=INVALID_SOCKET)closesocket(owned);}
 }
}
}
BridgeHost::BridgeHost(){}BridgeHost::~BridgeHost(){stop();}
bool BridgeHost::running()const{return state_&&!state_->stopping;}
std::wstring BridgeHost::endpoint()const{return state_?state_->path:L"";}
std::wstring BridgeHost::start(Store& store,const std::string& agent,Counter session){
 stop();store.authorize(agent,session);auto s=std::make_shared<State>();s->store=&store;s->agent=agent;s->session=session;s->token=randomToken();state_=s;
 try{
  static bool winsock=false;if(!winsock){WSADATA data;require(WSAStartup(MAKEWORD(2,2),&data)==0);winsock=true;}
  s->listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);require(s->listener!=INVALID_SOCKET);BOOL exclusive=TRUE;require(setsockopt(s->listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive))==0);
  sockaddr_in address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;require(::bind(s->listener.load(),reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);require(listen(s->listener,4)==0);int length=sizeof(address);require(getsockname(s->listener,reinterpret_cast<sockaddr*>(&address),&length)==0);
  wchar_t temp[MAX_PATH+1];require(GetTempPathW(MAX_PATH,temp)>0);const std::string suffix=randomToken();s->directory=temp;s->directory+=L"mushclient-agent-";s->directory.append(suffix.begin(),suffix.end());s->path=s->directory+L"\\endpoint.json";
  PSECURITY_DESCRIPTOR sd=accountDescriptor();SECURITY_ATTRIBUTES sa={sizeof(sa),sd,FALSE};BOOL made=CreateDirectoryW(s->directory.c_str(),&sa);if(!made){LocalFree(sd);require(false);}HANDLE file=CreateFileW(s->path.c_str(),GENERIC_WRITE,0,&sa,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,0);LocalFree(sd);require(file!=INVALID_HANDLE_VALUE);
  std::string endpoint=Json({{"host","127.0.0.1"},{"port",ntohs(address.sin_port)},{"token",s->token},{"api_version",1}}).dump();DWORD written=0;BOOL ok=WriteFile(file,endpoint.data(),static_cast<DWORD>(endpoint.size()),&written,0);CloseHandle(file);require(ok&&written==endpoint.size());
  WNDCLASSW cls={};cls.lpfnWndProc=requestWindow;cls.hInstance=GetModuleHandleW(0);cls.lpszClassName=L"MUSHclientAgentRequestHost";if(!RegisterClassW(&cls))require(GetLastError()==ERROR_CLASS_ALREADY_EXISTS);
  s->window=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,0,cls.hInstance,s.get());require(s->window!=0);std::thread(worker,s).detach();return s->path;
 }catch(...){stop();throw;}
}
void BridgeHost::stop(){
 if(!state_)return;auto s=state_;s->stopping=true;SetEvent(s->stopEvent);
 SOCKET listener=s->listener.exchange(INVALID_SOCKET);if(listener!=INVALID_SOCKET)closesocket(listener);
 {std::lock_guard<std::mutex> lock(s->mutex);SOCKET client=s->client.load();if(client!=INVALID_SOCKET)shutdown(client,SD_BOTH);for(auto& req:s->queue)req->abandoned=true;s->queue.clear();}
 if(s->window){DestroyWindow(s->window);s->window=0;}
 if(!s->path.empty())DeleteFileW(s->path.c_str());if(!s->directory.empty())RemoveDirectoryW(s->directory.c_str());state_.reset();
}
}
