#pragma once
#include <map>
#include <deque>
#include <string>
#include <cstdint>
#include <vector>
#include <cctype>

// Runtime-only history: connection identities, not game names or active focus.
// Commands are UTF-8 full wire lines; each pane owns its navigation scratch.
namespace workspace_history {
struct Cursor { size_t index=0; bool browsing=false,full=false,scratchFull=false; std::string scratch; };
struct Session {
  std::deque<std::string> commands;
  size_t bytes=0;
  std::map<std::uintptr_t,Cursor> cursors;
  void Sent(const std::string& command) {
    std::string verb;size_t begin=command.find_first_not_of(" \t");
    if(begin!=std::string::npos)for(size_t i=begin;i<command.size() && command[i]!=' ' && command[i]!='\t';++i)verb+=(char)std::tolower((unsigned char)command[i]);
    if(verb=="connect" || verb=="co" || verb=="create" || verb=="register" || verb=="@password" || verb=="@newpassword" || verb=="@pcreate")return;
    if(command.empty() || command.size()>65536 || command.find_first_of("\r\n")!=std::string::npos)return;
    if(commands.empty() || commands.back()!=command){commands.push_back(command);bytes+=command.size();}
    while(commands.size()>1000 || bytes>262144){bytes-=commands.front().size();commands.pop_front();}
    for(auto& item:cursors)item.second.browsing=false;
  }
  std::string Recall(std::uintptr_t pane,bool up,const std::string& draft) {
    auto& c=cursors[pane];
    if(commands.empty())return draft;
    if(up){if(!c.browsing){c.scratch=draft;c.scratchFull=c.full;c.index=commands.size();c.browsing=true;}if(c.index)--c.index;c.full=true;return commands[c.index];}
    if(!c.browsing)return draft;
    if(c.index+1<commands.size()){c.full=true;return commands[++c.index];}
    c.browsing=false;c.full=c.scratchFull;return c.scratch;
  }
  bool Full(std::uintptr_t pane)const{auto it=cursors.find(pane);return it!=cursors.end() && it->second.full;}
  void Edited(std::uintptr_t pane,bool empty){auto& c=cursors[pane];c.browsing=false;if(empty)c.full=false;}
  void Clear(){commands.clear();bytes=0;cursors.clear();}
};
inline std::map<std::string,Session>& Sessions(){static std::map<std::string,Session> sessions;return sessions;}
inline Session& For(const std::string& id){return Sessions()[id];}
inline void Transfer(std::uintptr_t from,std::uintptr_t to){for(auto& session:Sessions()){auto found=session.second.cursors.find(from);if(found!=session.second.cursors.end()){session.second.cursors[to]=found->second;session.second.cursors.erase(found);}}}
inline void Forget(std::uintptr_t pane){for(auto& session:Sessions())session.second.cursors.erase(pane);}
struct Capture {
  const void* document;std::string endpoint;Capture* previous;
  bool keep=false,privateInput=false;size_t pendingBytes=0;std::vector<std::string> pending;
  static Capture*& Active(){static Capture* value=nullptr;return value;}
  Capture(const void* doc,const std::string& id,bool secret=false):document(doc),endpoint(id),previous(Active()),privateInput(secret){For(id);Active()=this;}
  ~Capture(){Active()=previous;auto found=Sessions().find(endpoint);if(keep && !privateInput && !endpoint.empty() && found!=Sessions().end())for(const auto& line:pending)found->second.Sent(line);}
};
inline void Sent(const void* document,const std::string& utf8){auto* active=Capture::Active();if(active && active->document==document && !active->endpoint.empty() && active->pending.size()<1000 && utf8.size()<=65536 && active->pendingBytes+utf8.size()<=262144){active->pending.push_back(utf8);active->pendingBytes+=utf8.size();}}
}
