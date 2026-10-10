#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
HWND mainWindow,button; HFONT titleFont,bodyFont;std::wstring status=L"先进入游戏练习模式，再加载辅助。";
bool extract(const std::wstring& folder){
 const wchar_t* names[]={L"PoolAim.dll",L"LoadPoolAim.exe",L"PoolAim.ini",L"LICENSE.txt"};
 for(int i=0;i<4;i++){auto res=FindResourceW(nullptr,MAKEINTRESOURCEW(101+i),RT_RCDATA);if(!res)return false;
 auto data=LoadResource(nullptr,res);DWORD size=SizeofResource(nullptr,res);HANDLE file=CreateFileW((folder+L"\\"+names[i]).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 DWORD written=0;bool ok=WriteFile(file,LockResource(data),size,&written,nullptr)&&written==size;CloseHandle(file);if(!ok)return false;
 }return true;
}
DWORD WINAPI launch(void*){
 wchar_t temp[MAX_PATH]{},path[MAX_PATH]{};bool ok=GetTempPathW(MAX_PATH,temp)&&GetTempFileNameW(temp,L"PLA",0,path);std::wstring folder=path;
 if(ok){ok=DeleteFileW(path)&&CreateDirectoryW(path,nullptr)&&extract(folder);}
 DWORD result=1;if(ok){std::wstring cmd=L"\""+folder+L"\\LoadPoolAim.exe\"";std::vector<wchar_t> buf(cmd.begin(),cmd.end());buf.push_back(0);STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(CreateProcessW(nullptr,buf.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&si,&pi)){WaitForSingleObject(pi.hProcess,INFINITE);GetExitCodeProcess(pi.hProcess,&result);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}}
 PostMessageW(mainWindow,WM_APP,result,0);return 0;
}
void label(HDC dc,int x,int y,const wchar_t* text,COLORREF color,HFONT font){SelectObject(dc,font);SetTextColor(dc,color);TextOutW(dc,x,y,text,(int)wcslen(text));}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){case WM_CREATE:
 button=CreateWindowW(L"BUTTON",L"加载辅助",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,32,278,480,48,w,(HMENU)1,nullptr,nullptr);return 0;
 case WM_COMMAND:if(LOWORD(wp)==1){EnableWindow(button,FALSE);status=L"正在加载，请稍候…";InvalidateRect(w,nullptr,FALSE);auto thread=CreateThread(nullptr,0,launch,nullptr,0,nullptr);if(thread)CloseHandle(thread);else PostMessageW(w,WM_APP,1,0);}return 0;
 case WM_APP:EnableWindow(button,TRUE);status=wp==0?L"加载器已完成。请回到游戏查看辅助面板。":L"加载失败：确认只打开一个游戏，并进入练习模式。";InvalidateRect(w,nullptr,FALSE);return 0;
 case WM_DRAWITEM:{auto d=(DRAWITEMSTRUCT*)lp;HBRUSH brush=CreateSolidBrush(IsWindowEnabled(button)?RGB(60,205,174):RGB(55,75,82));FillRect(d->hDC,&d->rcItem,brush);DeleteObject(brush);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,RGB(8,27,32));SelectObject(d->hDC,bodyFont);DrawTextW(d->hDC,L"加载辅助",-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);HBRUSH bg=CreateSolidBrush(RGB(16,24,34));FillRect(dc,&r,bg);DeleteObject(bg);SetBkMode(dc,TRANSPARENT);
 label(dc,32,28,L"PoolAim",RGB(236,244,247),titleFont);label(dc,200,40,L"0.4.6  ·  台球瞄准辅助",RGB(86,218,188),bodyFont);
 label(dc,32,86,status.c_str(),RGB(206,219,229),bodyFont);
 label(dc,32,134,L"青线  白球走位     黄线  目标球路线",RGB(155,177,195),bodyFont);
 label(dc,32,168,L"绿光  几何路线可进，实际结果仍受力度和旋转影响",RGB(116,232,171),bodyFont);
 label(dc,32,210,L"Insert 显示 / 隐藏    F6 反弹    F11 参考线",RGB(155,177,195),bodyFont);
 label(dc,32,244,L"更新或移除辅助：完全退出游戏后重新打开",RGB(155,177,195),bodyFont);EndPaint(w,&ps);return 0;}
 case WM_DESTROY:PostQuitMessage(0);return 0;}return DefWindowProcW(w,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 int count;auto args=CommandLineToArgvW(GetCommandLineW(),&count);if(count==3&&std::wstring(args[1])==L"--extract-only"){bool ok=extract(args[2]);LocalFree(args);return ok?0:1;}LocalFree(args);
 SetProcessDPIAware();titleFont=CreateFontW(-32,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");bodyFont=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
 WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"PoolAimLauncher";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
 mainWindow=CreateWindowW(wc.lpszClassName,L"PoolAim · 瞄准辅助",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,560,386,nullptr,nullptr,instance,nullptr);if(!mainWindow)return 1;ShowWindow(mainWindow,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}DeleteObject(titleFont);DeleteObject(bodyFont);return 0;
}
