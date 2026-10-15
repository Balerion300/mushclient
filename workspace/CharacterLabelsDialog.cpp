#include "CharacterLabelsDialog.h"
#include "WorkflowUi.h"
#include <vector>
#include <cstring>

namespace {
enum {Game=101,Character=102};
struct Labels {std::wstring game,character;};
template<class T> void Append(std::vector<unsigned char>& bytes,const T& value){const size_t n=bytes.size();bytes.resize(n+sizeof(value));std::memcpy(bytes.data()+n,&value,sizeof(value));}
void Text(std::vector<unsigned char>& bytes,const wchar_t* value){do{Append(bytes,*value);}while(*value++);}
void Control(std::vector<unsigned char>& bytes,DWORD style,short x,short y,short w,short h,WORD id,WORD type,const wchar_t* label){while(bytes.size()%4)bytes.push_back(0);DLGITEMTEMPLATE item={};item.style=WS_CHILD|WS_VISIBLE|style;item.x=x;item.y=y;item.cx=w;item.cy=h;item.id=id;Append(bytes,item);Append(bytes,WORD(0xffff));Append(bytes,type);Text(bytes,label);Append(bytes,WORD(0));}
std::vector<unsigned char> Template(){std::vector<unsigned char> bytes;DLGTEMPLATE dialog={};dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;dialog.cx=310;dialog.cy=169;dialog.cdit=7;Append(bytes,dialog);Append(bytes,WORD(0));Append(bytes,WORD(0));Text(bytes,L"Game and character labels");Append(bytes,WORD(9));Text(bytes,L"Segoe UI");
 Control(bytes,SS_LEFT,12,12,286,12,WORD(-1),0x82,L"&Game");Control(bytes,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,12,27,286,18,Game,0x81,L"");
 Control(bytes,SS_LEFT,12,54,286,12,WORD(-1),0x82,L"&Character");Control(bytes,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,12,69,286,18,Character,0x81,L"");
 Control(bytes,SS_LEFT,12,97,286,25,WORD(-1),0x82,L"Display labels only. Changing them does not change your login.");
 Control(bytes,WS_TABSTOP|BS_DEFPUSHBUTTON,166,135,62,22,IDOK,0x80,L"&Save");Control(bytes,WS_TABSTOP|BS_PUSHBUTTON,236,135,62,22,IDCANCEL,0x80,L"Cancel");return bytes;}
std::wstring Value(HWND control){wchar_t value[129]={};GetWindowTextW(control,value,129);std::wstring result(value);const auto first=result.find_first_not_of(L" \t\r\n");if(first==std::wstring::npos)return L"";return result.substr(first,result.find_last_not_of(L" \t\r\n")-first+1);}
INT_PTR CALLBACK Proc(HWND window,UINT message,WPARAM wp,LPARAM lp){Labels* labels=(Labels*)GetWindowLongPtrW(window,DWLP_USER);if(message==WM_INITDIALOG){labels=(Labels*)lp;SetWindowLongPtrW(window,DWLP_USER,(LONG_PTR)labels);SetDlgItemTextW(window,Game,labels->game.c_str());SetDlgItemTextW(window,Character,labels->character.c_str());for(int id:{Game,Character})SendDlgItemMessageW(window,id,EM_SETLIMITTEXT,128,0);workflow_ui::Polish(window);workflow_ui::Button(window,IDOK,workspace_style::Icon::Save,true);workflow_ui::Button(window,IDCANCEL,workspace_style::Icon::Close);workspace_style::FitToWorkArea(window);return TRUE;}
 if(message==WM_COMMAND && labels){if(LOWORD(wp)==IDOK){auto game=Value(GetDlgItem(window,Game)),character=Value(GetDlgItem(window,Character));if(game.empty() || character.empty()){MessageBoxW(window,L"Enter a game and character display name.",L"Game and character labels",MB_OK|MB_ICONINFORMATION);return TRUE;}for(const auto& text:{game,character})for(wchar_t c:text)if(c<32 || c==127){MessageBoxW(window,L"Display names cannot contain control characters.",L"Game and character labels",MB_OK|MB_ICONINFORMATION);return TRUE;}labels->game=game;labels->character=character;EndDialog(window,IDOK);return TRUE;}if(LOWORD(wp)==IDCANCEL){EndDialog(window,IDCANCEL);return TRUE;}}return FALSE;}
}
bool ShowCharacterLabelsDialog(HWND owner,std::wstring& game,std::wstring& character){Labels labels={game,character};auto bytes=Template();const auto result=DialogBoxIndirectParamW(GetModuleHandleW(NULL),(const DLGTEMPLATE*)bytes.data(),owner,Proc,(LPARAM)&labels);if(result==IDOK){game=labels.game;character=labels.character;return true;}if(result==-1)MessageBoxW(owner,L"The label editor could not be opened.",L"Game and character labels",MB_OK|MB_ICONERROR);return false;}
