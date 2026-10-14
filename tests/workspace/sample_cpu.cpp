// Sample only an explicitly identified, owned fictional test process.
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
struct WindowCounts {DWORD pid;std::map<std::string,unsigned> classes;};
BOOL CALLBACK CountChild(HWND window,LPARAM data)
{
  WindowCounts* counts=(WindowCounts*)data;DWORD owner=0;GetWindowThreadProcessId(window,&owner);
  if(owner==counts->pid){char name[256]={0};GetClassNameA(window,name,256);++counts->classes[name];}return TRUE;
}
BOOL CALLBACK CountTop(HWND window,LPARAM data)
{
  WindowCounts* counts=(WindowCounts*)data;DWORD owner=0;GetWindowThreadProcessId(window,&owner);
  if(owner==counts->pid){CountChild(window,data);EnumChildWindows(window,CountChild,data);}return TRUE;
}
int main(int argc,char** argv)
{
  if(argc!=5&&argc!=6){fprintf(stderr,"Usage: sample-cpu PID expected-exe symbol-directory output [--windows-only]\n");return 2;}
  DWORD pid=strtoul(argv[1],NULL,10);
  HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
  if(!process)return 3;
  char path[MAX_PATH]={0};GetModuleFileNameExA(process,NULL,path,MAX_PATH);
  char temporary[MAX_PATH]={0};GetTempPathA(MAX_PATH,temporary);const std::string prefix=std::string(temporary)+"mush-perf-";
  if(_stricmp(path,argv[2]) || _strnicmp(path,prefix.c_str(),prefix.size())){CloseHandle(process);return 4;}
  if(argc==6){
    if(strcmp(argv[5],"--windows-only")){CloseHandle(process);return 2;}
    WindowCounts counts;counts.pid=pid;EnumWindows(CountTop,(LPARAM)&counts);
    for(HWND message=NULL;(message=FindWindowExA(HWND_MESSAGE,message,NULL,NULL))!=NULL;)CountChild(message,(LPARAM)&counts);
    FILE* out=fopen(argv[4],"w");if(!out){CloseHandle(process);return 7;}
    for(const auto& row:counts.classes)fprintf(out,"%u %s\n",row.second,row.first.c_str());
    fclose(out);CloseHandle(process);return 0;
  }
  HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);THREADENTRY32 entry={sizeof(entry)};
  DWORD tid=0;ULONGLONG earliest=~0ULL;
  if(Thread32First(snapshot,&entry))do{if(entry.th32OwnerProcessID==pid){
    HANDLE candidate=OpenThread(THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
    FILETIME created,exited,kernel,user;
    if(candidate && GetThreadTimes(candidate,&created,&exited,&kernel,&user)){
      const ULONGLONG when=((ULONGLONG)created.dwHighDateTime<<32)|created.dwLowDateTime;
      if(when<earliest){earliest=when;tid=entry.th32ThreadID;}
    }if(candidate)CloseHandle(candidate);
  }}while(Thread32Next(snapshot,&entry));CloseHandle(snapshot);
  HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,tid);
  if(!thread){CloseHandle(process);return 5;}
  SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS);
  if(!SymInitialize(process,argv[3],TRUE)){CloseHandle(thread);CloseHandle(process);return 6;}
  std::vector<std::vector<DWORD64> > samples;
  for(int i=0;i<200;++i){
    if(SuspendThread(thread)==(DWORD)-1)break;
    CONTEXT context={0};context.ContextFlags=CONTEXT_FULL;
    std::vector<DWORD64> addresses;
    if(GetThreadContext(thread,&context)){
      STACKFRAME64 frame={0};frame.AddrPC.Offset=context.Eip;frame.AddrPC.Mode=AddrModeFlat;
      frame.AddrFrame.Offset=context.Ebp;frame.AddrFrame.Mode=AddrModeFlat;
      frame.AddrStack.Offset=context.Esp;frame.AddrStack.Mode=AddrModeFlat;
      addresses.push_back(context.Eip);
      for(int j=0;j<24;++j){if(!StackWalk64(IMAGE_FILE_MACHINE_I386,process,thread,&frame,&context,NULL,SymFunctionTableAccess64,SymGetModuleBase64,NULL) || !frame.AddrPC.Offset)break;addresses.push_back(frame.AddrPC.Offset);}
    }
    ResumeThread(thread);samples.push_back(addresses);Sleep(50);
  }
  FILE* out=fopen(argv[4],"w");if(!out)return 7;
  std::map<std::string,unsigned> inclusive;
  for(const auto& sample:samples){std::vector<std::string> names;fprintf(out,"STACK\n");
    for(DWORD64 address:sample){char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]={0};SYMBOL_INFO* symbol=(SYMBOL_INFO*)storage;symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;DWORD64 offset=0;
      const bool resolved=SymFromAddr(process,address,&offset,symbol)!=FALSE;
      IMAGEHLP_MODULE64 module={0};module.SizeOfStruct=sizeof(module);SymGetModuleInfo64(process,address,&module);
      std::string name=module.ModuleName;name+='!';
      if(module.SymType==SymPdb && resolved)name+=symbol->Name;else{char raw[32];sprintf(raw,"0x%llx",address-module.BaseOfImage);name+=raw;}
      fprintf(out,"%s\n",name.c_str());if(std::find(names.begin(),names.end(),name)==names.end())names.push_back(name);
    }for(const auto& name:names)++inclusive[name];
  }
  std::vector<std::pair<unsigned,std::string> > sorted;for(const auto& row:inclusive)sorted.push_back({row.second,row.first});std::sort(sorted.rbegin(),sorted.rend());
  fprintf(out,"INCLUSIVE samples=%u\n",(unsigned)samples.size());for(const auto& row:sorted)fprintf(out,"%u %s\n",row.first,row.second.c_str());
  fclose(out);SymCleanup(process);CloseHandle(thread);CloseHandle(process);return 0;
}
