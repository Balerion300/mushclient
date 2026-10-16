#pragma once
// Native EDIT spelling. Analysis and dictionary I/O never run in a key handler.
#include <windows.h>
#include <commctrl.h>
// COM activation is optional at runtime. Expose SDK interface declarations even
// in legacy-targeted native fixtures; no Windows 8 entry point is statically imported.
#pragma push_macro("_WIN32_WINNT")
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0602
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#include <spellcheck.h>
#pragma pop_macro("_WIN32_WINNT")
#include <imm.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cwctype>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#pragma comment(lib,"ole32.lib")

namespace workspace_spell {
struct Context { std::wstring root,gameKey,gameLabel; bool westerosDefault=false; void(*polishDialog)(HWND)=NULL; };
struct Misspelling { ULONG first=0,length=0; std::wstring word; std::vector<std::wstring> suggestions; };
struct Snapshot {
  std::wstring text,errorMessage; std::vector<Misspelling> errors;
  bool pending=false,engineAvailable=false,enabled=true,westeros=false;
  unsigned long long workerMillis=0,revision=0;
};
inline std::wstring Fold(std::wstring word) {
  if(!word.empty()){std::wstring folded(word.size(),0);if(LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,word.data(),(int)word.size(),&folded[0],(int)folded.size(),NULL,NULL,0))word.swap(folded);}
  for(auto& c:word)if(c==0x2019)c=L'\'';
  return word;
}
inline bool WordValid(const std::wstring& word) {
  if(word.empty() || word.size()>80)return false;
  bool letter=false;
  for(auto c:word){if(IsCharAlphaW(c)){letter=true;continue;}if(c!=L'\'' && c!=0x2019 && c!=L'-')return false;}
  return letter;
}
inline std::string Utf8(const std::wstring& text) {
  int n=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),NULL,0,NULL,NULL);
  std::string out(n,0);if(n)WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),&out[0],n,NULL,NULL);return out;
}
inline std::wstring Wide(const std::string& text) {
  int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),NULL,0);
  std::wstring out(n,0);if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),&out[0],n);return out;
}
inline std::wstring GameFile(const Context& c) {
  unsigned long long hash=14695981039346656037ULL;
  for(auto b:Utf8(Fold(c.gameKey))){hash^=(unsigned char)b;hash*=1099511628211ULL;}
  std::wostringstream name;name<<c.root<<L"\\game-"<<std::hex<<hash;return name.str();
}
inline std::string ReadFile(const std::wstring& path) {
  HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE)return {};
  LARGE_INTEGER size={};std::string data;
  if(GetFileSizeEx(file,&size) && size.QuadPart>=0 && size.QuadPart<=1024*1024){data.resize((size_t)size.QuadPart);DWORD read=0;if(!::ReadFile(file,data.empty()?NULL:&data[0],(DWORD)data.size(),&read,NULL))data.clear();else data.resize(read);}
  CloseHandle(file);return data;
}
inline bool WriteFile(const std::wstring& path,const std::string& data) {
  // Replace only after a complete write, retaining the previous dictionary on failure.
  const std::wstring stage=path+L".tmp";
  HANDLE file=CreateFileW(stage.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(file==INVALID_HANDLE_VALUE)return false;
  DWORD written=0;bool ok=::WriteFile(file,data.data(),(DWORD)data.size(),&written,NULL)!=FALSE && written==data.size();
  if(ok)ok=FlushFileBuffers(file)!=FALSE;CloseHandle(file);
  if(ok)ok=MoveFileExW(stage.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
  if(!ok)DeleteFileW(stage.c_str());return ok;
}
inline std::set<std::wstring> Words(const std::wstring& path) {
  std::set<std::wstring> result;std::istringstream lines(ReadFile(path));std::string line;
  while(result.size()<10000 && std::getline(lines,line)){
    if(line.size()>1024)continue;if(!line.empty() && line.back()=='\r')line.pop_back();
    if(line.size()>=3 && (unsigned char)line[0]==0xef && (unsigned char)line[1]==0xbb && (unsigned char)line[2]==0xbf)line.erase(0,3);
    auto word=Wide(line);if(WordValid(word))result.insert(Fold(word));
  }return result;
}
inline std::string WordFile(const std::set<std::wstring>& words) {
  std::string out;for(const auto& word:words)out+=Utf8(word)+"\n";return out;
}
struct State {
  HWND window=NULL;std::function<Context()> context;
  std::mutex mutex;Snapshot ready;bool readyWaiting=false;
  unsigned long long completedAction=0;std::wstring actionError;
  Snapshot shown;Context current;std::wstring observed;
  unsigned long long revision=0,changed=0,submitted=0,dictionaryEpoch=0;
  bool composing=false,alive=true;std::set<std::wstring> ignored;
};
struct Job {
  std::weak_ptr<State> state;Context context;Snapshot result;
  std::wstring add;int action=0; // 1 personal, 2 game, 3 enable, 4 pack, 5 remove personal, 6 remove game
  bool value=false;
  unsigned long long actionId=0;
};
inline std::atomic<unsigned long long>& DictionaryEpoch(){static std::atomic<unsigned long long> epoch(1);return epoch;}
inline unsigned long long NextActionId(){static std::atomic<unsigned long long> next(0);return ++next;}
inline bool Checkable(const std::wstring& text,const Misspelling& error) {
  if(!WordValid(error.word))return false;
  const size_t first=error.first,last=first+error.length;
  if(first && (text[first-1]==L'@' || text[first-1]==L'%' || text[first-1]==L'$' || text[first-1]==L'#' || text[first-1]==L'+' || text[first-1]==L'&'))return false;
  if(last<text.size() && text[last]==L'(')return false;
  size_t begin=first,end=last;while(begin && !iswspace(text[begin-1]))--begin;while(end<text.size()&&!iswspace(text[end]))++end;
  auto token=text.substr(begin,end-begin);
  if(token.find(L"://")!=std::wstring::npos || token.find(L'@')!=std::wstring::npos)return false;
  const size_t line=text.rfind(L'\n',first);const size_t start=line==std::wstring::npos?0:line+1;
  if(start<text.size() && (text[start]==L'@'||text[start]==L'&'))return false;
  return true;
}
class Worker {
  std::mutex mutex_;std::condition_variable wake_;std::deque<Job> queue_;bool stop_=false;std::thread thread_;
  void Run() {
    const HRESULT initialized=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    ISpellCheckerFactory* factory=NULL;ISpellChecker* checker=NULL;
    if(SUCCEEDED(initialized) && SUCCEEDED(CoCreateInstance(__uuidof(SpellCheckerFactory),NULL,CLSCTX_INPROC_SERVER,__uuidof(ISpellCheckerFactory),(void**)&factory))){factory->CreateSpellChecker(L"en-US",&checker);factory->Release();}
    for(;;){Job job;bool ending=false;{std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[&]{return stop_||!queue_.empty();});ending=stop_;if(ending)queue_.erase(std::remove_if(queue_.begin(),queue_.end(),[](const Job& item){return !item.action;}),queue_.end());if(queue_.empty()){if(ending)break;continue;}job=std::move(queue_.front());queue_.pop_front();}
      auto state=job.state.lock();if(!state&&!job.action)continue;
      const ULONGLONG started=GetTickCount64();auto& out=job.result;out.engineAvailable=checker!=NULL;
      const auto& c=job.context;const auto game=GameFile(c);
      auto personal=Words(c.root+L"\\personal.txt"),specific=Words(game+L".txt");
      const auto settings=ReadFile(game+L".settings");
      out.enabled=settings.find("enabled=0")==std::string::npos;
      out.westeros=settings.find("westeros=1")!=std::string::npos || (settings.find("westeros=0")==std::string::npos && c.westerosDefault);
      if(job.action){
        CreateDirectoryW(c.root.c_str(),NULL);bool saved=false;
        if(job.action==1||job.action==2||job.action==5||job.action==6){auto& target=(job.action==1||job.action==5)?personal:specific;
          if(WordValid(job.add) && (job.action>=5 || target.size()<10000 || target.count(Fold(job.add)))){if(job.action<=2)target.insert(Fold(job.add));else target.erase(Fold(job.add));saved=WriteFile((job.action==1||job.action==5)?c.root+L"\\personal.txt":game+L".txt",WordFile(target));}}
        else {if(job.action==3)out.enabled=job.value;if(job.action==4)out.westeros=job.value;
          saved=WriteFile(game+L".settings",std::string("enabled=")+(out.enabled?"1":"0")+"\nwesteros="+(out.westeros?"1":"0")+"\n");}
        if(!saved)out.errorMessage=L"Dictionary could not be saved. Check that the client folder is writable.";
        else ++DictionaryEpoch();
        if(state&&job.actionId){std::lock_guard<std::mutex> lock(state->mutex);state->completedAction=job.actionId;state->actionError=saved?L"":out.errorMessage;}
      }
      if(!checker)out.errorMessage=L"Windows English spelling is unavailable. Install the English language spelling features in Windows Settings.";
      if(out.enabled && checker && !ending){
        auto accepted=std::move(personal);accepted.insert(specific.begin(),specific.end());
        if(out.westeros){auto pack=Words(c.root+L"\\packs\\westeros.txt");accepted.insert(pack.begin(),pack.end());}
        IEnumSpellingError* errors=NULL;
        if(SUCCEEDED(checker->Check(out.text.c_str(),&errors)) && errors){
          ISpellingError* error=NULL;
          while(out.errors.size()<128 && errors->Next(&error)==S_OK){
            Misspelling item;CORRECTIVE_ACTION action=CORRECTIVE_ACTION_NONE;
            error->get_StartIndex(&item.first);error->get_Length(&item.length);error->get_CorrectiveAction(&action);error->Release();error=NULL;
            if(!item.length || item.first>out.text.size() || item.length>out.text.size()-item.first || action==CORRECTIVE_ACTION_NONE)continue;
            item.word=out.text.substr(item.first,item.length);
            if(!Checkable(out.text,item)||accepted.count(Fold(item.word)))continue;
            IEnumString* suggestions=NULL;
            if(SUCCEEDED(checker->Suggest(item.word.c_str(),&suggestions))&&suggestions){LPOLESTR word=NULL;ULONG count=0;
              while(item.suggestions.size()<6 && suggestions->Next(1,&word,&count)==S_OK){if(word && wcslen(word)<=120)item.suggestions.push_back(word);CoTaskMemFree(word);word=NULL;}suggestions->Release();}
            // Game words participate in suggestions, without changing Windows' global dictionary.
            const auto folded=Fold(item.word);
            for(const auto& candidate:accepted){if(item.suggestions.size()>=8)break;if(candidate.size()+1<folded.size() || candidate.size()>folded.size()+1)continue;
              size_t edits=0,i=0,j=0;while(i<folded.size()&&j<candidate.size()){if(folded[i]==candidate[j]){++i;++j;}else{++edits;if(i+1<folded.size()&&j+1<candidate.size()&&folded[i]==candidate[j+1]&&folded[i+1]==candidate[j]){i+=2;j+=2;}else{if(folded.size()>=candidate.size())++i;if(candidate.size()>=folded.size())++j;}if(edits>1)break;}}
              edits+=folded.size()-i+candidate.size()-j;if(edits<=1 && candidate!=folded && std::find(item.suggestions.begin(),item.suggestions.end(),candidate)==item.suggestions.end())item.suggestions.push_back(candidate);}
            out.errors.push_back(std::move(item));
          }errors->Release();
        }
      }
      out.pending=false;out.workerMillis=GetTickCount64()-started;
      if(state){std::lock_guard<std::mutex> lock(state->mutex);if(state->alive){state->ready=std::move(out);state->readyWaiting=true;}}
    }
    if(checker)checker->Release();if(SUCCEEDED(initialized))CoUninitialize();
  }
public:
  Worker():thread_([this]{Run();}){}
  ~Worker(){{std::lock_guard<std::mutex> lock(mutex_);stop_=true;}wake_.notify_one();thread_.join();}
  bool Submit(Job job){if(job.context.root.empty()||job.context.gameKey.empty())return false;std::lock_guard<std::mutex> lock(mutex_);
    if(!job.action){auto state=job.state.lock();for(auto it=queue_.begin();it!=queue_.end();)if(!it->action&&it->state.lock()==state)it=queue_.erase(it);else ++it;}
    if(queue_.size()>=64)return false;
    queue_.push_back(std::move(job));wake_.notify_one();return true;}
};
inline Worker& Engine(){static Worker engine;return engine;}
inline std::map<HWND,std::shared_ptr<State>>& Editors(){static std::map<HWND,std::shared_ptr<State>> editors;return editors;}
inline std::wstring Text(HWND window){int n=GetWindowTextLengthW(window);if(n<0||n>65536)return {};std::wstring text(n+1,0);int read=GetWindowTextW(window,&text[0],n+1);text.resize(read);return text;}
inline bool Same(const Context& a,const Context& b){return a.root==b.root&&a.gameKey==b.gameKey&&a.westerosDefault==b.westerosDefault;}
inline void Refresh(HWND window) {
  auto found=Editors().find(window);if(found==Editors().end())return;auto state=found->second;
  const auto context=state->context();const auto text=Text(window);const auto now=GetTickCount64();
  if(text!=state->observed || !Same(context,state->current) || state->dictionaryEpoch!=DictionaryEpoch()){
    if(!Same(context,state->current))state->ignored.clear();state->observed=text;state->current=context;state->dictionaryEpoch=DictionaryEpoch();
    ++state->revision;state->changed=now;state->shown.errors.clear();state->shown.pending=true;InvalidateRect(window,NULL,FALSE);
  }
  {std::lock_guard<std::mutex> lock(state->mutex);if(state->readyWaiting){if(state->ready.revision==state->revision && state->ready.text==text){state->shown=std::move(state->ready);InvalidateRect(window,NULL,FALSE);}state->readyWaiting=false;}}
  if(!state->composing && state->submitted!=state->revision && now-state->changed>=350 && !context.gameKey.empty()){
    Job job;job.state=state;job.context=context;job.result.text=text;job.result.revision=state->revision;
    // Bound work for pasted code or unusually large drafts; typing remains native.
    if(text.size()>16384){state->shown.pending=false;state->shown.errorMessage=L"Inline spelling pauses for drafts over 16,384 characters.";state->submitted=state->revision;return;}
    if(Engine().Submit(std::move(job)))state->submitted=state->revision;
  }
}
inline Snapshot Inspect(HWND window){Refresh(window);auto found=Editors().find(window);return found==Editors().end()?Snapshot():found->second->shown;}
inline bool HandlesInput(HWND window){auto found=Editors().find(window);return found!=Editors().end() && !found->second->current.gameKey.empty() && (found->second->shown.engineAvailable||found->second->shown.errorMessage.empty());}
inline bool AddWord(HWND window,const std::wstring& word,bool game) {
  auto found=Editors().find(window);if(found==Editors().end()||!WordValid(word))return false;
  auto state=found->second;Job job;job.state=state;job.context=state->context();job.add=word;job.action=game?2:1;job.result.text=Text(window);job.result.revision=state->revision;
  if(!Engine().Submit(std::move(job))){state->shown.errorMessage=L"Spelling is busy. The word was not saved; try again.";return false;}return true;
}
inline POINT Position(HWND window,LONG index){LRESULT result=SendMessageW(window,EM_POSFROMCHAR,index,0);POINT p={(short)LOWORD(result),(short)HIWORD(result)};return p;}
inline void Paint(const std::shared_ptr<State>& state,HDC dc=NULL) {
  if(state->composing || state->shown.errors.empty() || state->observed!=Text(state->window))return;
  HWND window=state->window;bool release=!dc;if(!dc)dc=GetDC(window);if(!dc)return;
  int saved=SaveDC(dc);RECT client;GetClientRect(window,&client);IntersectClipRect(dc,client.left+1,client.top,client.right-1,client.bottom);
  HFONT font=(HFONT)SendMessageW(window,WM_GETFONT,0,0);if(font)SelectObject(dc,font);TEXTMETRICW metrics={};GetTextMetricsW(dc,&metrics);
  HPEN pen=CreatePen(PS_SOLID,1,RGB(210,45,55));SelectObject(dc,pen);
  for(const auto& error:state->shown.errors){if(state->ignored.count(Fold(error.word)))continue;
    for(ULONG i=error.first;i<error.first+error.length;++i){POINT p=Position(window,i),next=Position(window,i+1);if(p.y+metrics.tmHeight<client.top||p.y>=client.bottom||p.x>=client.right)continue;
      SIZE extent={};GetTextExtentPoint32W(dc,state->observed.c_str()+i,1,&extent);
      int end=(next.y==p.y && next.x>p.x)?next.x:p.x+extent.cx;end=(std::min)(end,(int)client.right-1);int y=p.y+metrics.tmHeight-1;
      for(int x=(std::max)(p.x,client.left+1);x<end;x+=4){MoveToEx(dc,x,y,NULL);LineTo(dc,(std::min)(x+2,end),y-1);LineTo(dc,(std::min)(x+4,end),y);}
    }
  }RestoreDC(dc,saved);DeleteObject(pen);if(release)ReleaseDC(window,dc);
}
struct DictionaryDialog {
  std::shared_ptr<State> state;Context context;std::vector<std::wstring> words;
  unsigned long long epoch=0,waiting=0,pendingAction=0;int scope=0;
  bool ValidOwner()const{auto found=Editors().find(state->window);return state->alive&&found!=Editors().end()&&found->second==state&&Same(context,state->context());}
  void Load(HWND window){
    auto path=scope==0?context.root+L"\\personal.txt":scope==1?GameFile(context)+L".txt":context.root+L"\\packs\\westeros.txt";
    auto entries=Words(path);words.assign(entries.begin(),entries.end());HWND list=GetDlgItem(window,3);
    SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);
    for(const auto& word:words)SendMessageW(list,LB_ADDSTRING,0,(LPARAM)word.c_str());
    SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,NULL,TRUE);
    EnableWindow(GetDlgItem(window,5),scope!=2);EnableWindow(GetDlgItem(window,6),scope!=2);EnableWindow(GetDlgItem(window,4),scope!=2);
    SetDlgItemTextW(window,7,scope==0?L"Personal words apply to every game in this client.":scope==1?(L"These words apply to every profile in "+context.gameLabel+L".").c_str():L"Bundled Westeros vocabulary is read-only. Enable it per game from the input's spelling menu.");
    epoch=DictionaryEpoch();
  }
  static INT_PTR CALLBACK Procedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    auto self=(DictionaryDialog*)GetWindowLongPtrW(window,DWLP_USER);
    if(message==WM_INITDIALOG){self=(DictionaryDialog*)lp;SetWindowLongPtrW(window,DWLP_USER,(LONG_PTR)self);
      RECT rect;GetClientRect(window,&rect);const int width=rect.right,height=rect.bottom;
      typedef UINT(WINAPI* DpiFunction)(HWND);auto getDpi=(DpiFunction)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");const UINT dpi=getDpi?getDpi(window):96;
      auto S=[&](int n){return MulDiv(n,(int)dpi,96);};HFONT font=(HFONT)SendMessageW(window,WM_GETFONT,0,0);if(!font)font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
      auto control=[&](const wchar_t* type,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){HWND child=CreateWindowExW(type==std::wstring(L"LISTBOX")||type==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE);return child;};
      control(L"STATIC",L"Choose a &dictionary",0,S(16),S(10),width-S(32),S(22),8);
      HWND scope=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,S(16),S(34),width-S(32),S(150),102);
      for(const auto& name:std::vector<std::wstring>{L"Personal dictionary",L"Game dictionary: "+self->context.gameLabel,L"Westeros word pack (read-only)"})SendMessageW(scope,CB_ADDSTRING,0,(LPARAM)name.c_str());SendMessageW(scope,CB_SETCURSEL,0,0);
      control(L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL|WS_TABSTOP|LBS_NOINTEGRALHEIGHT,S(16),S(70),width-S(32),height-S(240),3);
      control(L"STATIC",L"&New word",0,S(16),height-S(162),width-S(32),S(22),9);
      control(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,S(16),height-S(134),width-S(220),S(27),4);SendDlgItemMessageW(window,4,EM_SETLIMITTEXT,80,0);
      control(L"BUTTON",L"&Add word",WS_TABSTOP|BS_DEFPUSHBUTTON,width-S(194),height-S(134),S(82),S(27),5);SendMessageW(window,DM_SETDEFID,5,0);
      control(L"BUTTON",L"&Remove",WS_TABSTOP|BS_PUSHBUTTON,width-S(104),height-S(134),S(88),S(27),6);
      control(L"STATIC",L"",0,S(16),height-S(96),width-S(32),S(48),7);
      control(L"BUTTON",L"&Close",WS_TABSTOP|BS_PUSHBUTTON,width-S(104),height-S(36),S(88),S(27),IDCANCEL);
      self->Load(window);if(self->context.polishDialog)self->context.polishDialog(window);SetTimer(window,1,150,NULL);return TRUE;
    }
    if(!self)return FALSE;
    if(message==WM_CLOSE){EndDialog(window,0);return TRUE;}
    if(message==WM_TIMER){if(!self->ValidOwner()){EndDialog(window,0);return TRUE;}
      unsigned long long completed=0;std::wstring actionError;{std::lock_guard<std::mutex> lock(self->state->mutex);completed=self->state->completedAction;actionError=self->state->actionError;}
      if(self->waiting && completed==self->pendingAction){self->waiting=0;self->Load(window);SetDlgItemTextW(window,7,actionError.empty()?L"Dictionary saved.":actionError.c_str());}
      else if(self->waiting && GetTickCount64()-self->waiting>10000){self->waiting=0;self->Load(window);SetDlgItemTextW(window,7,L"Saving is taking longer than expected. Check the word list before trying again.");}
      return TRUE;
    }
    if(message==WM_COMMAND){const UINT id=LOWORD(wp);
      if(id==IDCANCEL){EndDialog(window,0);return TRUE;}
      if(id==102&&HIWORD(wp)==CBN_SELCHANGE){self->scope=(int)SendDlgItemMessageW(window,102,CB_GETCURSEL,0,0);self->Load(window);return TRUE;}
      if((id==5||id==6||id==IDOK)&&self->scope!=2){
        if(!self->ValidOwner()){EndDialog(window,0);return TRUE;}
        std::wstring word;
        const bool adding=id!=6;
        if(adding){wchar_t text[82]={};GetDlgItemTextW(window,4,text,82);word=text;}
        else{int selected=(int)SendDlgItemMessageW(window,3,LB_GETCURSEL,0,0);if(selected>=0&&selected<(int)self->words.size())word=self->words[selected];}
        if(!WordValid(word)){SetDlgItemTextW(window,7,L"Enter one word, optionally containing an apostrophe or hyphen.");return TRUE;}
        Job job;job.state=self->state;job.context=self->context;job.action=self->scope==0?(adding?1:5):(adding?2:6);job.add=word;job.result.text=Text(self->state->window);job.result.revision=self->state->revision;
        self->waiting=GetTickCount64();self->epoch=DictionaryEpoch();self->pendingAction=job.actionId=NextActionId();self->state->shown.errorMessage.clear();if(!Engine().Submit(std::move(job))){self->waiting=0;SetDlgItemTextW(window,7,L"Spelling is busy. Try saving the word again.");return TRUE;}SetDlgItemTextW(window,7,L"Saving dictionary…");SetDlgItemTextW(window,4,L"");return TRUE;
      }
    }return FALSE;
  }
};
inline void Manage(const std::shared_ptr<State>& state){
  DictionaryDialog dialog;dialog.state=state;dialog.context=state->current;
  std::vector<DWORD> storage(128,0);auto form=(DLGTEMPLATE*)storage.data();form->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;form->cx=340;form->cy=260;
  auto tail=(WORD*)((BYTE*)form+sizeof(DLGTEMPLATE));*tail++=0;*tail++=0;const wchar_t* title=L"Spelling dictionaries";wcscpy_s((wchar_t*)tail,100,title);tail+=wcslen(title)+1;*tail++=9;wcscpy_s((wchar_t*)tail,32,L"Segoe UI");
  DialogBoxIndirectParamW(GetModuleHandle(NULL),form,GetAncestor(state->window,GA_ROOT),DictionaryDialog::Procedure,(LPARAM)&dialog);
}
inline void Menu(const std::shared_ptr<State>& state,LPARAM lp) {
  Refresh(state->window);HWND window=state->window;POINT point={(short)LOWORD(lp),(short)HIWORD(lp)};
  if(state->current.root.empty()||state->current.gameKey.empty()){DefSubclassProc(window,WM_CONTEXTMENU,(WPARAM)window,lp);return;}
  DWORD start=0,end=0;SendMessageW(window,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);LONG hit=(LONG)start;
  if(point.x==-1&&point.y==-1){auto at=Position(window,hit);point=at;ClientToScreen(window,&point);}else{POINT client=point;ScreenToClient(window,&client);hit=LOWORD(SendMessageW(window,EM_CHARFROMPOS,0,MAKELPARAM(client.x,client.y)));}
  const auto original=Text(window);Misspelling selected;bool has=false;
  for(const auto& error:state->shown.errors)if(hit>=(LONG)error.first&&hit<(LONG)(error.first+error.length)&&!state->ignored.count(Fold(error.word))){selected=error;has=true;break;}
  HMENU menu=CreatePopupMenu();
  if(has){if(selected.suggestions.empty())AppendMenuW(menu,MF_STRING|MF_GRAYED,0,L"No spelling suggestions");
    for(size_t i=0;i<selected.suggestions.size();++i)AppendMenuW(menu,MF_STRING,100+(UINT)i,selected.suggestions[i].c_str());
    AppendMenuW(menu,MF_STRING,200,L"Ignore this word for this editing session");AppendMenuW(menu,MF_STRING,201,L"Add to personal dictionary");
    AppendMenuW(menu,MF_STRING,202,(L"Add to game dictionary: "+state->current.gameLabel).c_str());AppendMenuW(menu,MF_SEPARATOR,0,NULL);}
  AppendMenuW(menu,MF_STRING|(state->shown.enabled?MF_CHECKED:0),210,L"Check spelling for this game");
  AppendMenuW(menu,MF_STRING|(state->shown.westeros?MF_CHECKED:0),211,L"Use Westeros word pack for this game");
  AppendMenuW(menu,MF_STRING,212,L"Manage personal and game dictionaries…");
  if(!state->shown.errorMessage.empty())AppendMenuW(menu,MF_STRING|MF_GRAYED,0,state->shown.errorMessage.c_str());
  AppendMenuW(menu,MF_SEPARATOR,0,NULL);AppendMenuW(menu,MF_STRING,220,L"Undo");AppendMenuW(menu,MF_STRING,221,L"Cut");AppendMenuW(menu,MF_STRING,222,L"Copy");AppendMenuW(menu,MF_STRING,223,L"Paste");AppendMenuW(menu,MF_STRING,224,L"Select all");AppendMenuW(menu,MF_STRING,225,L"More input actions…");
  const auto context=state->current;const auto revision=state->revision;
  UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window,NULL);DestroyMenu(menu);
  auto registered=Editors().find(window);
  if(!IsWindow(window)||!state->alive||registered==Editors().end()||registered->second!=state)return;
  if(command>=100&&command<=211 && (!Same(context,state->context())||Text(window)!=original||state->revision!=revision))return;
  if(command>=100&&command<100+selected.suggestions.size()&&has){if(Text(window)!=original||state->revision!=revision||!Same(context,state->context()))return;
    SendMessageW(window,EM_SETSEL,selected.first,selected.first+selected.length);SendMessageW(window,EM_REPLACESEL,TRUE,(LPARAM)selected.suggestions[command-100].c_str());}
  else if(command==200){state->ignored.insert(Fold(selected.word));InvalidateRect(window,NULL,TRUE);}
  else if(command==201||command==202){if(Same(context,state->context())&&Text(window)==original)AddWord(window,selected.word,command==202);}
  else if(command==210||command==211){if(!Same(context,state->context()))return;Job job;job.state=state;job.context=context;job.action=command==210?3:4;job.value=command==210?!state->shown.enabled:!state->shown.westeros;job.result.text=original;job.result.revision=revision;Engine().Submit(std::move(job));}
  else if(command==212){if(Same(context,state->context()))Manage(state);}
  else if(command==220)SendMessageW(window,WM_UNDO,0,0);else if(command==221)SendMessageW(window,WM_CUT,0,0);else if(command==222)SendMessageW(window,WM_COPY,0,0);else if(command==223)SendMessageW(window,WM_PASTE,0,0);else if(command==224)SendMessageW(window,EM_SETSEL,0,-1);
  else if(command==225)DefSubclassProc(window,WM_CONTEXTMENU,(WPARAM)window,lp);
}
inline LRESULT CALLBACK Procedure(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR) {
  auto found=Editors().find(window);if(found==Editors().end())return DefSubclassProc(window,message,wp,lp);auto state=found->second;
  if(message==WM_NCDESTROY){KillTimer(window,0x7651);{std::lock_guard<std::mutex> lock(state->mutex);state->alive=false;}RemoveWindowSubclass(window,Procedure,id);Editors().erase(window);return DefSubclassProc(window,message,wp,lp);}
  if(message==WM_CONTEXTMENU){Menu(state,lp);return 0;}
  if(message==WM_IME_STARTCOMPOSITION){state->composing=true;state->shown.errors.clear();InvalidateRect(window,NULL,TRUE);}
  if(message==WM_IME_ENDCOMPOSITION){state->composing=false;state->changed=GetTickCount64();state->submitted=0;}
  if(message==WM_TIMER && wp==0x7651){Refresh(window);return 0;}
  const LRESULT result=DefSubclassProc(window,message,wp,lp);
  auto registered=Editors().find(window);if(!state->alive||registered==Editors().end()||registered->second!=state)return result;
  if(message==WM_PAINT)Paint(state);else if(message==WM_PRINTCLIENT)Paint(state,(HDC)wp);
  if(message==WM_CHAR||message==WM_KEYDOWN||message==WM_SETTEXT||message==EM_REPLACESEL||message==WM_PASTE||message==WM_CUT||message==WM_UNDO){state->changed=GetTickCount64();state->shown.errors.clear();InvalidateRect(window,NULL,FALSE);}
  return result;
}
inline bool Attach(HWND window,std::function<Context()> context) {
  if(!IsWindow(window))return false;auto found=Editors().find(window);if(found!=Editors().end()){found->second->context=std::move(context);return true;}
  auto state=std::make_shared<State>();state->window=window;state->context=std::move(context);
  if(!SetWindowSubclass(window,Procedure,0x7650,0))return false;
  Editors()[window]=state;SetTimer(window,0x7651,120,NULL);Refresh(window);return true;
}
inline void Detach(HWND window){auto found=Editors().find(window);if(found==Editors().end())return;{std::lock_guard<std::mutex> lock(found->second->mutex);found->second->alive=false;}KillTimer(window,0x7651);RemoveWindowSubclass(window,Procedure,0x7650);Editors().erase(found);}
} // namespace workspace_spell
