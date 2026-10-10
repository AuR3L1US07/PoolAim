#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <GL/gl.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <array>
#include <algorithm>
#include "geometry.hpp"
#include "identity.hpp"
#include "../vendor/minhook/include/MinHook.h"
static_assert(sizeof(void*)==4,"Poolians needs a 32-bit plugin");
namespace {
using pool::Vec;using pool::Matrix;using pool::Ball;
HMODULE selfModule;std::atomic<bool> stopping{false},rescan{false};
std::atomic<bool> cueAvailable{false};
std::mutex objectsMutex;std::vector<uintptr_t> objects;std::atomic<uintptr_t> ballVtable{0};
bool shown=true,labels=false,references=false;int banks=1,lineWidth=2,cueId=0,targetId=1,pocketId=0;
float halfX=.55f,halfZ=1.10f,radius=.028575f;
Matrix view{},projection{};bool haveView=false,haveProjection=false;HWND gameWindow=nullptr;
HRESULT (WINAPI* originalEndScene)(IDirect3DDevice9*)=nullptr;
HRESULT (WINAPI* originalSetTransform)(IDirect3DDevice9*,D3DTRANSFORMSTATETYPE,const D3DMATRIX*)=nullptr;
bool glBackend=false,glDrawing=false;
template<class T>bool read(uintptr_t,T&);
bool validMatrix(const Matrix&);
BOOL (WINAPI* originalSwapBuffers)(HDC)=nullptr;
using SetMatrixFn=void (__thiscall*)(void*,const Matrix*);
SetMatrixFn originalGLView=nullptr,originalGLProjection=nullptr;
void __fastcall hookGLView(void* object,void*,const Matrix* input){
 Matrix copy{};
 if(read((uintptr_t)input,copy)&&validMatrix(copy)&&(std::abs(copy.m[3])+std::abs(copy.m[7])+std::abs(copy.m[11])>.001f)){view=copy;haveView=true;}
 originalGLView(object,input);
}
void __fastcall hookGLProjection(void* object,void*,const Matrix* input){
 Matrix copy{};
 if(read((uintptr_t)input,copy)&&validMatrix(copy)&&std::abs(copy.m[14])>.1f&&std::abs(copy.m[15])<.01f){projection=copy;haveProjection=true;}
 originalGLProjection(object,input);
}
using PositionFn=const Vec* (__thiscall*)(void*);
using NameFn=const void* (__thiscall*)(void*);
PositionFn getNodePosition=nullptr;NameFn getEntityName=nullptr;
FILE* logFile=nullptr;std::mutex logMutex;
void log(const char* msg){std::lock_guard<std::mutex> l(logMutex);if(logFile){fprintf(logFile,"%lu %s\n",GetTickCount(),msg);fflush(logFile);}}
bool read(uintptr_t address,void* output,size_t size){SIZE_T count=0;return address>0x10000&&ReadProcessMemory(GetCurrentProcess(),(void*)address,output,size,&count)&&count==size;}
template<class T>bool read(uintptr_t address,T& output){return read(address,&output,sizeof(output));}
bool validMatrix(const Matrix& m){for(float f:m.m)if(!std::isfinite(f)||std::abs(f)>1e5f)return false;return true;}
HRESULT WINAPI hookSetTransform(IDirect3DDevice9* device,D3DTRANSFORMSTATETYPE state,const D3DMATRIX* input){
 D3DMATRIX value{};Matrix copy{};
 if(input&&read((uintptr_t)input,value)){
  for(int row=0;row<4;row++)for(int col=0;col<4;col++)copy.m[row*4+col]=value.m[col][row];
  if(validMatrix(copy)){
   if(state==D3DTS_VIEW && (std::abs(copy.m[3])+std::abs(copy.m[7])+std::abs(copy.m[11])>.001f)){view=copy;haveView=true;}
   if(state==D3DTS_PROJECTION && std::abs(copy.m[14])>.1f && std::abs(copy.m[15])<.01f){projection=copy;haveProjection=true;}
  }
 }
 return originalSetTransform(device,state,input);
}
uintptr_t findBallVtable(HMODULE module){
 if(!module)return 0;
 auto base=(uintptr_t)module;IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS nt{};
 if(!read(base,dos)||!read(base+dos.e_lfanew,nt))return 0;
 std::vector<unsigned char> data(nt.OptionalHeader.SizeOfImage);
 // Sections are copied individually: not every image page must be readable.
 for(size_t offset=0;offset<data.size();offset+=4096)read(base+offset,data.data()+offset,std::min<size_t>(4096,data.size()-offset));
 const char type[]=".?AVCBall@@";uintptr_t descriptor=0;
 for(size_t i=8;i+sizeof(type)<data.size();i++)if(!memcmp(data.data()+i,type,sizeof(type))){descriptor=base+i-8;break;}
 if(!descriptor)return 0;
 auto word=[&](size_t i){uint32_t v;memcpy(&v,data.data()+i,4);return v;};
 for(size_t i=12;i+8<data.size();i+=4)if(word(i)==descriptor&&word(i-12)==0&&word(i-8)==0){
  uintptr_t col=base+i-12;
  for(size_t j=0;j+8<data.size();j+=4)if(word(j)==col&&word(j+4)>=base&&word(j+4)<base+data.size())return base+j+4;
 }
 return 0;
}
std::vector<uintptr_t> scanBalls(uintptr_t vtable){
 std::vector<uintptr_t> result;uintptr_t cursor=0;MEMORY_BASIC_INFORMATION info{};std::vector<unsigned char> chunk(1024*1024);
 while(cursor<0x7fff0000&&!stopping){
  if(!VirtualQuery((void*)cursor,&info,sizeof(info)))break;
  uintptr_t start=(uintptr_t)info.BaseAddress,next=start+info.RegionSize;if(next<=cursor)break;cursor=next;
  if(info.State!=MEM_COMMIT||info.Type!=MEM_PRIVATE||(info.Protect&PAGE_GUARD)||!(info.Protect&0xee))continue;
  for(size_t offset=0;offset<info.RegionSize;offset+=chunk.size()){
   if(stopping)break;
   Sleep(1);
   size_t size=std::min<size_t>(chunk.size(),info.RegionSize-offset);if(!read(start+offset,chunk.data(),size))continue;
   for(size_t i=0;i+4<=size;i+=4){uint32_t v;memcpy(&v,chunk.data()+i,4);if(v==vtable)result.push_back(start+offset+i);}
  }
 }
 if(result.size()>64)result.resize(64);
 return result;
}
std::string msvcString(const void* string){
 // MSVC 2010 basic_string: inline buffer/pointer, size, capacity.
 unsigned char data[24]{};if(!read((uintptr_t)string,data,24))return {};
 uint32_t size,capacity,pointer;memcpy(&size,data+16,4);memcpy(&capacity,data+20,4);memcpy(&pointer,data,4);
 if(size>96||size>capacity)return {};
 std::string out(size,'\0');
 if(capacity<16){if(size>15)return {};memcpy(out.data(),data,size);}else if(!read(pointer,out.data(),size))return {};
 return out;
}
std::vector<Ball> liveBalls(){
 std::vector<uintptr_t> list;{std::lock_guard<std::mutex> lock(objectsMutex);list=objects;}
 std::vector<Ball> result;
 for(auto object:list){
  uint32_t vt,node,entity;float r;
  if(!read(object,vt)||vt!=ballVtable||!read(object+0x28,node)||!read(object+0x30,entity)||!read(object+0x40,r)||std::abs(r-radius)>.001f)continue;
  uint32_t test;if(!read(node,test)||!read(entity,test))continue;
  auto name=pool::decodeName(msvcString(getEntityName((void*)entity)));
  int id=pool::ballId(name);
  if(id<0||id>15)continue;
  Vec p;if(!read((uintptr_t)getNodePosition((void*)node),p)||!pool::finite(p)||std::abs(p.x)>2||std::abs(p.z)>3)continue;
  if(p.y<radius*.25f||p.y>radius*2.5f)continue;
  if(std::find_if(result.begin(),result.end(),[&](const Ball& b){return b.id==id;})==result.end())result.push_back({p,id});
 }
 std::sort(result.begin(),result.end(),[](const Ball&a,const Ball&b){return a.id<b.id;});return result;
}
std::vector<Ball> sampleBalls(){
 static DWORD tick=0,missingSince=0;static std::vector<Ball> cache;
 DWORD now=GetTickCount();if(now-tick>=33||tick==0){cache=liveBalls();tick=now;}
 bool hasCue=std::any_of(cache.begin(),cache.end(),[](const Ball&b){return b.id==cueId;});
 cueAvailable.store(hasCue);
 if(hasCue)missingSince=0;else if(!missingSince)missingSince=now;
 else if(now-missingSince>1500){rescan=true;missingSince=now;}
 return cache;
}
struct Vertex {float x,y,z,rhw;DWORD color;};
IDirect3DDevice9* drawing=nullptr;D3DVIEWPORT9 viewport{};
void drawVertices(Vertex* v){
 if(glDrawing){for(int i: {0,1,2,2,1,3}){auto c=v[i].color;glColor4ub((c>>16)&255,(c>>8)&255,c&255,(c>>24)&255);glVertex2f(v[i].x,v[i].y);}}
 else drawing->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(Vertex));
}
void screenLine(float ax,float ay,float bx,float by,DWORD color,int thickness=1){
 float dx=bx-ax,dy=by-ay,len=std::sqrt(dx*dx+dy*dy);if(len<.1f)return;
 float px=-dy/len*thickness*.5f,py=dx/len*thickness*.5f;
 Vertex v[4]={{ax+px,ay+py,0,1,color},{ax-px,ay-py,0,1,color},{bx+px,by+py,0,1,color},{bx-px,by-py,0,1,color}};
 drawVertices(v);
}
bool project(Vec p,float& x,float& y){if(!pool::project(p,view,projection,(float)viewport.Width,(float)viewport.Height,x,y))return false;x+=viewport.X;y+=viewport.Y;return true;}
void worldLine(Vec a,Vec b,DWORD c,int width){float ax,ay,bx,by;if(project(a,ax,ay)&&project(b,bx,by))screenLine(ax,ay,bx,by,c,width);}
void ring(Vec p,float r,DWORD c){for(int i=0;i<48;i++){float a=i*6.2831853f/48,b=(i+1)*6.2831853f/48;worldLine(p+Vec{std::cos(a)*r,0,std::sin(a)*r},p+Vec{std::cos(b)*r,0,std::sin(b)*r},c,lineWidth);}}
// Small bitmap font: no D3DX or font resource lost-device dependencies.
const char* glyph(char c){
 switch(c){
 case '0':return "111101101101111";case '1':return "010110010010111";case '2':return "111001111100111";case '3':return "111001111001111";case '4':return "101101111001001";case '5':return "111100111001111";case '6':return "111100111101111";case '7':return "111001010010010";case '8':return "111101111101111";case '9':return "111101111001111";
 default:return "000000000000000";
 }
}
void text(float x,float y,const char* message,DWORD color,int scale=2){float initial=x;for(;*message;message++){if(*message=='\n'){y+=7*scale;x=initial;continue;}const char* bits=glyph(*message);for(int j=0;j<5;j++)for(int i=0;i<3;i++)if(bits[j*3+i]=='1')screenLine(x+i*scale,y+j*scale,x+(i+1)*scale,y+j*scale,color,scale);x+=4*scale;}}
#include "panel.hpp"
bool key(int code){return (GetAsyncKeyState(code)&1)!=0;}
void controls(){
 if(GetForegroundWindow()!=gameWindow)return;
 if(key(VK_INSERT))shown=!shown;
 if(key(VK_F6))banks=(banks+1)%4;
 if(key(VK_F7)){cueId=(cueId+1)%16;}
 if(key(VK_F8))labels=!labels;
 if(key(VK_F9))targetId=targetId%15+1;
 if(key(VK_F10))pocketId=(pocketId+1)%6;
 if(key(VK_F11))references=!references;
 if(key(VK_OEM_PLUS))lineWidth=std::min(lineWidth+1,6);
 if(key(VK_OEM_MINUS))lineWidth=std::max(lineWidth-1,1);
 if(key(VK_F5))rescan=true;
}
void renderContent(const std::vector<Ball>& balls){
 std::string prediction="NO POCKET ON PREVIEW";
 auto cue=std::find_if(balls.begin(),balls.end(),[](const Ball&b){return b.id==cueId;});
 if(haveView&&haveProjection&&cue!=balls.end()){
  float sign=glBackend?-1.0f:1.0f; Vec direction=pool::unit({sign*view.m[8],0,sign*view.m[10]});static pool::Trace path;static DWORD traceTick=0;static int lastBanks=-1,lastCue=-1;static Vec lastDirection{};
  DWORD now=GetTickCount();if(!traceTick||now-traceTick>=50||(now-traceTick>=16&&pool::dot(direction-lastDirection,direction-lastDirection)>1e-10f)||banks!=lastBanks||cueId!=lastCue){path=pool::trace(cue->p,direction,balls,cueId,halfX,halfZ,radius,banks);traceTick=now;lastBanks=banks;lastCue=cueId;lastDirection=direction;}
  if(path.lines.empty())prediction="CHECK TABLE BOUNDS / F5 RESCAN";
  for(int id:path.potted){if(id==cueId)prediction="CUE SCRATCH RISK";else if(prediction!="CUE SCRATCH RISK")prediction="POSSIBLE POT BALL "+std::to_string(id);}
  for(auto&s:pool::essentialLines(path,cueId)){
   bool targetPot=!path.thinContact&&path.hit>=0&&std::find(path.potted.begin(),path.potted.end(),path.hit)!=path.potted.end();
   bool incoming=s.id==cueId&&path.hasContact&&pool::dot(s.b-path.contact,s.b-path.contact)<1e-6f;
   bool pot=!path.thinContact&&((s.id!=cueId&&std::find(path.potted.begin(),path.potted.end(),s.id)!=path.potted.end())||(incoming&&targetPot));
   if(pot){worldLine(s.a,s.b,0x3078ffab,lineWidth+7);worldLine(s.a,s.b,0x6078ffab,lineWidth+3);}
   worldLine(s.a,s.b,pot?0xff78ffab:path.thinContact&&s.kind?0xffffb454:s.kind?0xffffcf66:0xff63e7f2,lineWidth);
  }
  for(int id:path.potted)if(id!=cueId&&!path.thinContact){auto ball=std::find_if(balls.begin(),balls.end(),[&](const Ball&b){return b.id==id;});if(ball!=balls.end())ring(ball->p,radius*1.25f,0xff78ffab);auto last=std::find_if(path.lines.rbegin(),path.lines.rend(),[&](const pool::Segment&s){return s.id==id;});if(last!=path.lines.rend())ring(last->b,radius*1.8f,0xff78ffab);}
  if(path.thinContact)prediction="THIN CUT - SENSITIVE";
  if(path.hasContact)ring(path.contact,radius,path.thinContact?0xffffb454:0xff76ffc1);
  ring(cue->p,radius*1.1f,0xfff0f6ff);
  auto target=std::find_if(balls.begin(),balls.end(),[](const Ball&b){return b.id==targetId;});
  if(references&&target!=balls.end()){
   Vec pockets[6]={{-halfX, radius,halfZ},{-halfX,radius,0},{-halfX,radius,-halfZ},{halfX,radius,-halfZ},{halfX,radius,0},{halfX,radius,halfZ}};
   Vec pocket=pockets[pocketId],ghost=target->p-pool::unit(pocket-target->p)*(2*radius);
   bool clear=!pool::blocked(cue->p,ghost,balls,cueId,targetId,radius)&&!pool::blocked(target->p,pocket,balls,cueId,targetId,radius);
   if(pool::dot(pool::unit(ghost-cue->p),pool::unit(pocket-target->p))>0){DWORD color=clear?0xaa7eff9a:0xaaff776b;worldLine(cue->p,ghost,color,1);worldLine(target->p,pocket,color,1);ring(ghost,radius,color);}
  }
 }
 if(haveView&&haveProjection&&labels)for(auto&ball:balls){float x,y;if(project(ball.p+Vec{0,radius,0},x,y)){char id[8];snprintf(id,sizeof(id),"%d",ball.id);text(x+6,y-10,id,ball.id==cueId?0xff8cfff0:0xfff0f6ff);}}
 std::wstring status=L"未预测到进袋路线";
 if(!haveView||!haveProjection)status=L"正在读取相机…";
 else if(cue==balls.end())status=L"正在识别白球…";
 else if(prediction=="CHECK TABLE BOUNDS / F5 RESCAN")status=L"请校准桌面尺寸，或按 F5 重新识别";
 else if(prediction=="THIN CUT - SENSITIVE")status=L"极薄碰撞：橙色路线对误差敏感";
 else if(prediction=="CUE SCRATCH RISK")status=L"注意：白球可能落袋";
 else if(prediction.find("POSSIBLE POT BALL ")==0)status=L"几何路线可进："+std::wstring(prediction.begin()+18,prediction.end())+L" 号球（非保证）";
 std::wstring info=L"PoolAim 0.4.6 · 台球瞄准辅助\n球数 "+std::to_wstring(balls.size())+L"   白球 "+std::to_wstring(cueId)+L"   反弹 "+std::to_wstring(banks)+L"\nInsert 显隐    F5 重新识别\nF6 反弹    F7 白球    F8 编号\nF11 参考线    + / - 线宽\n"+status;
 chinesePanel(info);
}
HRESULT WINAPI hookEndScene(IDirect3DDevice9* device){
 if(stopping)return originalEndScene(device);
 D3DDEVICE_CREATION_PARAMETERS parameters{};if(SUCCEEDED(device->GetCreationParameters(&parameters)))gameWindow=parameters.hFocusWindow;
 controls();auto balls=sampleBalls();if(!shown)return originalEndScene(device);
 IDirect3DStateBlock9* state=nullptr;if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return originalEndScene(device);
 if(FAILED(state->Capture())){state->Release();return originalEndScene(device);}
 drawing=device;device->GetViewport(&viewport);
 device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
 device->SetTexture(0,nullptr);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
 device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
 device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
 device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
 renderContent(balls);
 state->Apply();state->Release();drawing=nullptr;
 return originalEndScene(device);
}
BOOL WINAPI hookSwapBuffers(HDC dc){
 if(stopping||!wglGetCurrentContext())return originalSwapBuffers(dc);
 gameWindow=GetAncestor(WindowFromDC(dc),GA_ROOT);controls();auto balls=sampleBalls();if(!shown)return originalSwapBuffers(dc);
 GLint vp[4],previousMode;glGetIntegerv(GL_VIEWPORT,vp);glGetIntegerv(GL_MATRIX_MODE,&previousMode);
 if(vp[2]<=0||vp[3]<=0)return originalSwapBuffers(dc);
 viewport.X=vp[0];viewport.Y=vp[1];viewport.Width=vp[2];viewport.Height=vp[3];
 glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
 using ActiveTextureFn=void (APIENTRY*)(GLenum);using UseProgramFn=void (APIENTRY*)(GLuint);
 static ActiveTextureFn activeTexture=nullptr;static UseProgramFn useProgram=nullptr;static bool extensions=false;
 if(!extensions){auto a=wglGetProcAddress("glActiveTexture");auto u=wglGetProcAddress("glUseProgram");if((uintptr_t)a>3&&(uintptr_t)a!=0xffffffff)memcpy(&activeTexture,&a,4);if((uintptr_t)u>3&&(uintptr_t)u!=0xffffffff)memcpy(&useProgram,&u,4);extensions=true;}
 GLint oldProgram=0;if(useProgram){glGetIntegerv(0x8B8D,&oldProgram);useProgram(0);}
 if(activeTexture){GLint units=1;glGetIntegerv(0x84E2,&units);for(int i=0;i<std::min(units,16);i++){activeTexture(0x84C0+i);glDisable(GL_TEXTURE_2D);glDisable(GL_TEXTURE_1D);}activeTexture(0x84C0);}else glDisable(GL_TEXTURE_2D);
 glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_FOG);glDisable(GL_COLOR_LOGIC_OP);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(vp[0],vp[0]+vp[2],vp[1]+vp[3],vp[1],-1,1);
 glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glDrawing=true;glBegin(GL_TRIANGLES);renderContent(balls);glEnd();glDrawing=false;
 glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(previousMode);
 if(useProgram)useProgram(oldProgram);
 glPopClientAttrib();glPopAttrib();
 return originalSwapBuffers(dc);
}
std::array<void*,2> deviceAddresses(){
 auto d3d=Direct3DCreate9(D3D_SDK_VERSION);if(!d3d)return {};
 HWND window=CreateWindowW(L"STATIC",L"PoolAimDevice",WS_OVERLAPPED,0,0,100,100,nullptr,nullptr,selfModule,nullptr);
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
 IDirect3DDevice9* device=nullptr;std::array<void*,2> functions{};
 if(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device))){functions={(*(void***)device)[42],(*(void***)device)[44]};device->Release();}
 DestroyWindow(window);d3d->Release();return functions;
}
DWORD WINAPI worker(void*){
 wchar_t dllPath[MAX_PATH];GetModuleFileNameW(selfModule,dllPath,MAX_PATH);std::wstring folder=dllPath;folder=folder.substr(0,folder.find_last_of(L"\\/"));
 logFile=_wfopen((folder+L"\\PoolAim.log").c_str(),L"w");log("plugin loaded (x86); gameplay memory read only");
 auto ini=folder+L"\\PoolAim.ini";wchar_t value[40];GetPrivateProfileStringW(L"Table",L"HalfX",L"0.55",value,40,ini.c_str());halfX=(float)_wtof(value);GetPrivateProfileStringW(L"Table",L"HalfZ",L"1.10",value,40,ini.c_str());halfZ=(float)_wtof(value);
 if(halfX<.3f||halfX>1||halfZ<.6f||halfZ>2){halfX=.55f;halfZ=1.1f;}
 HMODULE ogre=nullptr;while(!stopping&&!(ogre=GetModuleHandleW(L"OgreMain.dll")))Sleep(250);
 if(!ogre)return 0;
 auto positionExport=GetProcAddress(ogre,"?_getDerivedPosition@Node@Ogre@@UBEABVVector3@2@XZ");memcpy(&getNodePosition,&positionExport,sizeof(getNodePosition));
 auto nameExport=GetProcAddress(ogre,"?getName@MovableObject@Ogre@@UBEABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ");memcpy(&getEntityName,&nameExport,sizeof(getEntityName));
 if(!getNodePosition||!getEntityName){log("unsupported Ogre exports; plugin disabled");return 0;}
 if(MH_Initialize()!=MH_OK){log("MinHook initialization failed");return 0;}
 auto glModule=GetModuleHandleW(L"RenderSystem_GL.dll");bool hooksCreated=false;
 if(glModule){
  glBackend=true;
  auto swap=GetProcAddress(GetModuleHandleW(L"gdi32.dll"),"SwapBuffers");
  auto setView=GetProcAddress(glModule,"?_setViewMatrix@GLRenderSystem@Ogre@@UAEXABVMatrix4@2@@Z");
  auto setProjection=GetProcAddress(glModule,"?_setProjectionMatrix@GLRenderSystem@Ogre@@UAEXABVMatrix4@2@@Z");
  hooksCreated=swap&&setView&&setProjection&&MH_CreateHook((void*)swap,(void*)hookSwapBuffers,(void**)&originalSwapBuffers)==MH_OK&&MH_CreateHook((void*)setView,(void*)hookGLView,(void**)&originalGLView)==MH_OK&&MH_CreateHook((void*)setProjection,(void*)hookGLProjection,(void**)&originalGLProjection)==MH_OK;
 }else{
  auto functions=deviceAddresses();hooksCreated=functions[0]&&functions[1]&&MH_CreateHook(functions[0],(void*)hookEndScene,(void**)&originalEndScene)==MH_OK&&MH_CreateHook(functions[1],(void*)hookSetTransform,(void**)&originalSetTransform)==MH_OK;
 }
 if(!hooksCreated){log("hook creation failed; plugin disabled");MH_Uninitialize();return 0;}
 if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){log("hook enable failed");MH_Uninitialize();return 0;}log(glBackend?"OpenGL SwapBuffers and Ogre matrix hooks enabled":"DirectX EndScene and SetTransform hooks enabled");
 HMODULE previousModule=nullptr;DWORD lastScan=0;
 while(!stopping){
  HMODULE module=GetModuleHandleW(L"game.dll");
  if(module!=previousModule){previousModule=module;ballVtable=findBallVtable(module);log(ballVtable?"CBall RTTI resolved":"waiting for pool game module");rescan=true;}
  // Recovery must not depend on a request from the render callback: loading the
  // DLL in the lobby can yield only AimBall objects before the rack exists.
  bool recoveryDue=!cueAvailable.load()&&(lastScan==0||GetTickCount()-lastScan>10000);
  bool refreshDue=GetTickCount()-lastScan>60000;
  if(ballVtable&&(recoveryDue||refreshDue||(rescan.load()&&(lastScan==0||GetTickCount()-lastScan>10000)))){
   rescan=false;
   auto found=scanBalls(ballVtable);{std::lock_guard<std::mutex> lock(objectsMutex);objects=found;}
   char msg[80];snprintf(msg,sizeof(msg),"scan: %u CBall candidates",(unsigned)found.size());log(msg);lastScan=GetTickCount();
  }
  Sleep(100);
 }
 return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){selfModule=instance;DisableThreadLibraryCalls(instance);HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
 // Do not unpatch/unload from DllMain. The DLL remains resident until game exit.
 if(reason==DLL_PROCESS_DETACH)stopping=true;
 return TRUE;
}
