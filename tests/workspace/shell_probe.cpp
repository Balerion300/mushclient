#include "stdafx.h"
#include "ModernFixture.h"
#include "workspace/WorkspaceShell.h"
#include "WorkspaceStyle.h"
#include <fstream>

namespace ws=workspace_shell;
bool Check(std::ofstream& out,const char* label,bool value){out<<(value?"PASS ":"FAIL ")<<label<<"\n";return value;}
bool Screenshot(HWND window,const wchar_t* path){RECT rect;GetWindowRect(window,&rect);const int width=rect.right-rect.left,height=rect.bottom-rect.top;HDC screen=GetDC(window),memory=CreateCompatibleDC(screen);BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* pixels=NULL;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,NULL,0);HGDIOBJ old=SelectObject(memory,bitmap);
 const BOOL rendered=PrintWindow(window,memory,0);BITMAPFILEHEADER file={0};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+width*height*4;HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD written=0;bool ok=rendered && output!=INVALID_HANDLE_VALUE;if(output!=INVALID_HANDLE_VALUE){ok=WriteFile(output,&file,sizeof(file),&written,NULL) && written==sizeof(file) && ok;ok=WriteFile(output,&info.bmiHeader,sizeof(info.bmiHeader),&written,NULL) && written==sizeof(info.bmiHeader) && ok;ok=WriteFile(output,pixels,width*height*4,&written,NULL) && written==width*height*4 && ok;CloseHandle(output);}SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,screen);return ok;}
bool Inside(HWND child,HWND parent){RECT a,b;GetWindowRect(child,&a);GetClientRect(parent,&b);MapWindowPoints(parent,NULL,(POINT*)&b,2);return a.left>=b.left && a.top>=b.top && a.right<=b.right && a.bottom<=b.bottom;}
bool Separate(HWND bar){for(int i=0;i<7;++i)for(int j=i+1;j<7;++j){RECT a,b,overlap;GetWindowRect(GetDlgItem(bar,0x7800+i),&a);GetWindowRect(GetDlgItem(bar,0x7800+j),&b);if(IntersectRect(&overlap,&a,&b))return false;}return true;}
class ShellApp:public CWinApp {
 int result_=1;
 BOOL InitInstance(){
#ifdef _DEBUG
  _CrtSetReportHook([](int type,char* message,int*)->int{if(type==_CRT_ASSERT){FILE* file=NULL;fopen_s(&file,"shell-debug-assert.txt","w");if(file){fputs(message,file);fclose(file);}ExitProcess(91);}return FALSE;});
#endif
  AfxOleInit();AfxInitRichEdit2();std::ofstream out("shell-results.txt");out.setf(std::ios::unitbuf);bool ok=true;
  auto* frame=new WorkspaceTestFrame;ok=frame->Create(NULL,"Native workspace shell - fictional fixture",WS_OVERLAPPEDWINDOW,CRect(80,80,1360,960))!=FALSE;m_pMainWnd=frame;frame->EnableWorkspaceDocking();
  ws::Action last=ws::Tools;std::string world,name;int actions=0;CWorkspaceShell shell;ok=shell.Create(*frame,[&](ws::Action action,const std::string& w,const std::string& n){last=action;world=w;name=n;++actions;})&&ok;
  ws::Snapshot snapshot;snapshot.world="fictional-harbor:runtime:alpha";
  ws::Row harbor;harbor.world=snapshot.world;harbor.label=L"Silver Harbor";harbor.connected=true;harbor.logging=true;harbor.unread=12;snapshot.rows.push_back(harbor);
  ws::Row chat;chat.world=harbor.world;chat.name="Chat";chat.label=L"Chat";chat.channel=true;chat.unread=12;chat.mentions=2;snapshot.rows.push_back(chat);
  chat.name="Tells";chat.label=L"Tells";chat.unread=3;chat.mentions=0;snapshot.rows.push_back(chat);
  ws::Row ash;ash.world="fictional-ash:runtime:beta";ash.label=L"Ash Hill";snapshot.rows.push_back(ash);ws::Row watch;watch.world="fictional-watch:runtime:gamma";watch.label=L"Watchroom";watch.connecting=true;snapshot.rows.push_back(watch);
  shell.Update(snapshot);frame->ShowWindow(SW_SHOWNOACTIVATE);frame->RecalcLayout();frame->UpdateWindow();
  ws::Target targetA;targetA.window=(HWND)1;targetA.document=11;ws::Target targetB;targetB.window=(HWND)2;targetB.document=22;ws::Target active=targetA;bool alive=true;int wrongActions=0;
  auto resolve=[&](const std::string& id){return id==harbor.world && alive?targetA:ws::Target();};
  if(ws::ActivateOriginal(harbor.world,resolve,[&](HWND){active=targetB;},[&](){return active;}))++wrongActions;
  ok=Check(out,"reentrant activation redirect cannot dispatch to another world",wrongActions==0)&&ok;
  if(ws::ActivateOriginal(harbor.world,resolve,[&](HWND){alive=false;active=targetB;},[&](){return active;}))++wrongActions;
  ok=Check(out,"reentrant activation closure cannot use stale target or dispatch to another world",wrongActions==0)&&ok;
  HWND bar=frame->GetDlgItem(0xe7a0)->m_hWnd,nav=frame->GetDlgItem(0xe7a1)->m_hWnd,tree=::GetDlgItem(nav,0x7820);auto* nativeTree=(CTreeCtrl*)CWnd::FromHandle(tree);
  MEASUREITEMSTRUCT gameMeasure={};gameMeasure.CtlType=ODT_LISTBOX;gameMeasure.CtlID=0x7822;
  ::SendMessage(nav,WM_MEASUREITEM,0x7822,(LPARAM)&gameMeasure);
  ok=Check(out,"owner-drawn game rail supplies native row measurement",gameMeasure.itemHeight==(UINT)workspace_style::Scale(nav,52))&&ok;
  ok=Check(out,"native labeled daily actions and navigator register with frame",bar && nav && tree && ::IsWindowVisible(bar) && shell.NavigatorVisible())&&ok;
  RECT client;::GetClientRect(frame->m_hWndMDIClient,&client);HWND transcript=::CreateWindowExW(0,L"RichEdit20W",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY,0,0,client.right,client.bottom-70,frame->m_hWndMDIClient,NULL,AfxGetInstanceHandle(),NULL);
  HFONT original=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH,L"Consolas");SendMessageW(transcript,WM_SETFONT,(WPARAM)original,FALSE);SendMessageW(transcript,EM_SETBKGNDCOLOR,0,RGB(25,25,28));CHARFORMAT2W format;memset(&format,0,sizeof(format));format.cbSize=sizeof(format);format.dwMask=CFM_COLOR;format.crTextColor=RGB(226,226,230);SendMessageW(transcript,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&format);
  std::wstring sample=L"Silver Harbor\r\n\r\nThe lanterns along the harbor cast a warm light on the water.\r\nA bell sounds from the watchtower.\r\n\r\n[Chat] Mira says, \"Meet at the eastern gate when you are ready.\"\r\n[Tell] Rowan: I will bring the maps.\r\n\r\nThis is a fictional native fixture. No world connection or game command is used.\r\n\r\n";for(int i=0;i<4;++i)sample+=L"A ship passes beyond the harbor wall. The evening watch changes.\r\n";SetWindowTextW(transcript,sample.c_str());SendMessageW(transcript,EM_SETSEL,34,42);
  HWND destination=CreateWindowExW(0,L"STATIC",L"SILVER HARBOR  /  MAIN TRANSCRIPT",WS_CHILD|WS_VISIBLE,12,client.bottom-63,client.right-24,20,frame->m_hWndMDIClient,NULL,AfxGetInstanceHandle(),NULL);workspace_style::StyleControl(destination);
  HWND composer=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,12,client.bottom-40,client.right-24,30,frame->m_hWndMDIClient,NULL,AfxGetInstanceHandle(),NULL);SendMessageW(composer,WM_SETFONT,(WPARAM)original,FALSE);
  const LRESULT transcriptFont=SendMessageW(transcript,WM_GETFONT,0,0),composerFont=SendMessageW(composer,WM_GETFONT,0,0),selection=SendMessageW(transcript,EM_GETSEL,0,0);
  ok=Check(out,"central transcript remains wider than navigator",client.right>500)&&ok;
  ::SendMessageW(::GetDlgItem(bar,0x7801),BM_CLICK,0,0);ok=Check(out,"Connect/Disconnect carries exact original runtime world identity",last==ws::Disconnect && world==harbor.world && actions==1)&&ok;
  const HTREEITEM root=nativeTree->GetRootItem(),first=nativeTree->GetChildItem(root);nativeTree->SelectItem(first);ok=Check(out,"channel selection retains original world and channel identity",last==ws::Select && world==harbor.world && name=="Chat")&&ok;
  ::SetFocus(composer);const HWND focus=::GetFocus();const int before=actions;for(int i=0;i<500;++i){snapshot.rows[1].unread=i;shell.Update(snapshot);}
  ok=Check(out,"state-only refreshes retain native tree items and keyboard focus without dispatch",nativeTree->GetRootItem()==root && nativeTree->GetChildItem(root)==first && ::GetFocus()==focus && actions==before)&&ok;
  snapshot.world=ash.world;snapshot.name.clear();shell.Update(snapshot);ok=Check(out,"external world change synchronizes native selection without dispatch or focus loss",nativeTree->GetSelectedItem()==nativeTree->GetNextSiblingItem(root) && actions==before && ::GetFocus()==focus)&&ok;snapshot.world=harbor.world;shell.Update(snapshot);
  workspace_style::SetTheme(workspace_style::Theme::Light);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
  ok=Check(out,"light appearance updates explicit navigator colors",nativeTree->GetBkColor()==workspace_style::Colors(nav).surface && nativeTree->GetTextColor()==workspace_style::Colors(nav).text)&&ok;
  ok=Check(out,"light composition screenshot",Screenshot(frame->m_hWnd,L"shell-light.bmp"))&&ok;
  workspace_style::SetTheme(workspace_style::Theme::Dark);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
  ok=Check(out,"dark appearance updates explicit navigator colors",nativeTree->GetBkColor()==workspace_style::Colors(nav).surface && nativeTree->GetTextColor()==workspace_style::Colors(nav).text)&&ok;
  ok=Check(out,"theme does not overwrite original transcript/composer font or selection",SendMessageW(transcript,WM_GETFONT,0,0)==transcriptFont && SendMessageW(composer,WM_GETFONT,0,0)==composerFont && SendMessageW(transcript,EM_GETSEL,0,0)==selection)&&ok;
  ok=Check(out,"dark composition screenshot",Screenshot(frame->m_hWnd,L"shell-dark.bmp"))&&ok;
  snapshot.rows.clear();snapshot.world.clear();shell.Update(snapshot);ok=Check(out,"world-specific actions disable when the original world is closed",!::IsWindowEnabled(::GetDlgItem(bar,0x7801)) && !::IsWindowEnabled(::GetDlgItem(bar,0x7802)) && !::IsWindowEnabled(::GetDlgItem(bar,0x7803)) && !::IsWindowEnabled(::GetDlgItem(bar,0x7804)))&&ok;
  // Exercise the same inherited CDockablePane idle update used by the client.
  for(int i=0;i<3;++i)::SendMessageW(nav,WM_IDLEUPDATECMDUI,TRUE,0);
  const HWND openButton=::GetDlgItem(nav,0x7821);const int beforeOpen=actions;
  ok=Check(out,"Open a world remains enabled after native idle command updates with no worlds",::IsWindowEnabled(openButton)!=FALSE)&&ok;
  ::SendMessageW(openButton,BM_CLICK,0,0);ok=Check(out,"idle-updated Open a world dispatches only the original open-file action",actions==beforeOpen+1 && last==ws::OpenWorld && world.empty() && name.empty())&&ok;
  const HWND createButton=::GetDlgItem(nav,0x7825);const int beforeCreate=actions;
  ok=Check(out,"first-run create and open are distinct enabled actions",createButton && ::IsWindowEnabled(createButton) && Inside(createButton,nav) && Inside(openButton,nav))&&ok;
  ::SendMessageW(createButton,BM_CLICK,0,0);ok=Check(out,"New connection dispatches creation rather than an existing-file picker",actions==beforeCreate+1 && last==ws::CreateWorld && world.empty() && name.empty())&&ok;
  const LONG widthWithNav=client.right;shell.ShowNavigator(false);::GetClientRect(frame->m_hWndMDIClient,&client);ok=Check(out,"collapsing navigator returns space to central transcript",client.right>widthWithNav && !shell.NavigatorVisible())&&ok;
  frame->SetWindowPos(NULL,0,0,600,700,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);frame->RecalcLayout();bool inside=true;for(int i=0;i<6;++i)inside=Inside(::GetDlgItem(bar,0x7800+i),bar)&&inside;
  ok=Check(out,"narrow daily action bar wraps every labeled action inside its reserved height",inside && Inside(::GetDlgItem(bar,0x7806),bar))&&ok;
  ok=Check(out,"wrapped status and every action occupy separate rectangles",Separate(bar))&&ok;ok=Check(out,"narrow composition screenshot",Screenshot(frame->m_hWnd,L"shell-narrow.bmp"))&&ok;
  frame->SetWindowPos(NULL,0,0,1280,880,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);shell.ShowNavigator(true);
  snapshot.rows={harbor,chat,ash,watch};snapshot.rows[0].game="game-harbor";snapshot.rows[0].gameLabel=L"Silver Harbor";snapshot.rows[0].characterLabel=L"Mira";
  snapshot.rows[1].world=harbor.world;snapshot.rows[1].name="Chat";snapshot.rows[1].label=L"Chat";
  snapshot.rows[2].game="game-harbor";snapshot.rows[2].gameLabel=L"Silver Harbor";snapshot.rows[2].characterLabel=L"Rowan";
  ws::Row secondChat=snapshot.rows[1];secondChat.world=ash.world;snapshot.rows.insert(snapshot.rows.begin()+3,secondChat);
  snapshot.rows[4].game="game-watch";snapshot.rows[4].gameLabel=L"Ash Hill";snapshot.rows[4].characterLabel=L"Quinn";
  snapshot.world=harbor.world;snapshot.name.clear();shell.Update(snapshot);frame->RecalcLayout();
  const HTREEITEM gameRoot=nativeTree->GetRootItem(),mira=nativeTree->GetChildItem(gameRoot),rowan=nativeTree->GetNextSiblingItem(mira);
  const HTREEITEM miraChat=nativeTree->GetChildItem(mira),rowanChat=nativeTree->GetChildItem(rowan);
  ok=Check(out,"one game groups two distinct own-character connections",gameRoot && mira && rowan && nativeTree->GetParentItem(rowan)==gameRoot && nativeTree->GetNextSiblingItem(gameRoot))&&ok;
  ok=Check(out,"same-name spawns remain distinct children of their own character",miraChat && rowanChat && miraChat!=rowanChat && nativeTree->GetParentItem(miraChat)==mira && nativeTree->GetParentItem(rowanChat)==rowan)&&ok;
  const int beforeGroup=actions;nativeTree->SelectItem(gameRoot);ok=Check(out,"selecting cosmetic game group never dispatches a character command",actions==beforeGroup)&&ok;
  nativeTree->SelectItem(miraChat);ok=Check(out,"first character Chat retains its exact runtime target",last==ws::Select && world==harbor.world && name=="Chat")&&ok;
  nativeTree->SelectItem(rowanChat);ok=Check(out,"second same-name Chat dispatches only its own runtime target",last==ws::Select && world==ash.world && name=="Chat")&&ok;
  const HWND rail=::GetDlgItem(nav,0x7822);::SendMessage(rail,LB_SETCURSEL,1,0);const int beforeRail=actions;
  ::SendMessage(nav,WM_COMMAND,MAKEWPARAM(0x7822,LBN_SELCHANGE),(LPARAM)rail);
  ok=Check(out,"game rail dispatches exact first character in newly selected game",actions==beforeRail+1 && last==ws::Select && world==watch.world && name.empty())&&ok;
  snapshot.rows[2].characterLabel=L"Rowan renamed";shell.Update(snapshot);ok=Check(out,"character rename preserves native items and runtime identity",nativeTree->GetChildItem(gameRoot)==mira && nativeTree->GetNextSiblingItem(mira)==rowan)&&ok;
  nativeTree->SelectItem(rowan);ok=Check(out,"display rename cannot change the original connection destination",world==ash.world && name.empty())&&ok;
  nativeTree->Expand(gameRoot,TVE_COLLAPSE);for(int i=0;i<100;++i){snapshot.rows[1].unread=i;shell.Update(snapshot);}ok=Check(out,"passive spawn updates keep cosmetic game group collapsed",!(nativeTree->GetItemState(gameRoot,TVIS_EXPANDED)&TVIS_EXPANDED))&&ok;
  nativeTree->Expand(gameRoot,TVE_EXPAND);workspace_style::SetTheme(workspace_style::Theme::Light);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);ok=Check(out,"character hierarchy light screenshot",Screenshot(frame->m_hWnd,L"shell-characters-light.bmp"))&&ok;
  workspace_style::SetTheme(workspace_style::Theme::Dark);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);ok=Check(out,"character hierarchy dark screenshot",Screenshot(frame->m_hWnd,L"shell-characters-dark.bmp"))&&ok;
  out<<(ok?"PASS":"FAIL")<<" native shell fixture; native API simulation, no physical mixed-DPI/screen-reader acceptance\n";frame->DestroyWindow();DeleteObject(original);result_=ok?0:1;return FALSE;
 }
 int ExitInstance(){return result_;}
};ShellApp app;
