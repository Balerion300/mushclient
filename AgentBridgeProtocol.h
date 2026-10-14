#pragma once
#include "AgentWorkspaceCore.h"
namespace agent_workspace {
// UI-thread dispatch only. Authentication belongs to AgentBridgeHost.
std::string dispatch(Store& store,const std::string& agent,Counter session,const std::string& request);
bool validateToolArguments(const std::string& tool,const std::string& arguments);
}
