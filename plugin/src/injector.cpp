#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#include <vector>
static_assert(sizeof(void*)==4,"The loader must be x86");
int fail(const wchar_t* step){fwprintf(stderr,L"%ls failed (Windows error %lu).\n",step,GetLastError());return 1;}
DWORD findGame(){
 auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return 0;
 PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);DWORD pid=0;int count=0;
 if(Process32FirstW(snapshot,&entry))do{if(!_wcsicmp(entry.szExeFile,L"Poolians.exe")){pid=entry.th32ProcessID;count++;}}while(Process32NextW(snapshot,&entry));
 CloseHandle(snapshot);return count==1?pid:0;
}
uintptr_t remoteModule(DWORD pid,const wchar_t* name){
 auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid);if(snapshot==INVALID_HANDLE_VALUE)return 0;
 MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);uintptr_t base=0;
 if(Module32FirstW(snapshot,&entry))do{if(!_wcsicmp(entry.szModule,name)){base=(uintptr_t)entry.modBaseAddr;break;}}while(Module32NextW(snapshot,&entry));
 CloseHandle(snapshot);return base;
}
int wmain(int argc,wchar_t** argv){
 wchar_t executable[MAX_PATH];GetModuleFileNameW(nullptr,executable,MAX_PATH);std::wstring folder=executable;folder=folder.substr(0,folder.find_last_of(L"\\/"));
 std::wstring dll=argc>1?argv[1]:folder+L"\\PoolAim.dll";wchar_t absolute[32768];
 if(!GetFullPathNameW(dll.c_str(),32768,absolute,nullptr))return fail(L"Resolve DLL path");
 dll=absolute;
 if(GetFileAttributesW(dll.c_str())==INVALID_FILE_ATTRIBUTES){fwprintf(stderr,L"PoolAim.dll not found: %ls\n",dll.c_str());return 1;}
 DWORD pid=findGame();if(!pid){fwprintf(stderr,L"Open exactly one Poolians instance and enter pool practice first.\n");return 1;}
 if(remoteModule(pid,L"PoolAim.dll")){wprintf(L"PoolAim is already loaded. Insert toggles visibility. Restart Poolians to reload a new build.\n");return 0;}
 HANDLE process=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
 if(!process)return fail(L"Open Poolians");
 BOOL wow=FALSE;
 if(!IsWow64Process(process,&wow)){CloseHandle(process);return fail(L"Check target architecture");}
 SYSTEM_INFO sys{};GetNativeSystemInfo(&sys);
 if(sys.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64&&!wow){fwprintf(stderr,L"Target is not x86. No injection performed.\n");CloseHandle(process);return 1;}
 auto load=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");HMODULE owner=nullptr;
 if(!load||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)load,&owner)){CloseHandle(process);return fail(L"Resolve LoadLibraryW");}
 wchar_t ownerPath[MAX_PATH];GetModuleFileNameW(owner,ownerPath,MAX_PATH);const wchar_t* name=wcsrchr(ownerPath,L'\\');name=name?name+1:ownerPath;
 auto remoteBase=remoteModule(pid,name);if(!remoteBase){CloseHandle(process);return fail(L"Resolve target loader module");}
 auto remoteLoad=(LPTHREAD_START_ROUTINE)(remoteBase+((uintptr_t)load-(uintptr_t)owner));
 SIZE_T bytes=(dll.size()+1)*sizeof(wchar_t),written=0;void* remote=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!remote){CloseHandle(process);return fail(L"Allocate DLL path");}
 if(!WriteProcessMemory(process,remote,dll.c_str(),bytes,&written)||written!=bytes){VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return fail(L"Copy DLL path");}
 HANDLE thread=CreateRemoteThread(process,nullptr,0,remoteLoad,remote,0,nullptr);
 if(!thread){VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return fail(L"Load plugin");}
 DWORD wait=WaitForSingleObject(thread,15000),loaded=0;
 if(wait==WAIT_OBJECT_0){GetExitCodeThread(thread,&loaded);VirtualFreeEx(process,remote,0,MEM_RELEASE);}
 CloseHandle(thread);CloseHandle(process);
 if(wait!=WAIT_OBJECT_0){fwprintf(stderr,L"Loader still pending. The DLL path allocation was retained so the game can finish loading.\n");return 1;}
 if(!loaded){fwprintf(stderr,L"LoadLibraryW returned zero. Check DLL architecture and dependencies.\n");return 1;}
 wprintf(L"PoolAim loaded into Poolians (PID %lu).\nINS: show/hide; F6: banks; F8: labels; F9: target; F10: pocket.\nThe plugin does not alter ball positions, angles or power. Restart the game to remove it.\n",pid);
 return 0;
}
