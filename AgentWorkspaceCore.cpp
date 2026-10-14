#include "AgentWorkspaceCore.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <chrono>

namespace agent_workspace {
namespace {
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void bounded(const std::string& s,std::size_t n,bool empty=false) { require((empty||!s.empty())&&s.size()<=n,"text outside byte bounds"); }
std::string clip(const std::string& s,std::size_t n) { if(s.size()<=n)return s; while(n && (static_cast<unsigned char>(s[n])&0xc0)==0x80)--n; return s.substr(0,n); }
std::string lower(std::string s) {
#ifdef _WIN32
 int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),0,0);
 if(count){std::wstring source(count,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),&source[0],count);
  int needed=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,source.data(),count,0,0,0,0,0);if(needed){std::wstring folded(needed,L'\0');LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,source.data(),count,&folded[0],needed,0,0,0);int bytes=WideCharToMultiByte(CP_UTF8,0,folded.data(),needed,0,0,0,0);std::string result(bytes,'\0');if(bytes)WideCharToMultiByte(CP_UTF8,0,folded.data(),needed,&result[0],bytes,0,0);return result;}}
#endif
 for(std::size_t i=0;i<s.size();++i) s[i]=static_cast<char>(std::tolower(static_cast<unsigned char>(s[i]))); return s;
}
bool matches(const std::string& s,const std::string& q) { return lower(s).find(lower(q))!=std::string::npos; }
}
Store::Store():serial_(0),retainedOrder_(0),historyBytes_(0){}
Window Store::metadata(const Window& source)const{Window w;w.id=source.id;w.world=source.world;w.title=source.title;w.generation=source.generation;w.revision=source.revision;w.next=source.next;w.retainedBytes=source.retainedBytes;return w;}
void Store::removeOldest(Window& w){std::size_t bytes=w.lines.front().text.size()+128;w.retainedBytes-=bytes;historyBytes_-=bytes;w.lines.pop_front();++w.revision;for(auto& g:grants_)if(g.second.windows.count(w.id))++g.second.events;}
void Store::pruneHistory(Window& w){
 while(w.lines.size()>10000||w.retainedBytes>2*1024*1024)removeOldest(w);
 while(historyBytes_>16*1024*1024){Window* oldest=0;for(auto& p:windows_)if(!p.second.lines.empty()&&(!oldest||p.second.lines.front().retainedOrder<oldest->lines.front().retainedOrder))oldest=&p.second;require(oldest!=0,"history accounting failure");removeOldest(*oldest);}
}
std::string Store::id(const char* p) { return std::string(p)+std::to_string(++serial_); }
void Store::addWindow(const std::string& key,const std::string& world,const std::string& title) {
 require(!key.empty(),"missing window ID");require(windows_.count(key)||windows_.size()<128,"window limit"); if(windows_.count(key))expireWindow(key);
 Window w; w.id=key;w.world=world;w.title=title;w.generation=++serial_;w.revision=1;w.next=0;windows_[key]=w;
}
void Store::append(const std::string& key,const std::string& text,Counter time) {
 require(windows_.count(key)!=0,"unknown window");Window& w=windows_.at(key);
 Line l;l.index=w.next++;l.sourceIndex=l.index;l.generation=w.generation;l.retainedOrder=++retainedOrder_;l.capturedAt=0;l.text=clip(text,4096);l.truncated=text.size()>l.text.size();l.receivedAt=time;w.lines.push_back(l);++w.revision;
 std::size_t bytes=l.text.size()+128;w.retainedBytes+=bytes;historyBytes_+=bytes;pruneHistory(w);
 for(auto& p:captures_) { Capture& c=p.second;if(!c.active||c.window!=key)continue;
  Line cl=l;cl.index=c.next++;cl.capturedAt=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();c.lines.push_back(cl);c.bytes+=cl.text.size();
  while(c.lines.size()>10000||c.bytes>1048576) {c.bytes-=c.lines.front().text.size();c.lines.pop_front();++c.dropped;}
 }
 for(auto& g:grants_)if(g.second.windows.count(key))++g.second.events;
}
void Store::resetWindow(const std::string& key) {
 require(windows_.count(key)!=0,"unknown window");Window& w=windows_.at(key);historyBytes_-=w.retainedBytes;w.retainedBytes=0;w.lines.clear();w.next=0;w.generation=++serial_;++w.revision;
 for(auto& g:grants_)if(g.second.windows.count(key)){g.second.windows[key]=w.generation;++g.second.events;}
 for(auto& c:captures_)if(c.second.window==key&&c.second.active)++c.second.gaps;
}
void Store::expireWindow(const std::string& key) {
 for(auto& g:grants_) { if(!g.second.windows.erase(key))continue;
  ++g.second.events;
  for(auto e=g.second.editors.begin();e!=g.second.editors.end();)if(editors_.count(*e)&&editors_.at(*e).window==key)e=g.second.editors.erase(e);else ++e;
  for(auto c=g.second.contexts.begin();c!=g.second.contexts.end();)if(c->second.window==key)c=g.second.contexts.erase(c);else ++c;
 }
 for(auto c=captures_.begin();c!=captures_.end();)if(c->second.window==key)c=captures_.erase(c);else ++c;
 for(auto& s:scenes_)for(auto& n:s.second.npcs)if(n.second.window==key){invalidateScene(s.second);break;}
 auto window=windows_.find(key);if(window!=windows_.end())historyBytes_-=window->second.retainedBytes;windows_.erase(key);
}
void Store::replaceWindowHistory(const std::string& key,const std::vector<Line>& lines){
 resetWindow(key);Window& w=windows_.at(key);
 std::size_t start=lines.size()>10000?lines.size()-10000:0;
 for(std::size_t i=start;i<lines.size();++i){Line l=lines[i];l.index=w.next++;l.sourceIndex=l.index;l.generation=w.generation;l.retainedOrder=++retainedOrder_;l.capturedAt=0;l.text=clip(lines[i].text,4096);l.truncated=lines[i].truncated||lines[i].text.size()>l.text.size();w.lines.push_back(l);std::size_t bytes=l.text.size()+128;w.retainedBytes+=bytes;historyBytes_+=bytes;pruneHistory(w);}
 // This is a retained-history projection, not newly arriving output. resetWindow
 // records one active-capture gap; old lines must never be replayed into captures.
}
Counter Store::enable(const std::string& agent,const std::vector<std::string>& windows,bool drafts) {
 bounded(agent,64);require(windows.size()<=32,"too many windows");require(grants_.count(agent)||grants_.size()<8,"too many agents");
 for(const auto& key:windows)require(windows_.count(key)!=0,"unknown window");
 revoke(agent);Grant g;g.session=++serial_;g.events=1;g.nextMessage=1;g.drafts=drafts;if(archived_.count(agent)){g.messages=archived_.at(agent).messages;g.nextMessage=archived_.at(agent).nextMessage;archived_.erase(agent);}
 for(const auto& key:windows)g.windows[key]=windows_.at(key).generation;grants_[agent]=g;return g.session;
}
void Store::invalidate(const std::string& agent) {
 for(auto& p:proposals_)if(p.second.agent==agent&&p.second.status=="pending")p.second.status="expired";
 for(auto c=captures_.begin();c!=captures_.end();)if(c->second.agent==agent)c=captures_.erase(c);else ++c;
 for(auto& s:scenes_)for(auto& n:s.second.npcs)if(n.second.agent==agent){invalidateScene(s.second);break;}
}
void Store::revoke(const std::string& agent){invalidate(agent);auto it=grants_.find(agent);if(it!=grants_.end()){Grant summary=it->second;summary.session=0;summary.windows.clear();summary.editors.clear();summary.contexts.clear();summary.retries.clear();summary.drafts=false;for(auto& t:summary.tasks)if(t.second.status=="running"||t.second.status=="waiting")t.second.status="interrupted";archived_[agent]=summary;while(archived_.size()>8)archived_.erase(archived_.begin());grants_.erase(it);}}
void Store::revokeAll(){while(!grants_.empty())revoke(grants_.begin()->first);}
std::vector<Window> Store::humanWindows()const{std::vector<Window> out;for(const auto& p:windows_)out.push_back(metadata(p.second));return out;}
Grant Store::humanWorkspace(const std::string& a)const{if(grants_.count(a))return grants_.at(a);if(archived_.count(a))return archived_.at(a);Grant g;g.session=0;g.events=0;g.nextMessage=1;g.drafts=false;return g;}
Scene Store::humanScene(const std::string& key)const{require(scenes_.count(key),"unknown scene");return scenes_.at(key);}
std::vector<Scene> Store::humanScenes()const{std::vector<Scene> out;for(const auto& p:scenes_)out.push_back(p.second);return out;}
void Store::rejectAction(const std::string& key,const std::string& action){require(scenes_.count(key),"unknown scene");for(auto& a:scenes_.at(key).actions)if(a.id==action){a.status="rejected";return;}throw std::runtime_error("unknown action");}
void Store::removeNpc(const std::string& key,const std::string& npc){require(scenes_.count(key),"unknown scene");Scene& s=scenes_.at(key);s.npcs.erase(npc);if(s.turn==npc)s.turn.clear();invalidateScene(s);}
void Store::removeScene(const std::string& key){require(scenes_.count(key)&&scenes_.at(key).state=="stopped","stop scene first");scenes_.erase(key);}
const Grant& Store::authorize(const std::string& agent,Counter session)const {auto g=grants_.find(agent);require(g!=grants_.end()&&g->second.session==session,"access expired");return g->second;}
void Store::checkWindow(const Grant& g,const std::string& key)const {auto p=g.windows.find(key);auto w=windows_.find(key);require(p!=g.windows.end()&&w!=windows_.end()&&p->second==w->second.generation,"window not shared");}
std::vector<Window> Store::listWindows(const std::string& a,Counter s)const {const Grant& g=authorize(a,s);std::vector<Window> out;for(const auto& p:g.windows){checkWindow(g,p.first);out.push_back(metadata(windows_.at(p.first)));}return out;}
Output Store::read(const std::string& a,Counter s,const std::string& key,const Cursor* cursor,unsigned limit)const {
 const Grant& g=authorize(a,s);checkWindow(g,key);require(limit>=1&&limit<=200,"read limit outside bounds");const Window& w=windows_.at(key);
 Counter first=w.lines.empty()?w.next:w.lines.front().index;
 Output out;out.revision=w.revision;out.reset=cursor&&(cursor->window!=key||cursor->generation!=w.generation||cursor->next<first||cursor->next>w.next);
 Counter start=cursor&&!out.reset?cursor->next:std::max(first,w.next>limit?w.next-limit:Counter(0));std::size_t bytes=0;
 out.cursor.window=key;out.cursor.generation=w.generation;out.cursor.next=start;
 for(const auto& l:w.lines)if(l.index>=start){if(out.lines.size()>=limit||bytes+l.text.size()>32768)break;out.lines.push_back(l);bytes+=l.text.size();out.cursor.next=l.index+1;}
 out.more=out.cursor.next<w.next;return out;
}
Output Store::search(const std::string& a,Counter s,const std::string& key,const std::string& q,unsigned limit)const {
 const Grant& g=authorize(a,s);checkWindow(g,key);bounded(q,4096);require(limit>=1&&limit<=50,"search limit outside bounds");const Window& w=windows_.at(key);
 Output out;out.reset=false;out.more=false;out.revision=w.revision;out.cursor.window=key;out.cursor.generation=w.generation;out.cursor.next=w.next;std::size_t bytes=0;
 Counter start=w.next>2000?w.next-2000:0;
 for(const auto& l:w.lines)if(l.index>=start&&matches(l.text,q)){if(out.lines.size()==limit||bytes+l.text.size()>32768){out.more=true;break;}out.lines.push_back(l);bytes+=l.text.size();}return out;
}
Draft Store::prepareDraft(const std::string& a,Counter s,const std::string& key,const std::string& request,const std::string& text) {
 const Grant& cg=authorize(a,s);checkWindow(cg,key);require(cg.drafts,"draft permission required");bounded(request,64);bounded(text,32768);
 Grant& g=grants_.at(a);auto r=g.retries.find(request);if(r!=g.retries.end()){require(r->second.window==key&&r->second.text==text,"request ID content changed");return r->second;}
 require(g.retries.size()<16,"draft request limit");require(drafts_.size()<128,"session draft limit");
 unsigned count=0;for(const auto& d:drafts_)if(d.window==key)++count;require(count<8,"window draft limit");
 Draft d;d.id=id("draft-");d.agent=a;d.window=key;d.text=text;d.sent=false;d.opened=false;d.reviewRequired=true;drafts_.push_back(d);g.retries[request]=d;++g.events;return d;
}
void Store::postHuman(const std::string& a,const std::string& text){bounded(text,32768);require(grants_.count(a)!=0,"access expired");Grant& g=grants_.at(a);Message m;m.id=g.nextMessage++;m.role="human";m.text=text;g.messages.push_back(m);while(g.messages.size()>128)g.messages.pop_front();++g.events;}
Counter Store::postMessage(const std::string& a,Counter s,const std::string& text){authorize(a,s);postHuman(a,text);Grant& g=grants_.at(a);g.messages.back().role="agent";return g.messages.back().id;}
std::vector<Message> Store::messages(const std::string& a,Counter s,Counter after,unsigned limit)const{const Grant& g=authorize(a,s);require(limit>=1&&limit<=200,"message limit");std::vector<Message> out;std::size_t bytes=0;for(const auto& m:g.messages)if(m.id>after){if(out.size()==limit||bytes+m.text.size()>32768)break;out.push_back(m);bytes+=m.text.size();}return out;}
std::string Store::attachSelection(const std::string& a,Counter s,const std::string& key,const std::string& text,Counter time){const Grant& g=authorize(a,s);checkWindow(g,key);bounded(text,32768);require(g.contexts.size()<32,"context limit");Context c;c.id=id("context-");c.window=key;c.text=text;c.capturedAt=time;grants_.at(a).contexts[c.id]=c;++grants_.at(a).events;return c.id;}
Context Store::context(const std::string& a,Counter s,const std::string& key)const{const Grant& g=authorize(a,s);require(g.contexts.count(key)!=0,"context not shared");Context c=g.contexts.at(key);checkWindow(g,c.window);return c;}
void Store::putEditor(const Editor& e){bounded(e.id,128);require(windows_.count(e.window)!=0,"unknown window");require(editors_.count(e.id)||editors_.size()<128,"editor limit");editors_[e.id]=e;}
void Store::closeEditor(const std::string& key){editors_.erase(key);for(auto& g:grants_)g.second.editors.erase(key);for(auto& p:proposals_)if(p.second.editor==key&&p.second.status=="pending")p.second.status="expired";}
void Store::shareEditor(const std::string& a,Counter s,const std::string& key,bool share){const Grant& g=authorize(a,s);require(editors_.count(key)!=0,"unknown editor");checkWindow(g,editors_.at(key).window);if(share)grants_.at(a).editors.insert(key);else{grants_.at(a).editors.erase(key);for(auto& p:proposals_)if(p.second.agent==a&&p.second.editor==key&&p.second.status=="pending")p.second.status="expired";}++grants_.at(a).events;}
Editor Store::readEditor(const std::string& a,Counter s,const std::string& key)const{const Grant& g=authorize(a,s);require(g.editors.count(key)&&editors_.count(key),"editor not shared");Editor e=editors_.at(key);checkWindow(g,e.window);bounded(e.text,32768,true);return e;}
Proposal Store::proposeEdit(const std::string& a,Counter s,const std::string& key,Counter revision,const std::string& text,const std::string& explanation){Editor e=readEditor(a,s,key);require(authorize(a,s).drafts,"draft permission required");require(e.revision==revision&&!e.uploading,"stale or uploading editor");bounded(text,32768,true);bounded(explanation,4096,true);require(proposals_.size()<128,"proposal limit");Proposal p;p.id=id("edit-");p.agent=a;p.session=s;p.editor=key;p.revision=revision;p.text=text;p.explanation=explanation;p.status="pending";proposals_[p.id]=p;++grants_.at(a).events;return p;}
Editor Store::acceptEdit(const std::string& key){require(proposals_.count(key)!=0,"unknown proposal");Proposal& p=proposals_.at(key);require(p.status=="pending","proposal not pending");Editor e=readEditor(p.agent,p.session,p.editor);require(e.revision==p.revision&&!e.uploading,"stale or uploading editor");e.text=p.text;++e.revision;editors_[e.id]=e;p.status="accepted";++grants_.at(p.agent).events;return e;}
void Store::rejectEdit(const std::string& key){require(proposals_.count(key)!=0,"unknown proposal");proposals_.at(key).status="rejected";}
Task Store::startTask(const std::string& a,Counter s,const std::string& title,Counter now){const Grant& g=authorize(a,s);bounded(title,256);require(g.tasks.size()<16,"task limit");Task t;t.id=id("task-");t.title=title;t.status="running";t.heartbeat=now;t.cancellationRequested=false;grants_.at(a).tasks[t.id]=t;++grants_.at(a).events;return t;}
Task Store::updateTask(const std::string& a,Counter s,const std::string& key,const std::string& status,const std::string& progress,Counter now){const Grant& g=authorize(a,s);require(g.tasks.count(key)!=0,"unknown task");require(status=="running"||status=="waiting"||status=="completed"||status=="failed"||status=="cancelled","invalid task status");bounded(progress,4096,true);Task& t=grants_.at(a).tasks.at(key);t.status=status;t.progress=progress;t.heartbeat=now;++grants_.at(a).events;return t;}
void Store::cancelTask(const std::string& a,const std::string& key){require(grants_.count(a)&&grants_.at(a).tasks.count(key),"unknown task");grants_.at(a).tasks.at(key).cancellationRequested=true;++grants_.at(a).events;}
std::string Store::startCapture(const std::string& a,Counter s,const std::string& key,const std::string& name){checkWindow(authorize(a,s),key);bounded(name,128,true);require(captures_.size()<8,"capture limit");Capture c;c.id=id("capture-");c.agent=a;c.window=key;c.name=name;c.active=true;c.next=c.dropped=c.gaps=0;c.bytes=0;captures_[c.id]=c;return c.id;}
void Store::stopCapture(const std::string& key){require(captures_.count(key),"unknown capture");captures_.at(key).active=false;}
void Store::removeCapture(const std::string& key){captures_.erase(key);}
std::vector<Capture> Store::captures(const std::string& a,Counter s)const{const Grant& g=authorize(a,s);std::vector<Capture> out;for(const auto& p:captures_)if(p.second.agent==a){checkWindow(g,p.second.window);Capture c=p.second;c.lines.clear();out.push_back(c);}return out;}
Output Store::readCapture(const std::string& a,Counter s,const std::string& key,Counter cursor,unsigned limit,const std::string& q)const{const Grant& g=authorize(a,s);require(captures_.count(key)&&captures_.at(key).agent==a,"capture not shared");const Capture& c=captures_.at(key);checkWindow(g,c.window);require(limit>=1&&limit<=(q.empty()?200:50),"capture limit");bounded(q,4096,true);Output out;out.revision=c.next;Counter first=c.lines.empty()?c.next:c.lines.front().index;out.reset=cursor<first||cursor>c.next;Counter start=out.reset?first:cursor;out.cursor.window=key;out.cursor.generation=1;out.cursor.next=start;std::size_t bytes=0;for(const auto& l:c.lines)if(l.index>=start){if(!q.empty()&&!matches(l.text,q)){out.cursor.next=l.index+1;continue;}if(out.lines.size()==limit||bytes+l.text.size()>32768)break;out.lines.push_back(l);bytes+=l.text.size();out.cursor.next=l.index+1;}out.more=out.cursor.next<c.next;return out;}
void Store::invalidateScene(Scene& s){++s.revision;for(auto& a:s.actions)if(a.status=="pending"||a.status=="staged")a.status="expired";}
std::string Store::createScene(const std::string& title){bounded(title,256);require(scenes_.size()<8,"scene limit");Scene s;s.id=id("scene-");s.title=title;s.state="paused";s.revision=1;scenes_[s.id]=s;return s.id;}
void Store::directScene(const std::string& key,const std::string& notes,const std::string& direction,const std::string& turn,const std::string& state){require(scenes_.count(key),"unknown scene");bounded(notes,32768,true);bounded(direction,32768,true);require(state=="paused"||state=="running"||state=="stopped","invalid scene state");Scene& s=scenes_.at(key);std::size_t bytes=s.title.size()+notes.size()+direction.size();for(const auto& p:s.npcs)bytes+=p.second.name.size()+p.second.summary.size()+p.second.brief.size()+p.second.knowledge.size();require(bytes<=65536,"scene byte limit");require(turn.empty()||s.npcs.count(turn),"unknown turn");s.notes=notes;s.direction=direction;s.turn=state=="stopped"?"":turn;s.state=state;invalidateScene(s);}
void Store::assignNpc(const std::string& key,const Npc& n){require(scenes_.count(key),"unknown scene");bounded(n.id,128);bounded(n.name,128);bounded(n.brief,16384,true);bounded(n.knowledge,16384,true);bounded(n.summary,4096,true);bounded(n.posePrefix,256,true);bounded(n.dialoguePrefix,256,true);require(n.posePrefix.find_first_of("\r\n")==std::string::npos&&n.dialoguePrefix.find_first_of("\r\n")==std::string::npos,"multiline prefix");Scene& s=scenes_.at(key);require(s.npcs.count(n.id)||s.npcs.size()<16,"NPC limit");std::size_t bytes=s.title.size()+s.notes.size()+s.direction.size()+n.name.size()+n.summary.size()+n.brief.size()+n.knowledge.size();for(const auto& p:s.npcs)if(p.first!=n.id)bytes+=p.second.name.size()+p.second.summary.size()+p.second.brief.size()+p.second.knowledge.size();require(bytes<=65536,"scene byte limit");s.npcs[n.id]=n;invalidateScene(s);}
Scene Store::scene(const std::string& a,Counter session,const std::string& key)const{const Grant& g=authorize(a,session);require(scenes_.count(key),"unknown scene");Scene out=scenes_.at(key);bool own=false;for(auto& p:out.npcs)if(p.second.agent==a){checkWindow(g,p.second.window);own=true;}else{p.second.brief.clear();p.second.knowledge.clear();p.second.posePrefix.clear();p.second.dialoguePrefix.clear();}require(own,"scene not assigned");for(auto p=out.actions.begin();p!=out.actions.end();)if(p->agent!=a&&p->status!="queued")p=out.actions.erase(p);else ++p;return out;}
std::vector<Scene> Store::scenes(const std::string& a,Counter s)const{authorize(a,s);std::vector<Scene> out;for(const auto& p:scenes_)try{out.push_back(scene(a,s,p.first));}catch(const std::runtime_error&){}return out;}
Action Store::proposeAction(const std::string& a,Counter session,const std::string& key,const std::string& npc,Counter revision,const std::string& kind,const std::string& text,const std::string& request){Scene view=scene(a,session,key);require(authorize(a,session).drafts,"draft permission required");bounded(text,4096);bounded(request,64);require(text.find_first_of("\r\n")==std::string::npos,"action must be single line");require(kind=="pose"||kind=="dialogue","invalid action kind");Scene& s=scenes_.at(key);require(s.npcs.count(npc)&&s.npcs.at(npc).agent==a,"NPC not assigned");for(const auto& old:s.actions)if(old.agent==a&&old.session==session&&old.request==request){require(old.npc==npc&&old.requestText==text&&old.kind==kind&&old.revision==revision,"request ID content changed");require(old.status!="expired","action expired");return old;}require(s.state=="running"&&s.turn==npc&&s.revision==revision,"paused, wrong turn or stale scene");Action action;action.id=id("action-");action.agent=a;action.session=session;action.npc=npc;action.kind=kind;action.text=text;action.requestText=text;action.request=request;action.status="pending";action.revision=revision;s.actions.push_back(action);std::size_t bytes=0;for(const auto& record:s.actions)bytes+=record.text.size();while(s.actions.size()>32||bytes>65536){bytes-=s.actions.front().text.size();s.actions.pop_front();}return action;}
Draft Store::stageAction(const std::string& key,const std::string& action){require(scenes_.count(key),"unknown scene");Scene& s=scenes_.at(key);for(auto& a:s.actions)if(a.id==action){require(a.status=="pending"&&a.revision==s.revision&&s.state=="running"&&s.turn==a.npc,"action expired");const Npc& n=s.npcs.at(a.npc);Draft d=prepareDraft(a.agent,a.session,n.window,a.id,(a.kind=="pose"?n.posePrefix:n.dialoguePrefix)+a.text);a.status="staged";staged_[d.id]=std::make_pair(key,action);return d;}throw std::runtime_error("unknown action");}
void Store::editAction(const std::string& key,const std::string& action,const std::string& prose){bounded(prose,4096);require(prose.find_first_of("\r\n")==std::string::npos,"prose must be single line");require(scenes_.count(key),"unknown scene");Scene& s=scenes_.at(key);for(auto& a:s.actions)if(a.id==action){require(a.status=="pending"&&a.revision==s.revision&&s.state=="running"&&s.turn==a.npc,"action expired");checkWindow(authorize(a.agent,a.session),s.npcs.at(a.npc).window);a.text=prose;return;}throw std::runtime_error("unknown action");}
bool Store::validateStagedDraft(const std::string& draft)const{try{auto binding=staged_.find(draft);require(binding!=staged_.end(),"draft has no scene binding");const Scene& s=scenes_.at(binding->second.first);for(const auto& a:s.actions)if(a.id==binding->second.second){require(a.status=="staged"&&a.revision==s.revision&&s.state=="running"&&s.turn==a.npc,"scene draft stale");checkWindow(authorize(a.agent,a.session),s.npcs.at(a.npc).window);return true;}}catch(const std::exception&){}return false;}
bool Store::validateStagedDraft(const std::string& draft,const std::string& actualCommand)const{if(!validateStagedDraft(draft))return false;for(const auto& d:drafts_)if(d.id==draft)return d.text==actualCommand;return false;}
bool Store::hasSceneBinding(const std::string& draft)const{return staged_.count(draft)!=0;}
void Store::markStagedQueued(const std::string& draft){require(validateStagedDraft(draft),"scene draft stale");const auto& binding=staged_.at(draft);for(auto& a:scenes_.at(binding.first).actions)if(a.id==binding.second){a.status="queued";return;}}
}
