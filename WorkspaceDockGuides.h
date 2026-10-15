#pragma once
class CMDIFrameWndEx;
class CDockingManager;
// Original presentation and hit regions around the native MFC docking engine.
void InstallWorkspaceDockGuides(CMDIFrameWndEx& frame,CDockingManager& docking);
