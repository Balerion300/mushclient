// Independently authored MUSHclient agent model. No network or game commands.
#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace agent_workspace {
typedef std::uint64_t Counter;
struct Line { Counter index; std::string text; Counter receivedAt, capturedAt, sourceIndex, generation, retainedOrder; bool truncated; Line():index(0),receivedAt(0),capturedAt(0),sourceIndex(0),generation(0),retainedOrder(0),truncated(false){} };
struct Cursor { std::string window; Counter generation, next; Cursor():generation(0),next(0){} };
struct Output { std::vector<Line> lines; Cursor cursor; Counter revision; bool reset, more; };
struct Window { std::string id, world, title; Counter generation, revision, next; std::deque<Line> lines; std::size_t retainedBytes; Window():generation(0),revision(0),next(0),retainedBytes(0){} };
struct Draft { std::string id, agent, window, text; bool sent, opened, reviewRequired; };
struct Message { Counter id; std::string role, text; };
struct Editor { std::string id, window, text; Counter revision; bool uploading; };
struct Proposal { std::string id, agent, editor, text, explanation, status; Counter revision, session; };
struct Task { std::string id, title, status, progress; Counter heartbeat; bool cancellationRequested; };
struct Context { std::string id, window, text; Counter capturedAt; };
struct Capture { std::string id, agent, window, name; bool active; Counter next, dropped, gaps; std::deque<Line> lines; std::size_t bytes; };
struct Grant { Counter session, events, nextMessage; bool drafts; std::map<std::string,Counter> windows; std::set<std::string> editors; std::map<std::string,Draft> retries; std::deque<Message> messages; std::map<std::string,Context> contexts; std::map<std::string,Task> tasks; };
struct Npc { std::string id, agent, window, name, summary, brief, knowledge, posePrefix, dialoguePrefix; };
struct Action { std::string id, agent, npc, kind, text, requestText, request, status; Counter revision, session; };
struct Scene { std::string id, title, notes, direction, turn, state; Counter revision; std::map<std::string,Npc> npcs; std::deque<Action> actions; };
class Store {
 public:
  Store();
  void addWindow(const std::string& id,const std::string& world,const std::string& title);
  void append(const std::string& id,const std::string& completedText,Counter receivedAt=0);
  void resetWindow(const std::string& id);
  void replaceWindowHistory(const std::string& id,const std::vector<Line>& retainedLines);
  void expireWindow(const std::string& id);
  Counter enable(const std::string& agent,const std::vector<std::string>& windows,bool drafts);
  void revoke(const std::string& agent); void revokeAll();
  std::vector<Window> humanWindows() const;
  Grant humanWorkspace(const std::string& agent) const;
  Scene humanScene(const std::string& id) const;
  std::vector<Scene> humanScenes() const;
  void rejectAction(const std::string& scene,const std::string& action);
  void removeNpc(const std::string& scene,const std::string& npc);
  void removeScene(const std::string& scene);
  const Grant& authorize(const std::string& agent,Counter session) const;
  std::vector<Window> listWindows(const std::string& agent,Counter session) const;
  Output read(const std::string& agent,Counter session,const std::string& window,const Cursor* cursor=0,unsigned limit=100) const;
  Output search(const std::string& agent,Counter session,const std::string& window,const std::string& text,unsigned limit=50) const;
  Draft prepareDraft(const std::string& agent,Counter session,const std::string& window,const std::string& request,const std::string& text);
  const std::vector<Draft>& drafts() const { return drafts_; }
  void postHuman(const std::string& agent,const std::string& text);
  Counter postMessage(const std::string& agent,Counter session,const std::string& text);
  std::vector<Message> messages(const std::string& agent,Counter session,Counter after,unsigned limit=100) const;
  std::string attachSelection(const std::string& agent,Counter session,const std::string& window,const std::string& text,Counter time);
  Context context(const std::string& agent,Counter session,const std::string& id) const;
  void putEditor(const Editor& editor); void closeEditor(const std::string& id);
  void shareEditor(const std::string& agent,Counter session,const std::string& id,bool share);
  Editor readEditor(const std::string& agent,Counter session,const std::string& id) const;
  Proposal proposeEdit(const std::string& agent,Counter session,const std::string& editor,Counter revision,const std::string& text,const std::string& explanation);
  Editor acceptEdit(const std::string& proposal); void rejectEdit(const std::string& proposal);
  const std::map<std::string,Proposal>& proposals() const { return proposals_; }
  Task startTask(const std::string& agent,Counter session,const std::string& title,Counter now);
  Task updateTask(const std::string& agent,Counter session,const std::string& id,const std::string& status,const std::string& progress,Counter now);
  void cancelTask(const std::string& agent,const std::string& id);
  std::string startCapture(const std::string& agent,Counter session,const std::string& window,const std::string& name);
  void stopCapture(const std::string& id); void removeCapture(const std::string& id);
  std::vector<Capture> captures(const std::string& agent,Counter session) const;
  Output readCapture(const std::string& agent,Counter session,const std::string& id,Counter cursor,unsigned limit=100,const std::string& searchText="") const;
  std::string createScene(const std::string& title);
  void directScene(const std::string& id,const std::string& notes,const std::string& direction,const std::string& turn,const std::string& state);
  void assignNpc(const std::string& scene,const Npc& npc);
  std::vector<Scene> scenes(const std::string& agent,Counter session) const;
  Scene scene(const std::string& agent,Counter session,const std::string& id) const;
  Action proposeAction(const std::string& agent,Counter session,const std::string& scene,const std::string& npc,Counter revision,const std::string& kind,const std::string& text,const std::string& request);
  Draft stageAction(const std::string& scene,const std::string& action);
  void editAction(const std::string& scene,const std::string& action,const std::string& prose);
  bool validateStagedDraft(const std::string& draft) const;
  bool hasSceneBinding(const std::string& draft) const;
  bool validateStagedDraft(const std::string& draft,const std::string& actualCommand) const;
  void markStagedQueued(const std::string& draft);
 private:
  Counter serial_, retainedOrder_; std::size_t historyBytes_; std::map<std::string,Window> windows_; std::map<std::string,Grant> grants_, archived_;
  std::map<std::string,Editor> editors_; std::map<std::string,Proposal> proposals_;
  std::map<std::string,Capture> captures_; std::map<std::string,Scene> scenes_; std::vector<Draft> drafts_;
  std::map<std::string,std::pair<std::string,std::string>> staged_;
  std::string id(const char* prefix); void checkWindow(const Grant&,const std::string&) const;
  void pruneHistory(Window&); void removeOldest(Window&); Window metadata(const Window&) const;
  void invalidate(const std::string& agent); void invalidateScene(Scene&);
};
}
