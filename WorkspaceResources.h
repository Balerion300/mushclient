#pragma once
#include <afxwin.h>
struct WorkspaceResourceState { HINSTANCE original;int depth;WorkspaceResourceState():original(NULL),depth(0){} };
inline WorkspaceResourceState& WorkspaceResourcesState(){static thread_local WorkspaceResourceState state;return state;}
// Native framework strings are in the EXE; legacy dialogs retain their DLL.
class WorkspaceModernResources
{
 HINSTANCE previous;
public:
 WorkspaceModernResources():previous(AfxGetResourceHandle()) {WorkspaceResourceState& state=WorkspaceResourcesState();if(state.depth++==0)state.original=previous;AfxSetResourceHandle(GetModuleHandle(NULL));}
 ~WorkspaceModernResources(){AfxSetResourceHandle(previous);--WorkspaceResourcesState().depth;}
};
// Application callbacks must execute with the original locale resource handle.
class WorkspaceApplicationResources
{
 HINSTANCE previous;
public:
 WorkspaceApplicationResources():previous(AfxGetResourceHandle()){WorkspaceResourceState& state=WorkspaceResourcesState();if(state.depth)AfxSetResourceHandle(state.original);}
 ~WorkspaceApplicationResources(){AfxSetResourceHandle(previous);}
};
inline bool WorkspaceFrameworkResourceMessage(UINT message,LPARAM lp)
{
 const bool tooltip=message==WM_NOTIFY && lp && (((NMHDR*)lp)->code==TTN_NEEDTEXTA || ((NMHDR*)lp)->code==TTN_NEEDTEXTW);
 return message==WM_CREATE || message==WM_PAINT || message==WM_NCPAINT || tooltip;
}
