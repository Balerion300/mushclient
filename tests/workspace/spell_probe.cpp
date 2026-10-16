// Hidden native controls only. No desktop focus, input injection or real worlds.
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include "workspace/WorkspaceSpell.h"

namespace ws=workspace_spell;
static std::ofstream out("spell-probe-results.txt");
static bool good=true;
static int polishedDialogs=0;
static void Check(const char* label,bool value){out<<(value?"PASS ":"FAIL ")<<label<<"\n";out.flush();good=value&&good;}
static std::wstring Text(HWND window){int n=GetWindowTextLengthW(window);std::vector<wchar_t> text(n+1);GetWindowTextW(window,text.data(),n+1);return text.data();}
static void Pump(){MSG message;while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
static bool Wait(const std::function<bool()>& condition,DWORD timeout=12000){DWORD start=GetTickCount();do{Pump();if(condition())return true;Sleep(10);}while(GetTickCount()-start<timeout);return false;}
static bool Word(HWND window,const std::wstring& word){for(const auto& error:ws::Inspect(window).errors)if(error.word==word)return true;return false;}
static bool Settled(HWND window,const std::wstring& text){const auto snap=ws::Inspect(window);return !snap.pending && snap.text==text;}
static void Set(HWND window,const std::wstring& text){SetWindowTextW(window,text.c_str());ws::Refresh(window);}

int main(int argc,char** argv){
 const HWND originalFocus=GetFocus();
 INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
 wchar_t folder[MAX_PATH];GetCurrentDirectoryW(MAX_PATH,folder);
 ws::Context context;context.root=folder;context.gameKey=L"fictional-alpha";context.gameLabel=L"Fictional Alpha";
 context.polishDialog=[](HWND window){if(GetDlgItem(window,5)&&FindWindowExW(window,NULL,L"COMBOBOX",NULL))++polishedDialogs;};
 HWND parent=CreateWindowExW(0,L"STATIC",L"Hidden spell probe",WS_POPUP,0,0,800,400,NULL,NULL,GetModuleHandle(NULL),NULL);
 HWND editor=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL,0,0,600,240,parent,(HMENU)10,GetModuleHandle(NULL),NULL);
 Check("hidden Unicode editor creates without a visible top-level window",parent && editor && !IsWindowVisible(parent));
 Check("spell controller attaches to original native Unicode editor",ws::Attach(editor,[&](){return context;}));
 const std::wstring unicode=L"Dragon \x03bb \xd83d\xdc09 qzxvvaal.";
 Set(editor,unicode);
 Check("native worker settles without requiring focus or recipient input",Wait([&](){return Settled(editor,unicode);}));
 Check("native Windows spelling engine is available",ws::Inspect(editor).engineAvailable);
 Check("fictional misspelling is reported",Word(editor,L"qzxvvaal"));
 SendMessageW(editor,EM_SETSEL,4,4);const auto navigationRevision=ws::Inspect(editor).revision;
 SendMessageW(editor,WM_KEYDOWN,VK_LEFT,0);SendMessageW(editor,WM_KEYUP,VK_LEFT,0);
 DWORD navigationStart=0,navigationEnd=0;SendMessageW(editor,EM_GETSEL,(WPARAM)&navigationStart,(LPARAM)&navigationEnd);
 Check("caret navigation retains spelling results without changing text or revision",navigationStart==3 && navigationEnd==3 && Text(editor)==unicode && ws::Inspect(editor).revision==navigationRevision && Word(editor,L"qzxvvaal"));
 SendMessageW(editor,WM_KEYDOWN,VK_SHIFT,0);SendMessageW(editor,WM_KEYUP,VK_SHIFT,0);
 Check("text-neutral modifier keys retain spelling suggestions",Word(editor,L"qzxvvaal"));
 SendMessageW(editor,EM_SETSEL,2,7);
 Check("changing the selection retains spelling results for unchanged text",Text(editor)==unicode && Word(editor,L"qzxvvaal") && ws::Inspect(editor).revision==navigationRevision);
 bool exact=false;for(const auto& error:ws::Inspect(editor).errors)if(error.word==L"qzxvvaal")exact=error.first==unicode.find(L"qzxvvaal") && error.length==8;
 Check("supplementary Unicode text retains UTF-16 error indices",exact && Text(editor)==unicode);
 HWND alternate=CreateWindowExW(0,L"STATIC",L"Hidden alternate composer",WS_POPUP,0,0,800,400,NULL,NULL,GetModuleHandle(NULL),NULL);
 SetParent(editor,alternate);ws::Refresh(editor);
 Check("borrowing an editor into another composer preserves text and spell state",GetParent(editor)==alternate && Text(editor)==unicode && Word(editor,L"qzxvvaal"));
 SetParent(editor,parent);DestroyWindow(alternate);Check("returning a borrowed editor leaves its original window and spelling alive",IsWindow(editor) && Text(editor)==unicode && Word(editor,L"qzxvvaal"));
 HDC memory=CreateCompatibleDC(NULL);BITMAPINFO bitmapInfo={};bitmapInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bitmapInfo.bmiHeader.biWidth=600;bitmapInfo.bmiHeader.biHeight=-240;bitmapInfo.bmiHeader.biPlanes=1;bitmapInfo.bmiHeader.biBitCount=32;void* pixels=NULL;
 HBITMAP bitmap=CreateDIBSection(memory,&bitmapInfo,DIB_RGB_COLORS,&pixels,NULL,0);HGDIOBJ oldBitmap=SelectObject(memory,bitmap);
 SendMessageW(editor,EM_SETSEL,3,6);DWORD paintFirst=0,paintLast=0;SendMessageW(editor,EM_GETSEL,(WPARAM)&paintFirst,(LPARAM)&paintLast);
 DWORD paintBegan=GetTickCount();for(int i=0;i<100;++i)SendMessageW(editor,WM_PRINTCLIENT,(WPARAM)memory,PRF_CLIENT);DWORD paintElapsed=GetTickCount()-paintBegan;
 DWORD afterFirst=0,afterLast=0;SendMessageW(editor,EM_GETSEL,(WPARAM)&afterFirst,(LPARAM)&afterLast);
 out<<"METRIC 100 print-client repaints_ms="<<paintElapsed<<"\n";
 Check("native underline paint preserves Unicode text and selection",Text(editor)==unicode && paintFirst==afterFirst && paintLast==afterLast);
 Check("repeated underline repaint is bounded",paintElapsed<1500);
 SelectObject(memory,oldBitmap);DeleteObject(bitmap);DeleteDC(memory);

 const std::wstring original=L"Dragon right.";
 Set(editor,original);SendMessageW(editor,EM_SETSEL,7,12);SendMessageW(editor,EM_REPLACESEL,TRUE,(LPARAM)L"qzxvvaal");ws::Refresh(editor);
 DWORD first=0,last=0;SendMessageW(editor,EM_GETSEL,(WPARAM)&first,(LPARAM)&last);
 Check("ordinary edit is undoable before spell completion",SendMessageW(editor,EM_CANUNDO,0,0)!=0);
 Check("asynchronous check preserves edit selection",Wait([&](){return Settled(editor,L"Dragon qzxvvaal.");}) && [&](){DWORD a=0,b=0;SendMessageW(editor,EM_GETSEL,(WPARAM)&a,(LPARAM)&b);return a==first && b==last;}());
 SendMessageW(editor,WM_UNDO,0,0);Check("spell checking leaves the original native undo operation intact",Text(editor)==original);

 Set(editor,L"vralqzx");Check("custom game word starts as a native misspelling",Wait([&](){return Settled(editor,L"vralqzx") && Word(editor,L"vralqzx");}));
 Check("game dictionary accepts a fictional custom word",ws::AddWord(editor,L"vralqzx",true));
 ws::Refresh(editor);Check("game dictionary suppresses only its accepted word",Wait([&](){return Settled(editor,L"vralqzx") && !Word(editor,L"vralqzx");}));
 context.gameKey=L"fictional-beta";context.gameLabel=L"Fictional Beta";ws::Refresh(editor);
 Check("game dictionary does not leak to another game context",Wait([&](){return Settled(editor,L"vralqzx") && Word(editor,L"vralqzx");}));
 Set(editor,L"qzxvvaal");Wait([&](){return Settled(editor,L"qzxvvaal");});
 Check("personal dictionary accepts a fictional word",ws::AddWord(editor,L"qzxvvaal",false));
 ws::Refresh(editor);Check("personal dictionary is applied",Wait([&](){return Settled(editor,L"qzxvvaal") && !Word(editor,L"qzxvvaal");}));
 context.gameKey=L"fictional-alpha";context.gameLabel=L"Fictional Alpha";Set(editor,L"QZXVVAAL");
 Check("personal dictionary follows games and ignores case",Wait([&](){return Settled(editor,L"QZXVVAAL") && ws::Inspect(editor).errors.empty();}));
 std::ifstream personal("personal.txt",std::ios::binary);std::string persisted((std::istreambuf_iterator<char>(personal)),std::istreambuf_iterator<char>());personal.close();
 Check("accepted personal word was actually persisted in this disposable folder",persisted.find("qzxvvaal")!=std::string::npos);
 CreateDirectoryW((context.root+L"\\packs").c_str(),NULL);std::ofstream pack("packs/westeros.txt",std::ios::binary);pack<<"vrxlqzz\n";pack.close();
 Set(editor,L"vrxlqzz");Check("optional game pack starts disabled for a generic game",Wait([&](){return Settled(editor,L"vrxlqzz") && Word(editor,L"vrxlqzz");}));
 context.westerosDefault=true;ws::Refresh(editor);
 Check("a game's enabled Westeros pack filters its installed custom terms",Wait([&](){return Settled(editor,L"vrxlqzz") && !Word(editor,L"vrxlqzz");}));
 context.westerosDefault=false;ws::Refresh(editor);
 Check("disabling the optional pack restores native checking",Wait([&](){return Settled(editor,L"vrxlqzz") && Word(editor,L"vrxlqzz");}));
 const auto normalContext=context;std::ofstream blocked("blocked-root",std::ios::binary);blocked<<"This is a file, not a dictionary directory.";blocked.close();context.root+=L"\\blocked-root";
 Set(editor,L"vrxlqzz");Wait([&](){return Settled(editor,L"vrxlqzz");});
 Check("dictionary-save request queues without blocking when its path is unwritable",ws::AddWord(editor,L"vrxlqzz",false));
 Check("failed dictionary persistence is reported instead of claiming the word saved",Wait([&](){return !ws::Inspect(editor).errorMessage.empty();}));
 context=normalContext;Set(editor,L"vrxlqzz");
 Check("a failed save does not change the real disposable personal dictionary",Wait([&](){return Settled(editor,L"vrxlqzz") && Word(editor,L"vrxlqzz");}));

 SendMessageW(editor,WM_IME_STARTCOMPOSITION,0,0);Set(editor,L"intermediate qzxvvaal");Pump();
 Check("IME composition messages do not change native text",Text(editor)==L"intermediate qzxvvaal");
 SendMessageW(editor,WM_IME_ENDCOMPOSITION,0,0);ws::Refresh(editor);
 Check("checking resumes after composition ends",Wait([&](){return Settled(editor,L"intermediate qzxvvaal");}));

 DWORD began=GetTickCount();for(int i=0;i<250;++i)Set(editor,L"rapid qzxvvaal "+std::to_wstring(i));DWORD elapsed=GetTickCount()-began;
 out<<"METRIC 250 edit-refresh calls_ms="<<elapsed<<"\n";
 Check("rapid edit notifications do not wait for the spelling engine",elapsed<1500);
 const std::wstring latest=L"The dragon walks home.";Set(editor,latest);
 Check("stale worker results cannot replace the latest native text snapshot",Wait([&](){return Settled(editor,latest);}) && Text(editor)==latest);
 const std::wstring longDraft(16385,L'x');Set(editor,longDraft);
 Check("very large drafts preserve text and explicitly pause inline analysis",Wait([&](){return !ws::Inspect(editor).pending && !ws::Inspect(editor).errorMessage.empty();}) && Text(editor)==longDraft);
 Set(editor,L"@pemit me=qzxvval\r\n&description #123=qzxvval\r\nhttps://example.invalid/qzxvval");
 Check("game code and URL tokens are not offered as prose corrections",Wait([&](){auto snap=ws::Inspect(editor);return !snap.pending && snap.text==Text(editor) && snap.errors.empty();}));
 Set(editor,L"destruction qzxvvaal");DestroyWindow(editor);Pump();
 Check("destroying an editor during queued work removes its native window",!IsWindow(editor));
 HWND replacement=CreateWindowExW(0,L"EDIT",L"replacement",WS_CHILD|ES_MULTILINE,0,0,600,240,parent,(HMENU)10,GetModuleHandle(NULL),NULL);
 Check("replacement control can attach independently after prior destruction",ws::Attach(replacement,[&](){return context;}));
 Set(replacement,L"QZXVVAAL vralqzx");Check("replacement editor reloads persistent personal and game dictionaries",Wait([&](){return Settled(replacement,L"QZXVVAAL vralqzx") && ws::Inspect(replacement).errors.empty();}) && Text(replacement)==L"QZXVVAAL vralqzx");
 HWND ansi=CreateWindowExA(0,"EDIT","",WS_CHILD|ES_MULTILINE,0,0,600,240,parent,(HMENU)11,GetModuleHandle(NULL),NULL);
 Check("legacy ANSI Main-style editor attaches without replacing its native procedure",ansi && !IsWindowUnicode(ansi) && ws::Attach(ansi,[&](){return context;}));
 const std::wstring ansiText=L"Caf\x00e9 vrxlqzz";Set(ansi,ansiText);
 Check("ANSI editor retains supported accented text and native error ranges",Wait([&](){return Settled(ansi,ansiText) && Word(ansi,L"vrxlqzz");}) && Text(ansi)==ansiText);
 std::vector<std::wstring> closingWords;bool queuedClosing=true;
 for(int i=0;i<16;++i){HWND closing=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_MULTILINE,0,0,400,100,parent,(HMENU)12,GetModuleHandle(NULL),NULL);std::wstring word=L"closeqz";word+=(wchar_t)(L'a'+i);
   bool attached=ws::Attach(closing,[&](){return context;});bool accepted=attached && ws::AddWord(closing,word,false);queuedClosing=queuedClosing&&accepted;if(accepted)closingWords.push_back(word);DestroyWindow(closing);}
 Check("short dictionary-save burst queues all accepted requests",queuedClosing);
 Check("accepted dictionary saves survive immediate editor destruction",Wait([&](){auto words=ws::ReadFile(context.root+L"\\personal.txt");for(const auto& word:closingWords)if(words.find(ws::Utf8(word)+"\n")==std::string::npos)return false;return true;}));
 ws::DictionaryDialog manager;manager.state=ws::Editors().at(replacement);manager.context=context;
 std::vector<DWORD> dialogStorage(128,0);auto form=(DLGTEMPLATE*)dialogStorage.data();form->style=WS_POPUP|DS_MODALFRAME;form->cx=340;form->cy=260;
 WORD* tail=(WORD*)((BYTE*)form+sizeof(DLGTEMPLATE));*tail++=0;*tail++=0;*tail=0;
 HWND dictionary=CreateDialogIndirectParamW(GetModuleHandle(NULL),form,parent,ws::DictionaryDialog::Procedure,(LPARAM)&manager);
 const int dictionaryScopeId=GetDlgCtrlID(FindWindowExW(dictionary,NULL,L"COMBOBOX",NULL));
 Check("dictionary manager creates actual controls in a hidden native dialog",dictionary && !IsWindowVisible(dictionary) && GetDlgItem(dictionary,4));
 Check("manager applies its optional modern styling hook after native controls exist",polishedDialogs==1);
 Check("manager explicitly separates personal game and read-only pack scopes",SendDlgItemMessageW(dictionary,dictionaryScopeId,CB_GETCOUNT,0,0)==3);
 Check("dictionary scope chooser has a unique non-Cancel native control ID",dictionaryScopeId!=IDCANCEL && dictionaryScopeId>0);
 SetDlgItemTextW(dictionary,4,L"two words");SendMessageW(dictionary,WM_COMMAND,MAKEWPARAM(5,BN_CLICKED),(LPARAM)GetDlgItem(dictionary,5));
 Check("manager rejects ambiguous multi-word additions with an explanation",Text(GetDlgItem(dictionary,7)).find(L"Enter one word")!=std::wstring::npos);
 SetDlgItemTextW(dictionary,4,L"managrqzx");SendMessageW(dictionary,WM_COMMAND,MAKEWPARAM(5,BN_CLICKED),(LPARAM)GetDlgItem(dictionary,5));
 Check("manager Add persists a personal word through its actual native button handler",Wait([&](){return ws::ReadFile(context.root+L"\\personal.txt").find("managrqzx\n")!=std::string::npos && Text(GetDlgItem(dictionary,7))==L"Dictionary saved.";}));
 LRESULT selected=SendDlgItemMessageW(dictionary,3,LB_FINDSTRINGEXACT,-1,(LPARAM)L"managrqzx");SendDlgItemMessageW(dictionary,3,LB_SETCURSEL,selected,0);SendMessageW(dictionary,WM_COMMAND,MAKEWPARAM(6,BN_CLICKED),(LPARAM)GetDlgItem(dictionary,6));
 Check("manager Remove persists removal of the selected personal word",Wait([&](){return ws::ReadFile(context.root+L"\\personal.txt").find("managrqzx\n")==std::string::npos && Text(GetDlgItem(dictionary,7))==L"Dictionary saved.";}));
 SetDlgItemTextW(dictionary,4,L"enterqzx");SendMessageW(dictionary,WM_COMMAND,IDOK,0);
 Check("manager Enter action adds the drafted word instead of closing the dialog",Wait([&](){return ws::ReadFile(context.root+L"\\personal.txt").find("enterqzx\n")!=std::string::npos && Text(GetDlgItem(dictionary,7))==L"Dictionary saved.";}) && IsWindow(dictionary));
 SendDlgItemMessageW(dictionary,dictionaryScopeId,CB_SETCURSEL,1,0);SendMessageW(dictionary,WM_COMMAND,MAKEWPARAM(dictionaryScopeId,CBN_SELCHANGE),(LPARAM)GetDlgItem(dictionary,dictionaryScopeId));
 SetDlgItemTextW(dictionary,4,L"gameqzx");SendMessageW(dictionary,WM_COMMAND,MAKEWPARAM(5,BN_CLICKED),(LPARAM)GetDlgItem(dictionary,5));
 Check("manager Add writes the selected game dictionary instead of personal scope",Wait([&](){return ws::ReadFile(ws::GameFile(context)+L".txt").find("gameqzx\n")!=std::string::npos && Text(GetDlgItem(dictionary,7))==L"Dictionary saved.";}) && ws::ReadFile(context.root+L"\\personal.txt").find("gameqzx\n")==std::string::npos);
 selected=SendDlgItemMessageW(dictionary,3,LB_FINDSTRINGEXACT,-1,(LPARAM)L"gameqzx");SendDlgItemMessageW(dictionary,3,LB_SETCURSEL,selected,0);SendMessageW(dictionary,WM_COMMAND,MAKEWPARAM(6,BN_CLICKED),(LPARAM)GetDlgItem(dictionary,6));
 Check("manager Remove deletes only the selected game entry",Wait([&](){return ws::ReadFile(ws::GameFile(context)+L".txt").find("gameqzx\n")==std::string::npos && Text(GetDlgItem(dictionary,7))==L"Dictionary saved.";}) && ws::ReadFile(ws::GameFile(context)+L".txt").find("vralqzx\n")!=std::string::npos);
 SendDlgItemMessageW(dictionary,dictionaryScopeId,CB_SETCURSEL,2,0);SendMessageW(dictionary,WM_COMMAND,MAKEWPARAM(dictionaryScopeId,CBN_SELCHANGE),(LPARAM)GetDlgItem(dictionary,dictionaryScopeId));
 Check("bundled word pack has visible read-only controls and its installed vocabulary",!IsWindowEnabled(GetDlgItem(dictionary,4))&&!IsWindowEnabled(GetDlgItem(dictionary,5))&&!IsWindowEnabled(GetDlgItem(dictionary,6))&&SendDlgItemMessageW(dictionary,3,LB_FINDSTRINGEXACT,-1,(LPARAM)L"vrxlqzz")!=LB_ERR);
 bool bounded=true;RECT bounds;GetClientRect(dictionary,&bounds);for(int id=2;id<=8;++id){RECT r;GetWindowRect(GetDlgItem(dictionary,id),&r);MapWindowPoints(NULL,dictionary,(POINT*)&r,2);if(id!=2)bounded=bounded&&r.left>=0&&r.top>=0&&r.right<=bounds.right&&r.bottom<=bounds.bottom;}
 Check("manager controls fit within the native dialog client area",bounded);
 context.gameKey=L"fictional-other";Check("manager rejects ownership after its editor changes game context",!manager.ValidOwner());context=manager.context;
 DestroyWindow(dictionary);
 ws::Context blockedContext=context;blockedContext.root+=L"\\blocked-root";
 HWND blockedEditor=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_MULTILINE,0,0,400,100,parent,(HMENU)13,GetModuleHandle(NULL),NULL);ws::Attach(blockedEditor,[&](){return blockedContext;});
 ws::DictionaryDialog failedManager;failedManager.state=ws::Editors().at(blockedEditor);failedManager.context=blockedContext;
 HWND failedDialog=CreateDialogIndirectParamW(GetModuleHandle(NULL),form,parent,ws::DictionaryDialog::Procedure,(LPARAM)&failedManager);
 SetDlgItemTextW(failedDialog,4,L"badsaveqzx");SendMessageW(failedDialog,WM_COMMAND,MAKEWPARAM(5,BN_CLICKED),(LPARAM)GetDlgItem(failedDialog,5));
 Check("another editor's accepted save remains independently durable",ws::AddWord(replacement,L"othersaveqzx",false) && Wait([&](){return ws::ReadFile(context.root+L"\\personal.txt").find("othersaveqzx\n")!=std::string::npos;}));
 Check("failed manager save cannot borrow another editor's success acknowledgment",Wait([&](){return Text(GetDlgItem(failedDialog,7)).find(L"could not be saved")!=std::wstring::npos;}));
 DestroyWindow(failedDialog);DestroyWindow(blockedEditor);
 ws::Detach(ansi);ws::Detach(replacement);DestroyWindow(parent);Pump();
 Check("spell checking never changes this thread's original input focus",GetFocus()==originalFocus);
 out<<(good?"PASS":"FAIL")<<" hidden native spell probe; no physical IME or visual underline acceptance claim\n";return good?0:1;
}
