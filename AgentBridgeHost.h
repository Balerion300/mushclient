#pragma once
#include "AgentWorkspaceCore.h"
#include <memory>
#include <string>
namespace agent_workspace {
// Construct/start/stop on the UI thread. Owns a hidden UI request window.
// Worker performs bounded socket IO; Store is accessed only by that UI window.
class BridgeHost {
 public:
  struct State;
  BridgeHost(); ~BridgeHost();
  std::wstring start(Store& store,const std::string& agent,Counter session);
  void stop();
  bool running() const;
  std::wstring endpoint() const;
 private:
  std::shared_ptr<State> state_;
  BridgeHost(const BridgeHost&); BridgeHost& operator=(const BridgeHost&);
};
}
