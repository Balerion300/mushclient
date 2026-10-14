#include "stdafx.h"
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include "WorkspaceResources.h"
#include <afxribbonres.h>
#include <fstream>
#include <richole.h>
void SaveFixtureBitmap(const char* path,HDC window,int width,int height,unsigned long& colored)
{
 BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* data=NULL;
 HBITMAP bitmap=CreateDIBSection(window,&info,DIB_RGB_COLORS,&data,NULL,0);HDC target=CreateCompatibleDC(window);HGDIOBJ old=SelectObject(target,bitmap);
 BitBlt(target,0,0,width,height,window,0,0,SRCCOPY);GdiFlush();colored=0;
 for(int i=0;i<width*height;++i){DWORD pixel=((DWORD*)data)[i];int r=(pixel>>16)&255,g=(pixel>>8)&255,b=pixel&255;if(max(r,max(g,b))-min(r,min(g,b))>35)++colored;}
 BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+width*height*4;
 std::ofstream out(path,std::ios::binary);out.write((char*)&header,sizeof(header));out.write((char*)&info.bmiHeader,sizeof(info.bmiHeader));out.write((char*)data,width*height*4);
 SelectObject(target,old);DeleteDC(target);DeleteObject(bitmap);
}
class PixelPane:public CWorkspacePane {public:size_t Retained(){return m_transcripts.empty()?0:m_transcripts[0]->records.size();}};
class InspectNativeTabs:public CMFCTabCtrl {public: static bool Ready(CMFCTabCtrl& tabs){return tabs.*&InspectNativeTabs::m_bReadyToDetach;} };
class PixelApp:public CWinApp
{
public:int result=1;
 BOOL InitInstance()
 {
  AfxOleInit();AfxInitRichEdit2();std::ofstream log("WinRel/workspace-image-pixels.txt");log.setf(std::ios::unitbuf);
  const std::shared_ptr<CInlineImage> image=CInlineImage::Request("https://raw.githubusercontent.com/Balerion300/mushclient/codex/inline-static-images/tests/fixture-scene.png");
  for(int i=0;i<1000 && image->GetState()==CInlineImage::Loading;++i)::Sleep(20);
  bool ok=image->GetState()==CInlineImage::Ready;log<<"real decoded image state="<<image->GetState()<<" size="<<image->GetWidth()<<"x"<<image->GetHeight()<<"\n";
  WorkspaceTestFrame* frame=new WorkspaceTestFrame;ok=ok && frame->Create(NULL,"Native image pixel fixture",WS_OVERLAPPEDWINDOW,CRect(40,40,1050,800));frame->EnableWorkspaceDocking();PixelPane pane;ok=ok && pane.Create(frame);pane.EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(&pane);ok=ok && pane.OpenTranscript("pixels","Image");
  WorkspaceRecord record;record.connectionId="pixels";record.sequence=1;record.image=image;record.imageAction="https://raw.githubusercontent.com/Balerion300/mushclient/codex/inline-static-images/tests/fixture-scene.png";WorkspaceRun link;link.text=L"[https://raw.githubusercontent.com/Balerion300/mushclient/codex/inline-static-images/tests/fixture-scene.png]";link.action=record.imageAction;link.actionType=0x0800;record.runs.push_back(link);// Reproduce the previous inline layout without changing production code:
  // insert the exact long generated URL before the image object on one line.
  HWND rich=pane.GetDlgItem(100)->m_hWnd;SendMessageW(rich,EM_SETREADONLY,FALSE,0);SendMessageW(rich,EM_REPLACESEL,FALSE,(LPARAM)link.text.c_str());SendMessageW(rich,EM_SETREADONLY,TRUE,0);
  ok=ok && pane.AppendRecord("Image",record);frame->ShowWindow(SW_SHOWNOACTIVATE);pane.ShowPane(TRUE,FALSE,FALSE);frame->RecalcLayout();
  MSG message;for(int i=0;i<20;++i){while(PeekMessage(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessage(&message);}Sleep(20);}
  CWnd* text=pane.GetDlgItem(100);text->UpdateWindow();CRect rect;text->GetClientRect(rect);HDC screen=GetDC(text->m_hWnd);unsigned long colored=0;
  SaveFixtureBitmap("WinRel/workspace-image-before.bmp",screen,rect.Width(),rect.Height(),colored);ReleaseDC(text->m_hWnd,screen);const unsigned long before=colored;
  log<<"long generated URL inline negative colored pixels="<<before<<"\n";
  pane.ClearTranscript("pixels","Image");record.sequence=2;ok=ok && pane.AppendRecord("Image",record);text->Invalidate();text->UpdateWindow();
  screen=GetDC(text->m_hWnd);SaveFixtureBitmap("WinRel/workspace-image-pixels.bmp",screen,rect.Width(),rect.Height(),colored);ReleaseDC(text->m_hWnd,screen);log<<"fixed native RichEdit colored pixels="<<colored<<" client="<<rect.Width()<<"x"<<rect.Height()<<" retained="<<pane.Retained()<<"\n";
  const HINSTANCE language=LoadLibraryExW(L".distribution-source/extracted/locale/en.dll",NULL,LOAD_LIBRARY_AS_DATAFILE);if(language){AfxSetResourceHandle(language);CString caption;{WorkspaceModernResources resources;ok=ok && caption.LoadString(IDS_AFXBARRES_CLOSEBAR);}ok=ok && AfxGetResourceHandle()==language;
  CWorkspacePane companion;ok=ok && companion.Create(frame,0xe904) && companion.OpenTranscript("other","Second");companion.EnableDocking(CBRS_ALIGN_ANY);CDockablePane* holder=NULL;companion.AttachToTabWnd(&pane,DM_SHOW,TRUE,&holder);CTabbedPane* group=DYNAMIC_DOWNCAST(CTabbedPane,holder);
  ok=ok && group && group->GetTabWnd()->GetTabsNum()==2 && AfxGetResourceHandle()==language;
  bool callbackLocale=false;pane.SetInputHandler([&](const CString&,const std::wstring&){callbackLocale=AfxGetResourceHandle()==language;return true;});SetWindowTextW(pane.GetDlgItem(2)->m_hWnd,L"native human fixture");{WorkspaceModernResources scope;pane.SendMessage(WM_COMMAND,MAKEWPARAM(3,BN_CLICKED),(LPARAM)pane.GetDlgItem(3)->m_hWnd);}ok=ok && callbackLocale && AfxGetResourceHandle()==language;
  CMFCTabCtrl* tabs=group->GetTabWnd();CRect hit;tabs->GetTabRect(tabs->GetActiveTab(),hit);const CPoint point=hit.CenterPoint();tabs->SendMessage(WM_MOUSEMOVE,0,MAKELPARAM(point.x,point.y));tabs->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));log<<"native tab mouse-down ready-to-detach="<<InspectNativeTabs::Ready(*tabs)<<" detachable="<<tabs->IsTabDetachable(tabs->GetActiveTab())<<" canFloat="<<companion.CanFloat()<<" mode="<<companion.GetDockingMode()<<" (message-state only, no physical drag)\n";tabs->SendMessage(WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
  log<<"static MFC native resources and original locale restore "<<(ok?"PASS":"FAIL")<<"\n";AfxSetResourceHandle(GetModuleHandle(NULL));FreeLibrary(language);}
  CDC source;source.CreateCompatibleDC(NULL);CBitmap bmp;CClientDC device(frame);bmp.CreateCompatibleBitmap(&device,320,160);CBitmap* old=source.SelectObject(&bmp);source.FillSolidRect(0,0,320,160,RGB(25,25,28));image->Draw(&source,CRect(0,0,320,160));GdiFlush();unsigned long drawn=0;SaveFixtureBitmap("WinRel/workspace-image-direct.bmp",source.m_hDC,320,160,drawn);source.SelectObject(old);log<<"same decoded image direct Draw colored pixels="<<drawn<<"\n";
  IRichEditOle* ole=NULL;SendMessageW(text->m_hWnd,EM_GETOLEINTERFACE,0,(LPARAM)&ole);int count=ole?ole->GetObjectCount():0;if(ole)ole->Release();log<<"OLE objects="<<count<<"\n";
  ok=ok && count==1 && before<500 && colored>500 && drawn>500 && pane.Retained()==1 && record.runs[0].text==link.text;log<<(ok?"PASS":"FAIL")<<" decoded native image visible pixels\n";frame->DestroyWindow();result=ok?0:1;return FALSE;
 }
 int ExitInstance(){return result;}
};PixelApp app;
