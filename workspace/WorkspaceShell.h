#pragma once
#include <functional>
#include <string>
#include <vector>
#include <windows.h>

class CMDIFrameWndEx;
namespace workspace_shell {
enum Action {ToggleWorlds,Connect,Disconnect,Find,Logging,Channels,Tools,Select,OpenWorld,Legacy,ThemeSystem,ThemeLight,ThemeDark};
struct Row {
  std::string world,name;
  std::wstring label;
  bool channel=false,connected=false,connecting=false,disconnecting=false,logging=false;
  unsigned unread=0,mentions=0;
  int priority=0;
};
struct Snapshot {std::vector<Row> rows;std::string world,name;bool legacy=false;};
typedef std::function<void(Action,const std::string&,const std::string&)> Handler;
struct Target {HWND window=NULL;UINT_PTR document=0;};
bool ActivateOriginal(const std::string& identity,const std::function<Target(const std::string&)>& resolve,const std::function<void(HWND)>& activate,const std::function<Target()>& active);
}
// Native chrome only. This owner never reads/writes transcript text or submits
// input. Original world/name identities remain separate from visible labels.
class CWorkspaceShell {
  struct Impl;Impl* impl_;
  CWorkspaceShell(const CWorkspaceShell&);CWorkspaceShell& operator=(const CWorkspaceShell&);
public:
  CWorkspaceShell();~CWorkspaceShell();
  bool Create(CMDIFrameWndEx& frame,const workspace_shell::Handler& handler);
  void Update(const workspace_shell::Snapshot& snapshot);
  void ToggleNavigator();
  bool NavigatorVisible() const;
  void ShowNavigator(bool show);
  bool OwnsFocus() const;
  bool PreTranslate(MSG* message);
};
