// Native independently authored tools-only stdio MCP companion; no Python needed.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include "../AgentToolSchemas.h"
#include "../AgentBridgeProtocol.h"
#include "vendor/json.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <set>
#pragma comment(lib,"ws2_32.lib")
using Json=nlohmann::json;
namespace {
void check(bool b){if(!b)throw std::runtime_error("Request rejected or access unavailable");}
Json parseJson(const std::string& text){std::vector<std::set<std::string>> keys;auto callback=[&](int depth,Json::parse_event_t event,Json& value){check(depth<=16);if(event==Json::parse_event_t::object_start)keys.push_back({});else if(event==Json::parse_event_t::key)check(!keys.empty()&&keys.back().insert(value.get<std::string>()).second);else if(event==Json::parse_event_t::object_end)keys.pop_back();return true;};return Json::parse(text,callback);}
bool boundedLine(std::istream& stream,std::string& text,std::size_t max){text.clear();char c;while(stream.get(c)){if(c=='\n')return true;if(text.size()>=max)throw std::runtime_error("Request byte limit");text+=c;}return false;}
Json hostCall(const std::wstring& path,const std::string& tool,const Json& args){
 check(agent_workspace::validateToolArguments(tool,args.dump()));std::ifstream file(path);check(file.good());std::string content((std::istreambuf_iterator<char>(file)),{});check(content.size()<=4096);Json ep=parseJson(content);check(ep.is_object()&&ep.size()==4&&ep.at("host")=="127.0.0.1"&&ep.at("api_version")==1&&ep.at("port").is_number_integer()&&ep.at("token").is_string());int port=ep.at("port").get<int>();check(port>=1&&port<=65535);std::string token=ep.at("token").get<std::string>();check(token.size()>=32&&token.size()<=256);
 std::string request=Json({{"token",token},{"tool",tool},{"arguments",args}}).dump()+"\n";check(request.size()<=131072);
 SOCKET socketHandle=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);check(socketHandle!=INVALID_SOCKET);
 try{DWORD timeout=2000;setsockopt(socketHandle,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));setsockopt(socketHandle,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));
  // Nonblocking connect bounds the connection phase independently of IO timeout.
  u_long nonblock=1;ioctlsocket(socketHandle,FIONBIO,&nonblock);sockaddr_in address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(static_cast<u_short>(port));int connection=connect(socketHandle,reinterpret_cast<sockaddr*>(&address),sizeof(address));if(connection!=0){check(WSAGetLastError()==WSAEWOULDBLOCK);fd_set writes,errors;FD_ZERO(&writes);FD_ZERO(&errors);FD_SET(socketHandle,&writes);FD_SET(socketHandle,&errors);timeval wait={2,0};check(select(0,0,&writes,&errors,&wait)>0&&FD_ISSET(socketHandle,&writes)&&!FD_ISSET(socketHandle,&errors));int error=0,length=sizeof(error);check(getsockopt(socketHandle,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&length)==0&&error==0);}nonblock=0;ioctlsocket(socketHandle,FIONBIO,&nonblock);
  std::size_t sent=0;while(sent<request.size()){int n=send(socketHandle,request.data()+sent,static_cast<int>(request.size()-sent),0);check(n>0);sent+=n;}std::string response;char buffer[8192];while(response.find('\n')==std::string::npos){int n=recv(socketHandle,buffer,sizeof(buffer),0);check(n>0);response.append(buffer,n);check(response.size()<=2097152);}closesocket(socketHandle);socketHandle=INVALID_SOCKET;Json reply=parseJson(response.substr(0,response.find('\n')));check(reply.is_object()&&reply.at("ok")==true);return reply.at("result");
 }catch(...){if(socketHandle!=INVALID_SOCKET)closesocket(socketHandle);throw;}
}
Json invoke(const std::wstring& endpoint,const std::string& tool,const Json& args){check(agent_workspace::validateToolArguments(tool,args.dump()));if(tool!="wait_for_output"&&tool!="wait_for_agent_events")return hostCall(endpoint,tool,args);
 auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(args.value("timeout_ms",10000));std::string probe=tool=="wait_for_output"?"read_output":"get_agent_workspace";Json params=Json::object();if(probe=="read_output")params={{"window_id",args.at("window_id")},{"limit",1}};
 while(true){Json result=hostCall(endpoint,probe,params);auto revision=result.at("revision");if(revision!=args.at("revision"))return {{"changed",true},{"revision",revision}};if(std::chrono::steady_clock::now()>=deadline)return {{"changed",false},{"revision",revision}};std::this_thread::sleep_for(std::chrono::milliseconds(100));}
}
Json rpc(const Json& request,const std::wstring& endpoint){check(request.is_object()&&request.at("jsonrpc")=="2.0");if(!request.count("id"))return nullptr;Json response={{"jsonrpc","2.0"},{"id",request.at("id")}};
 try{const std::string method=request.at("method").get<std::string>();Json params=request.value("params",Json::object());check(params.is_object());if(method=="initialize"){std::string version=params.value("protocolVersion",std::string());if(version!="2025-11-25"&&version!="2025-06-18"&&version!="2025-03-26"&&version!="2024-11-05")version="2025-11-25";response["result"]={{"protocolVersion",version},{"capabilities",{{"tools",Json::object()}}},{"serverInfo",{{"name","mushclient-workspace"},{"version","0.1.0"}}}};}
 else if(method=="ping")response["result"]=Json::object();else if(method=="tools/list")response["result"]={{"tools",Json::parse(kAgentToolSchemas)}};
 else if(method=="tools/call"){check(params.is_object()&&params.count("name")&&params.size()<3);for(auto it=params.begin();it!=params.end();++it)check(it.key()=="name"||it.key()=="arguments");Json value=invoke(endpoint,params.at("name").get<std::string>(),params.value("arguments",Json::object()));response["result"]={{"content",Json::array({{{"type","text"},{"text",value.dump()}}})}};}
 else response["error"]={{"code",-32601},{"message","Method not found"}};
 }catch(const std::exception&){response["error"]={{"code",-32602},{"message","Request rejected or access unavailable"}};}return response;
}
}
int wmain(int argc,wchar_t** argv){
 try{std::wstring endpoint,mode,tool,argumentsPath;for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--endpoint"&&i+1<argc)endpoint=argv[++i];else if(arg==L"--arguments-file"&&i+1<argc)argumentsPath=argv[++i];else if(mode.empty())mode=arg;else if(tool.empty())tool=arg;else check(false);}
 if(mode==L"tools"){std::cout<<Json({{"tools",Json::parse(kAgentToolSchemas)}}).dump(2)<<"\n";return 0;}check(!endpoint.empty());WSADATA data;check(WSAStartup(MAKEWORD(2,2),&data)==0);
 if(mode==L"call"){Json args=Json::object();if(!argumentsPath.empty()){std::ifstream input(argumentsPath);check(input.good());std::string text((std::istreambuf_iterator<char>(input)),{});check(text.size()<=131072);args=parseJson(text);}std::string name;for(wchar_t ch:tool){check(ch>=32&&ch<=126);name+=static_cast<char>(ch);}std::cout<<invoke(endpoint,name,args).dump()<<"\n";return 0;}
 check(mode==L"mcp");std::string line;while(boundedLine(std::cin,line,131072)){Json response;try{response=rpc(parseJson(line),endpoint);}catch(const std::exception&){response={{"jsonrpc","2.0"},{"id",nullptr},{"error",{{"code",-32700},{"message","Parse error"}}}};}if(!response.is_null())std::cout<<response.dump()<<"\n"<<std::flush;}return 0;
 }catch(const std::exception&){std::cerr<<"Request rejected or access unavailable\n";return 1;}
}
