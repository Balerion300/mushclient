#pragma once
#include <string>
#include <vector>
#include <windows.h>

struct LogStartSetupPlugin {
  std::wstring name, id;
  std::string namingConfiguration;
};

struct LogStartSetupOptions {
  std::wstring worldName;
  std::wstring script, plugin;
  std::vector<LogStartSetupPlugin> plugins; // This world's enabled plugins only.
  std::wstring defaultFolder;
  std::string namingConfiguration;
  bool saveNamingConfiguration = false;
};

// Edits settings only after Save. It never starts logging or executes a script.
// An empty script selects normal logging and clears the plugin ID.
bool ShowLogStartSetup(HWND owner, LogStartSetupOptions &options);
