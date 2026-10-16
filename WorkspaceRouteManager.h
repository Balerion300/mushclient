#pragma once
class CMainFrame;
void OpenWorkspaceRouteManager(CMainFrame *frame,
                               const CString &originalConnection,
                               const CString &channelName = CString(),
                               bool addChannel = false);
