#pragma once
class CMainFrame;
void OpenWorkspaceTool(CMainFrame& frame,UINT command);
void ShutdownWorkspaceTools();
BOOL WorkspaceTranslateMessage(MSG* message);
