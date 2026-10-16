#include "stdafx.h"
#include "MUSHclient.h"
#include "WorkspaceRouteManager.h"
#include "WorkspaceIntegration.h"
#include "doc.h"
#include "mainfrm.h"
#include "workspace/WorkspaceInputPolicy.h"
#include "workspace/WorkflowUi.h"
#include <algorithm>
#include <commctrl.h>
#include <cstring>
namespace {
std::wstring RouteWide(const char *value) {
  if (!value)
    return std::wstring();
  size_t length = strnlen(value, 4097);
  if (length > 4096)
    return std::wstring();
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                                  static_cast<int>(length), NULL, 0);
  std::wstring result(count, L'\0');
  if (count)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                        static_cast<int>(length), &result[0], count);
  return result;
}
std::wstring WorldLabel(const char *value) {
  auto label = RouteWide(value);
  if (!label.empty() || !value || !*value)
    return label;
  size_t length = strnlen(value, 4097);
  if (length > 4096)
    return L"World";
  int count =
      MultiByteToWideChar(CP_ACP, 0, value, static_cast<int>(length), NULL, 0);
  label.assign(count, L'\0');
  if (count)
    MultiByteToWideChar(CP_ACP, 0, value, static_cast<int>(length), &label[0],
                        count);
  return label.empty() ? L"World" : label;
}
std::wstring Read(HWND window, unsigned maximum) {
  int size = ::GetWindowTextLengthW(window);
  if (size < 0 || static_cast<unsigned>(size) > maximum)
    throw std::runtime_error("A route field exceeds its character limit.");
  std::wstring result(size + 1, L'\0');
  ::GetWindowTextW(window, &result[0], size + 1);
  result.resize(size);
  return result;
}
bool RouteUtf8(const std::wstring &text, std::string &result) {
  if (text.empty()) {
    result.clear();
    return true;
  }
  int size =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), NULL, 0, NULL, NULL);
  if (!size)
    return false;
  result.assign(size, '\0');
  return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                             static_cast<int>(text.size()), &result[0], size,
                             NULL, NULL) != 0;
}
struct SpawnOptions {
  CMainFrame* frame;
  CString identity,name;
  workspace_spawn::Preferences preferences;
};
HWND OptionControl(HWND dialog,const wchar_t* kind,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height)
{
  RECT bounds={x,y,x+width,y+height};MapDialogRect(dialog,&bounds);
  HWND control=::CreateWindowExW(!wcscmp(kind,L"EDIT")?WS_EX_CLIENTEDGE:0,kind,text,WS_CHILD|WS_VISIBLE|style,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,dialog,(HMENU)(INT_PTR)id,AfxGetInstanceHandle(),NULL);
  ::SendMessageW(control,WM_SETFONT,::SendMessageW(dialog,WM_GETFONT,0,0),TRUE);return control;
}
void UpdateSpawnAppearance(HWND dialog,const SpawnOptions& options)
{
  const bool inherit=IsDlgButtonChecked(dialog,105)==BST_CHECKED;
  for(int id:{106,107,108})EnableWindow(GetDlgItem(dialog,id),!inherit);
  auto appearance=options.preferences.appearance;if(inherit)WorkspaceWorldAppearance(options.identity,appearance);
  wchar_t color[64]={};swprintf_s(color,L"Text color #%02X%02X%02X",GetRValue(appearance.foreground),GetGValue(appearance.foreground),GetBValue(appearance.foreground));SetDlgItemTextW(dialog,107,color);
  swprintf_s(color,L"Background #%02X%02X%02X",GetRValue(appearance.background),GetGValue(appearance.background),GetBValue(appearance.background));SetDlgItemTextW(dialog,108,color);
  const std::wstring summary=inherit?L"Uses this world's Main font and colors; future changes follow Main.":appearance.face+L", "+std::to_wstring(appearance.pointSize)+L" pt. ANSI colors remain available.";
  SetDlgItemTextW(dialog,109,summary.c_str());
}
INT_PTR CALLBACK SpawnOptionsProc(HWND dialog,UINT message,WPARAM wp,LPARAM lp)
{
  auto* options=(SpawnOptions*)GetWindowLongPtrW(dialog,DWLP_USER);
  if(message==WM_INITDIALOG){
    options=(SpawnOptions*)lp;SetWindowLongPtrW(dialog,DWLP_USER,lp);
    SetWindowTextW(dialog,(L"Transcript options: "+RouteWide(options->name)).c_str());
    auto label=[&](const wchar_t* text,int x,int y,int width,int height){OptionControl(dialog,L"STATIC",text,SS_LEFT,-1,x,y,width,height);};
    label(L"Mention text (optional, literal, case-insensitive)",10,10,280,12);
    HWND mention=OptionControl(dialog,L"EDIT",options->preferences.mention.c_str(),WS_TABSTOP|ES_AUTOHSCROLL,101,10,25,280,16);::SendMessageW(mention,EM_SETLIMITTEXT,128,0);
    label(L"Mentions add unread badges only. No sound or focus change.",10,43,280,12);
    label(L"Priority (-100 to 100; higher comes first)",10,60,220,14);
    HWND priority=OptionControl(dialog,L"EDIT",std::to_wstring(options->preferences.priority).c_str(),WS_TABSTOP|ES_AUTOHSCROLL,102,238,57,52,17);::SendMessageW(priority,EM_SETLIMITTEXT,4,0);
    label(L"Applies to the route list and pane navigation; docking stays as arranged.",10,79,280,14);
    label(L"Focus shortcut (optional)",10,98,280,12);
    HWND hotkey=OptionControl(dialog,HOTKEY_CLASSW,L"",WS_TABSTOP,103,10,113,215,18);::SendMessageW(hotkey,HKM_SETHOTKEY,options->preferences.shortcut,0);
    OptionControl(dialog,L"BUTTON",L"Clear",WS_TABSTOP|BS_PUSHBUTTON,104,233,113,57,18);
    label(L"Ctrl+Alt with a letter/digit, or a modified F key. F4/F6/F10, existing commands and Ctrl+Shift+F1-F9 are reserved. AltGr never moves focus.",10,136,280,27);
    label(L"Ctrl+Alt+PageUp / PageDown moves through priority order when unused by existing commands.",10,166,280,16);
    OptionControl(dialog,L"BUTTON",L"Use Main font and colors",WS_TABSTOP|BS_AUTOCHECKBOX,105,10,188,280,18);
    CheckDlgButton(dialog,105,options->preferences.appearanceOverride?BST_UNCHECKED:BST_CHECKED);
    if(!options->preferences.appearanceOverride)WorkspaceWorldAppearance(options->identity,options->preferences.appearance);
    OptionControl(dialog,L"BUTTON",L"Font...",WS_TABSTOP|BS_PUSHBUTTON,106,10,212,70,18);
    OptionControl(dialog,L"BUTTON",L"Text color...",WS_TABSTOP|BS_PUSHBUTTON,107,87,212,98,18);
    OptionControl(dialog,L"BUTTON",L"Background...",WS_TABSTOP|BS_PUSHBUTTON,108,192,212,98,18);
    OptionControl(dialog,L"STATIC",L"",SS_LEFT,109,10,236,280,20);UpdateSpawnAppearance(dialog,*options);
    OptionControl(dialog,L"BUTTON",L"Save options",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,163,263,61,18);
    OptionControl(dialog,L"BUTTON",L"Cancel",WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL,231,263,59,18);
    return TRUE;
  }
  if(!options)return FALSE;
  if(message==WM_CLOSE){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(message!=WM_COMMAND)return FALSE;
  if(LOWORD(wp)==104){SendDlgItemMessageW(dialog,103,HKM_SETHOTKEY,0,0);return TRUE;}
  if(LOWORD(wp)==105){UpdateSpawnAppearance(dialog,*options);return TRUE;}
  if(LOWORD(wp)==106){
    auto& appearance=options->preferences.appearance;LOGFONTW font={};HDC dc=GetDC(dialog);const int dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(dialog,dc);font.lfHeight=-MulDiv(appearance.pointSize,dpi,72);font.lfWeight=appearance.weight;font.lfCharSet=(BYTE)appearance.charset;wcscpy_s(font.lfFaceName,appearance.face.c_str());
    CHOOSEFONTW chooser={sizeof(chooser)};chooser.hwndOwner=dialog;chooser.lpLogFont=&font;chooser.Flags=CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_LIMITSIZE;chooser.nSizeMin=1;chooser.nSizeMax=72;
    if(ChooseFontW(&chooser)){appearance.face=font.lfFaceName;appearance.pointSize=max(1,chooser.iPointSize/10);appearance.weight=font.lfWeight;appearance.charset=font.lfCharSet;UpdateSpawnAppearance(dialog,*options);}return TRUE;
  }
  if(LOWORD(wp)==107 || LOWORD(wp)==108){
    static COLORREF custom[16]={};auto& appearance=options->preferences.appearance;CHOOSECOLORW chooser={sizeof(chooser)};chooser.hwndOwner=dialog;chooser.lpCustColors=custom;chooser.rgbResult=LOWORD(wp)==107?appearance.foreground:appearance.background;chooser.Flags=CC_FULLOPEN|CC_RGBINIT;
    if(ChooseColorW(&chooser)){if(LOWORD(wp)==107)appearance.foreground=chooser.rgbResult;else appearance.background=chooser.rgbResult;UpdateSpawnAppearance(dialog,*options);}return TRUE;
  }
  if(LOWORD(wp)==IDCANCEL){EndDialog(dialog,IDCANCEL);return TRUE;}
  if(LOWORD(wp)==IDOK){
    try {
      auto preferences=options->preferences;preferences.mention=Read(GetDlgItem(dialog,101),128);
      preferences.appearanceOverride=IsDlgButtonChecked(dialog,105)!=BST_CHECKED;
      auto priority=Read(GetDlgItem(dialog,102),4);wchar_t* end=NULL;long number=wcstol(priority.c_str(),&end,10);
      if(priority.empty() || !end || *end || number < -100 || number>100){MessageBoxW(dialog,L"Choose a priority from -100 to 100.",L"Transcript options",MB_ICONWARNING);return TRUE;}
      preferences.priority=(int)number;preferences.shortcut=(unsigned short)SendDlgItemMessageW(dialog,103,HKM_GETHOTKEY,0,0);
      CString error;if(!options->frame->SetWorkspaceTranscriptPreferences(options->identity,options->name,preferences,error)){MessageBoxW(dialog,RouteWide(error).c_str(),L"Transcript options",MB_ICONWARNING);return TRUE;}
      options->preferences=preferences;EndDialog(dialog,IDOK);
    }catch(const std::exception& error){MessageBoxW(dialog,RouteWide(error.what()).c_str(),L"Transcript options",MB_ICONWARNING);}
    return TRUE;
  }
  return FALSE;
}
void ShowSpawnOptions(HWND owner,SpawnOptions& options)
{
  std::vector<unsigned char> bytes(sizeof(DLGTEMPLATE),0);
  auto* dialog=(DLGTEMPLATE*)bytes.data();dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;dialog->cx=300;dialog->cy=290;
  auto append=[&](const void* value,size_t size){const auto* begin=(const unsigned char*)value;bytes.insert(bytes.end(),begin,begin+size);};
  WORD zero=0,fontSize=9;append(&zero,sizeof(zero));append(&zero,sizeof(zero));
  const wchar_t title[]=L"Transcript options";append(title,sizeof(title));append(&fontSize,sizeof(fontSize));const wchar_t face[]=L"Segoe UI";append(face,sizeof(face));
  DialogBoxIndirectParamW(AfxGetInstanceHandle(),(DLGTEMPLATE*)bytes.data(),owner,SpawnOptionsProc,(LPARAM)&options);
}
} // namespace
// This modeless tool retains its original connection regardless of world focus.
class CWorkspaceRouteDialog : public CWnd {
  CMainFrame *frame;
  CString identity;
  CEdit name, pattern, prefix;
  CListBox routes;
  CButton regex, omit, followingImages, advancedButton, optionsButton, save, cancel, newRoute, removeRoute, channelExample,
      tellExample;
  CStatic nameLabel, patternLabel, prefixLabel, destination, hint, feedback;
  CFont font;
  std::vector<WorkspaceRoute> visibleRoutes;
  int editing;
  bool advanced=false;
  int scrollOffset=0,contentHeight=0;
  UINT dpi;
  int Scale(int value) const { return MulDiv(value, dpi, 96); }
  void Feedback(const std::wstring &value) {
    ::SetWindowTextW(feedback.m_hWnd, value.c_str());
  }
  void Font() {
    workflow_ui::Polish(m_hWnd);
  }
  void Destination() {
    CString world = "Closed world";
    bool found = false;
    POSITION position = App.m_pWorldDocTemplate->GetFirstDocPosition();
    while (position) {
      CMUSHclientDoc *doc =
          (CMUSHclientDoc *)App.m_pWorldDocTemplate->GetNextDoc(position);
      if (WorkspaceConnectionId(doc) == identity) {
        world = doc->m_mush_name;
        found = true;
        break;
      }
    }
    std::wstring label = L"Original world: " + WorldLabel((LPCTSTR)world) +
                         (WorkspaceIsConnectionLive(identity)
                              ? L" (connected)"
                              : L" (disconnected; drafts stay local)");
    ::SetWindowTextW(destination.m_hWnd, label.c_str());
    save.EnableWindow(found);
    channelExample.EnableWindow(found);
    tellExample.EnableWindow(found);
  }
  void Layout() {
    if (!routes.GetSafeHwnd())
      return;
    CRect client;
    GetClientRect(client);
    int pad = Scale(12), width = max(120, client.Width() - 2 * pad),
        listHeight = Scale(88);
    contentHeight=Scale(594+(advanced?76:0));
    scrollOffset=max(0,min(scrollOffset,max(0,contentHeight-client.Height())));
    SCROLLINFO scrolling={sizeof(scrolling),SIF_RANGE|SIF_PAGE|SIF_POS,0,contentHeight-1,(UINT)max(1,client.Height()),scrollOffset,0};
    SetScrollInfo(SB_VERT,&scrolling,TRUE);
    int y = pad;
    auto move=[&](CWnd& control,int x,int row,int w,int height){control.MoveWindow(x,row-scrollOffset,w,height);};
    move(destination,pad,y,width,Scale(34));y+=Scale(40);
    const int third=(width-Scale(16))/3;
    CWnd* common[]={&channelExample,&tellExample,&newRoute};
    for(int i=0;i<3;++i)move(*common[i],pad+i*(third+Scale(8)),y,third,Scale(30));y+=Scale(38);
    move(routes,pad,y,width,listHeight);y+=listHeight+Scale(8);
    const int half=(width-Scale(8))/2;
    move(optionsButton,pad,y,half,Scale(28));move(removeRoute,pad+half+Scale(8),y,half,Scale(28));y+=Scale(36);
    CWnd* labels[]={&nameLabel,&patternLabel,&prefixLabel};CWnd* edits[]={&name,&pattern,&prefix};
    for(int i=0;i<3;++i){move(*labels[i],pad,y,width,Scale(20));y+=Scale(22);move(*edits[i],pad,y,width,Scale(28));y+=Scale(34);}
    move(omit,pad,y,width,Scale(30));y+=Scale(34);
    move(advancedButton,pad,y,width,Scale(28));y+=Scale(34);
    regex.ShowWindow(advanced?SW_SHOW:SW_HIDE);followingImages.ShowWindow(advanced?SW_SHOW:SW_HIDE);
    if(advanced){move(regex,pad,y,width,Scale(28));y+=Scale(32);move(followingImages,pad,y,width,Scale(38));y+=Scale(44);}
    move(hint,pad,y,width,Scale(52));y+=Scale(58);
    move(save,pad,y,max(90,width-Scale(112)),Scale(32));move(cancel,pad+width-Scale(104),y,Scale(104),Scale(32));y+=Scale(40);
    move(feedback,pad,y,width,Scale(44));
    contentHeight=y+Scale(56);
    scrolling.nMax=contentHeight-1;SetScrollInfo(SB_VERT,&scrolling,TRUE);
  }
  void RevealFocus(){HWND focused=::GetFocus();if(!focused || !::IsChild(m_hWnd,focused))return;
    CRect control,client;::GetWindowRect(focused,control);ScreenToClient(control);GetClientRect(client);
    int delta=control.top<Scale(12)?control.top-Scale(12):control.bottom>client.bottom-Scale(12)?control.bottom-client.bottom+Scale(12):0;
    if(delta){scrollOffset=max(0,min(scrollOffset+delta,max(0,contentHeight-client.Height())));Layout();}}
  void RefreshRoutes() {
    visibleRoutes.clear();
    routes.ResetContent();
    const std::vector<WorkspaceRoute> &all = frame->WorkspaceRoutes();
    auto sorted=all;std::stable_sort(sorted.begin(),sorted.end(),[&](const WorkspaceRoute& a,const WorkspaceRoute& b){const int pa=frame->WorkspaceTranscriptPreferences(a.connectionId,a.transcriptName).priority,pb=frame->WorkspaceTranscriptPreferences(b.connectionId,b.transcriptName).priority;if(pa!=pb)return pa>pb;const int name=a.transcriptName.Compare(b.transcriptName);return name?name<0:a.pattern<b.pattern;});
    for (size_t i = 0; i < sorted.size(); ++i)
      if (sorted[i].connectionId == identity) {
        const auto& route=sorted[i];visibleRoutes.push_back(route);
        std::wstring label =
            RouteWide((LPCTSTR)route.transcriptName) + L" | priority "+std::to_wstring(frame->WorkspaceTranscriptPreferences(identity,route.transcriptName).priority)+L" | " +
            (route.regex ? L"regex: " : L"literal: ") + route.pattern +
            (route.omitFromMain ? L" | hidden from Main" : L" | copied from Main")+(route.followingImages?L" | following images":L"");
        ::SendMessageW(routes.m_hWnd, LB_ADDSTRING, 0, (LPARAM)label.c_str());
      }
    editing = -1;
    removeRoute.EnableWindow(FALSE);
    optionsButton.EnableWindow(FALSE);
    save.SetWindowText("Create route");
    Destination();
  }

public:
  CWorkspaceRouteDialog(CMainFrame *owner, const CString &id)
      : frame(owner), identity(id), editing(-1), dpi(96) {}
  BOOL Open() {
    return CreateEx(WS_EX_DLGMODALFRAME,
                    AfxRegisterWndClass(0, LoadCursor(NULL, IDC_ARROW),
                                        (HBRUSH)(COLOR_BTNFACE + 1)),
                    "Local output channels",
                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                        WS_MINIMIZEBOX | WS_VSCROLL,
                    CRect(200, 100, 860, 800), frame, 0);
  }
  virtual void PostNcDestroy() { delete this; }
  virtual BOOL PreTranslateMessage(MSG *message) {
    if (message->message == WM_KEYDOWN) {
      if(message->wParam==VK_TAB && ::IsDialogMessage(m_hWnd,message)){RevealFocus();return TRUE;}
      if (message->wParam == VK_ESCAPE) {
        OnClose();
        return TRUE;
      }
      if (message->wParam == VK_RETURN) {
        wchar_t focusedClass[32] = {0};
        ::GetClassNameW(message->hwnd, focusedClass, 32);
        if (!_wcsicmp(focusedClass, L"Button")) {
          ::SendMessageW(message->hwnd, BM_CLICK, 0, 0);
          return TRUE;
        }
        OnSave();
        return TRUE;
      }
      if (message->wParam == VK_DELETE && message->hwnd == routes.m_hWnd) {
        OnDelete();
        return TRUE;
      }
    }
    return CWnd::PreTranslateMessage(message);
  }

protected:
  afx_msg int OnCreate(LPCREATESTRUCT cs) {
    if (CWnd::OnCreate(cs) == -1)
      return -1;
    SetPropW(GetSafeHwnd(), L"MUSHclientWorkspace", (HANDLE)1);
    SetPropW(GetSafeHwnd(), L"MUSHclientRouteManager", (HANDLE)1);
    auto getDpi = (UINT(WINAPI *)(HWND))GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (getDpi)
      dpi = getDpi(m_hWnd);
    HWND list = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                      WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY,
                                  0, 0, 1, 1, m_hWnd, (HMENU)7,
                                  AfxGetInstanceHandle(), NULL);
    if (!list || !routes.Attach(list))
      return -1;
    newRoute.Create("New", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    CRect(0, 0, 1, 1), this, 8);
    removeRoute.Create("Delete",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(0, 0, 1, 1), this, 9);
    channelExample.Create("Channel",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                          CRect(0, 0, 1, 1), this, 11);
    tellExample.Create("Tell",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(0, 0, 1, 1), this, 12);
    optionsButton.Create("Options...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,13);
    advancedButton.Create("Advanced routing",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,15);
    nameLabel.Create("Transcript name", WS_CHILD | WS_VISIBLE,
                     CRect(0, 0, 1, 1), this);
    patternLabel.Create("Incoming label or text to match",
                        WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    prefixLabel.Create(
        "Reply prefix (include a trailing space or = when needed)",
        WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    destination.Create("Original world", WS_CHILD | WS_VISIBLE,
                       CRect(0, 0, 1, 1), this);
    hint.Create("Filters received text locally; it does not join game channels. A blank reply prefix sends complete commands.",
                WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    feedback.Create("Choose a channel or tell example, or select a saved rule. Select a rule to enable Options and Delete.",
                    WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    CEdit *edits[] = {&name, &pattern, &prefix};
    int ids[] = {1, 2, 10};
    for (int i = 0; i < 3; ++i) {
      HWND input = ::CreateWindowExW(
          WS_EX_CLIENTEDGE, L"EDIT", i == 0 ? L"Tells" : L"",
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 1, 1,
          m_hWnd, (HMENU)(INT_PTR)ids[i], AfxGetInstanceHandle(), NULL);
      if (!input || !edits[i]->Attach(input))
        return -1;
      ::SendMessageW(input, EM_SETLIMITTEXT, i == 0 ? 64 : 1024, 0);
    }
    ::SendMessageW(
        pattern.m_hWnd, EM_SETCUEBANNER, 0,
        (LPARAM)L"Paste a distinctive label from the received output");
    ::SendMessageW(prefix.m_hWnd, EM_SETCUEBANNER, 0,
                   (LPARAM)L"Example: +pub  or  page Alice=");
    regex.Create("Use regular expression (literal text is the default)",
                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                 CRect(0, 0, 1, 1), this, 3);
    omit.Create("Show matches only here (hide them from Main)",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX |
                    BS_MULTILINE,
                CRect(0, 0, 1, 1), this, 4);
    followingImages.Create("Include up to 4 immediately following server images (within 2 seconds)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX|BS_MULTILINE,CRect(0,0,1,1),this,14);
    save.Create("Create route",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                CRect(0, 0, 1, 1), this, 5);
    cancel.Create("Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  CRect(0, 0, 1, 1), this, 6);
    Font();
    using workspace_style::Icon;
    workflow_ui::Button(m_hWnd,5,Icon::Save,true);
    workflow_ui::Button(m_hWnd,6,Icon::Close);
    workflow_ui::Button(m_hWnd,8,Icon::Add);
    workflow_ui::Button(m_hWnd,9,Icon::Remove);
    workflow_ui::Button(m_hWnd,11,Icon::Channels,false,L"Fill a channel example. Replace its label and reply prefix before saving.");
    workflow_ui::Button(m_hWnd,12,Icon::Channels,false,L"Fill a tell example. Choose the recipient and match text before saving.");
    workflow_ui::Button(m_hWnd,13,Icon::Settings);
    workflow_ui::Button(m_hWnd,15,Icon::ChevronDown);
    Layout();
    RefreshRoutes();
    SetTimer(1, 1000, NULL);
    return 0;
  }
  afx_msg void OnSize(UINT type, int width, int height) {
    CWnd::OnSize(type, width, height);
    Layout();
  }
  afx_msg void OnGetMinMaxInfo(MINMAXINFO *info) {
    info->ptMinTrackSize.x = Scale(400);
    info->ptMinTrackSize.y = Scale(420);
    CWnd::OnGetMinMaxInfo(info);
  }
  afx_msg LRESULT OnDpiChanged(WPARAM value, LPARAM bounds) {
    dpi = LOWORD(value);
    Font();
    if (bounds) {
      RECT *r = (RECT *)bounds;
      SetWindowPos(NULL, r->left, r->top, r->right - r->left,
                   r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    Layout();
    return 0;
  }
  afx_msg void OnAdvanced(){advanced=!advanced;advancedButton.SetWindowText(advanced?"Hide advanced routing":"Advanced routing");Layout();}
  afx_msg void OnVScroll(UINT code,UINT,CScrollBar*){SCROLLINFO info={sizeof(info),SIF_ALL};GetScrollInfo(SB_VERT,&info);int next=scrollOffset;
    if(code==SB_LINEUP)next-=Scale(28);else if(code==SB_LINEDOWN)next+=Scale(28);else if(code==SB_PAGEUP)next-=(int)info.nPage;else if(code==SB_PAGEDOWN)next+=(int)info.nPage;else if(code==SB_THUMBTRACK)next=info.nTrackPos;
    scrollOffset=max(0,min(next,max(0,contentHeight-(int)info.nPage)));Layout();}
  afx_msg BOOL OnMouseWheel(UINT,short delta,CPoint){scrollOffset=max(0,scrollOffset-MulDiv(delta,Scale(56),WHEEL_DELTA));Layout();return TRUE;}
  afx_msg void OnTimer(UINT_PTR id) {
    if (id == 1)
      Destination();
    CWnd::OnTimer(id);
  }
  afx_msg void OnSelect() {
    editing = routes.GetCurSel();
    if (editing < 0 || editing >= (int)visibleRoutes.size())
      return;
    const WorkspaceRoute &route = visibleRoutes[editing];
    ::SetWindowTextW(name.m_hWnd,
                     RouteWide((LPCTSTR)route.transcriptName).c_str());
    ::SetWindowTextW(pattern.m_hWnd, route.pattern.c_str());
    ::SetWindowTextW(
        prefix.m_hWnd,
        frame->WorkspaceInputPrefix(identity, route.transcriptName).c_str());
    regex.SetCheck(route.regex ? BST_CHECKED : BST_UNCHECKED);
    omit.SetCheck(route.omitFromMain ? BST_CHECKED : BST_UNCHECKED);
    followingImages.SetCheck(route.followingImages?BST_CHECKED:BST_UNCHECKED);
    if(route.regex || route.followingImages){advanced=true;advancedButton.SetWindowText("Hide advanced routing");Layout();}
    optionsButton.EnableWindow(TRUE);
    removeRoute.EnableWindow(TRUE);
    save.SetWindowText("Save changes");
    Feedback(L"Editing the selected rule. The prefix applies to this entire "
             L"named transcript.");
  }
  afx_msg void OnNew() {
    editing = -1;
    routes.SetCurSel(-1);
    ::SetWindowTextW(name.m_hWnd, L"Tells");
    ::SetWindowTextW(pattern.m_hWnd, L"");
    ::SetWindowTextW(prefix.m_hWnd, L"");
    regex.SetCheck(BST_UNCHECKED);
    omit.SetCheck(BST_UNCHECKED);
    followingImages.SetCheck(BST_UNCHECKED);optionsButton.EnableWindow(FALSE);
    removeRoute.EnableWindow(FALSE);
    save.SetWindowText("Create route");
    Feedback(L"New rule: choose received text to match and an optional "
             L"outgoing prefix.");
    name.SetFocus();
  }
  afx_msg void OnChannel() {
    OnNew();
    ::SetWindowTextW(name.m_hWnd, L"Public");
    ::SetWindowTextW(pattern.m_hWnd, L"[Public]");
    ::SetWindowTextW(prefix.m_hWnd, L"+pub ");
    Feedback(L"Example only: replace [Public] and +pub with this game's actual "
             L"label and command before saving.");
    pattern.SetFocus();
  }
  afx_msg void OnTell() {
    OnNew();
    ::SetWindowTextW(name.m_hWnd, L"Tells (Alice)");
    ::SetWindowTextW(pattern.m_hWnd, L"pages:");
    ::SetWindowTextW(prefix.m_hWnd, L"page Alice=");
    Feedback(L"Example only: outgoing replies go to Alice. Change the "
             L"recipient and match text for your game; no recipient is "
             L"inferred from incoming tells.");
    prefix.SetFocus();
  }
  afx_msg void OnDelete() {
    if (editing < 0 || editing >= (int)visibleRoutes.size())
      return;
    if (AfxMessageBox("Delete this local routing rule? The transcript and its "
                      "text remain open.",
                      MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
      return;
    if (!frame->RemoveWorkspaceRoute(visibleRoutes[editing])) {
      Feedback(L"The route could not be removed; it remains selected.");
      return;
    }
    RefreshRoutes();
    OnNew();
    Feedback(L"Routing rule deleted. Existing transcript text is preserved.");
  }
  afx_msg void OnSave() {
    try {
      if (WorkspaceEndpointForWindow((LPCTSTR)identity).empty()) {
        Feedback(L"The original world has closed. Open its route manager again "
                 L"after reopening that world.");
        return;
      }
      auto nameText = Read(name.m_hWnd, 64),
           patternText = Read(pattern.m_hWnd, 1024),
           prefixText = Read(prefix.m_hWnd, 1024);
      CString transcriptName;
      for (wchar_t c : nameText) {
        if (c < 32 || c > 126) {
          Feedback(
              L"Transcript names use printable English-keyboard characters; "
              L"received text and outgoing messages support Unicode.");
          name.SetFocus();
          return;
        }
        transcriptName += (char)c;
      }
      transcriptName.TrimLeft();
      transcriptName.TrimRight();
      std::string prefixUtf8, patternUtf8;
      if (!RouteUtf8(prefixText, prefixUtf8) ||
          !RouteUtf8(patternText, patternUtf8)) {
        Feedback(
            L"The match text or prefix contains an invalid Unicode character.");
        return;
      }
      auto checked = workspace_input::PrepareHumanInput(
          (LPCTSTR)identity, (LPCTSTR)identity, true, prefixUtf8, "x");
      if (!checked.ok) {
        Feedback(RouteWide(checked.error.c_str()));
        prefix.SetFocus();
        return;
      }
      for (wchar_t c : patternText)
        if (c < 32 || c == 127) {
          Feedback(L"Match one completed incoming line; remove control "
                   L"characters from the match text.");
          pattern.SetFocus();
          return;
        }
      WorkspaceRoute route;
      route.connectionId = identity;
      route.transcriptName = transcriptName;
      route.pattern = patternText;
      route.regex = regex.GetCheck() == BST_CHECKED;
      route.omitFromMain = omit.GetCheck() == BST_CHECKED;
      route.followingImages=followingImages.GetCheck()==BST_CHECKED;
      CString error;
      if (!route.Compile(error)) {
        Feedback(RouteWide((LPCTSTR)error));
        pattern.SetFocus();
        return;
      }
      const WorkspaceRoute* original=editing>=0 && editing<(int)visibleRoutes.size()?&visibleRoutes[editing]:NULL;
      bool saved=frame->SaveWorkspaceRoute(original,route,prefixText,error);
      if (!saved) {
        Feedback(RouteWide((LPCTSTR)error));
        return;
      }
      RefreshRoutes();
      for (size_t i = 0; i < visibleRoutes.size(); ++i)
        if (visibleRoutes[i].transcriptName == route.transcriptName &&
            visibleRoutes[i].pattern == route.pattern &&
            visibleRoutes[i].regex == route.regex) {
          routes.SetCurSel((int)i);
          OnSelect();
          break;
        }
      Feedback(L"Rule and outgoing prefix saved locally. Incoming matches use "
               L"the selected copy/hide behavior.");
    } catch (const std::exception &error) {
      Feedback(RouteWide(error.what()));
    }
  }
  afx_msg void OnDestroy() {
    KillTimer(1);
    RemovePropW(GetSafeHwnd(), L"MUSHclientWorkspace");
    RemovePropW(GetSafeHwnd(), L"MUSHclientRouteManager");
    CWnd::OnDestroy();
  }
  afx_msg void OnClose() { DestroyWindow(); }
  afx_msg void OnOptions(){if(editing<0 || editing>=(int)visibleRoutes.size())return;SpawnOptions options;options.frame=frame;options.identity=identity;options.name=visibleRoutes[editing].transcriptName;options.preferences=frame->WorkspaceTranscriptPreferences(identity,options.name);const HWND owner=m_hWnd;ShowSpawnOptions(owner,options);if(::IsWindow(owner) && CWnd::FromHandlePermanent(owner)==this)RefreshRoutes();}
  DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CWorkspaceRouteDialog, CWnd)
ON_WM_CREATE()
ON_WM_CLOSE()
ON_WM_DESTROY()
ON_WM_SIZE()
ON_WM_GETMINMAXINFO()
ON_WM_TIMER()
ON_WM_VSCROLL()
ON_WM_MOUSEWHEEL()
ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
ON_BN_CLICKED(5, OnSave)
ON_BN_CLICKED(6, OnClose)
ON_BN_CLICKED(8, OnNew)
ON_BN_CLICKED(9, OnDelete)
ON_BN_CLICKED(11, OnChannel)
ON_BN_CLICKED(12, OnTell)
ON_BN_CLICKED(13, OnOptions)
ON_BN_CLICKED(15, OnAdvanced)
ON_LBN_SELCHANGE(7, OnSelect)
END_MESSAGE_MAP()
void OpenWorkspaceRouteManager(CMainFrame *frame, const CString &identity) {
  CWorkspaceRouteDialog *dialog = new CWorkspaceRouteDialog(frame, identity);
  if (!dialog->Open())
    return;
  dialog->ShowWindow(SW_SHOW);
  workspace_style::FitToWorkArea(dialog->GetSafeHwnd(),400,420);
  dialog->SetForegroundWindow();
}
