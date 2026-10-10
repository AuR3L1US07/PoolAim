#include "../src/geometry.hpp"
#include "../src/identity.hpp"
#include <cassert>
#include <iostream>
int main(){
 using namespace pool;
 for(int i=0;i<16;i++){std::string name="ball"+std::to_string(i)+"_abc",encoded=name;for(char& c:encoded)c=(char)((unsigned char)c^0xc6);assert(ballId(decodeName(encoded))==i);assert(ballId(decodeName(name))==i);}
 assert(ballId(decodeName("AimBall_abc"))==-1);assert(ballId("ball16_abc")==-1);assert(ballId("ball12garbage")==-1);assert(ballId("ball-1")==-1);
 assert(std::abs(sphere({0,0,0},{1,0,0},{1,0,0},.1f)-.9f)<1e-5f);
 assert(!std::isfinite(sphere({0,0,0},{1,0,0},{1,0,1},.1f)));
 std::vector<Ball> balls={{{0,.03f,0},0},{{.3f,.03f,0},1}};
 auto hit=trace(balls[0].p,{1,0,0},balls,0,.55f,1.1f,.03f,3);
 assert(hit.hit==1&&hit.hasContact&&hit.lines.size()>=2);
 assert(std::abs(hit.contact.x-.24f)<1e-5f);
 auto bank=trace({0,.03f,.3f},{1,0,.2f},{},0,.55f,1.1f,.03f,2);
 assert(bank.lines.size()==3);
 assert(std::abs(bank.lines[0].b.x-.52f)<1e-5f);
 assert(bank.lines[1].b.x<bank.lines[1].a.x);
 assert(blocked({0,0,0},{.5f,0,0},balls,0,-1,.03f));
 assert(!blocked({0,0,0},{.5f,0,0},balls,0,1,.03f));
 Matrix identity{};for(int i=0;i<4;i++)identity.m[i*5]=1;
 float sx,sy;assert(project({0,0,0},identity,identity,800,600,sx,sy)&&sx==400&&sy==300);
 Matrix behind=identity;behind.m[15]=-1;assert(!project({},identity,behind,800,600,sx,sy));
 auto empty=trace({}, {},balls,0,.55f,1.1f,.03f,2);assert(empty.lines.empty());
 std::vector<Ball> chain={{{-.3f,.03f,.3f},0},{{0,.03f,.3f},1},{{.3f,.03f,.3f},2}};
 auto combo=trace(chain[0].p,{1,0,0},chain,0,.55f,1.1f,.03f,2);
 bool transfer=false,objectBank=false;
 for(const auto& seg:combo.lines){if(seg.kind==1&&seg.a.x>.29f)transfer=true;if(seg.kind==1&&seg.b.x<seg.a.x) objectBank=true;}
 assert(transfer&&objectBank);
 auto cut=trace({-.3f,.03f,0},{1,0,0},{{{-.3f,.03f,0},0},{{0,.03f,.03f},1}},0,.55f,1.1f,.03f,1);
 bool cueAfter=false;for(const auto&seg:cut.lines)if(seg.kind==0&&std::abs(seg.b.z-seg.a.z)>.01f)cueAfter=true;assert(cueAfter);
 auto pot=trace({0,.03f,0},{1,0,0},{},0,.55f,1.1f,.03f,2);assert(pot.potted.size()==1&&pot.potted[0]==0);
 auto simple=essentialLines(combo,0);assert(simple.size()<=6);assert(!simple.empty());
 Trace fragmented;fragmented.hit=1;fragmented.lines={{{0,0,0},{.1f,0,0},0,0},{{.1f,0,0},{.2f,0,0},0,0},{{.2f,0,0},{.3f,0,0},0,0},{{0,0,0},{0,0,.2f},1,1},{{0,0,0},{0,0,.1f},1,2},{{0,0,0},{0,0,.1f},1,3}};
 auto compact=essentialLines(fragmented,0);assert(compact.size()==3);assert(std::abs(compact[0].b.x-.3f)<1e-6f);
 auto second=trace({.2f,.03f,.5f},{-1,0,0},{{{.2f,.03f,.5f},0},{{-.2f,.03f,.5f},2}},0,.55f,1.1f,.03f,1);assert(second.hit==2&&!second.lines.empty());
 auto direct=trace({-.3f,.03f,0},{1,0,0},{{{-.3f,.03f,0},0},{{.2f,.03f,0},7}},0,.55f,1.1f,.03f,0);
 assert(std::find(direct.potted.begin(),direct.potted.end(),7)!=direct.potted.end());
 Vec thinDir=unit({.01f,0,1});Vec thinOrigin=Vec{.24f,.03f,0}-thinDir*.1f;
 auto thin=trace(thinOrigin,thinDir,{{thinOrigin,0},{{.3f,.03f,0},7}},0,.55f,1.1f,.03f,0);
 assert(thin.hit==7&&std::find(thin.potted.begin(),thin.potted.end(),7)!=thin.potted.end());
 assert(thin.thinContact);
 for(double gap: {1e-5,1e-6,1e-7,1e-8}){
  auto inside=collisionTime({-.5,0,.06-gap},{1,0,0},.06);
  auto outside=collisionTime({-.5,0,.06+gap},{1,0,0},.06);
  assert(std::isfinite(inside)&&!std::isfinite(outside));
  double expected=.5-std::sqrt(.06*.06-(.06-gap)*(.06-gap));assert(std::abs(inside-expected)<1e-10);
 }
 assert(!std::isfinite(collisionTime({-.5,0,.06},{1,0,0},.06)));
 assert(!std::isfinite(collisionTime({0,0,0},{1,0,0},.06)));
 auto grazing=trace({-.4f,.03f,0},{1,0,0},{{{-.4f,.03f,0},0},{{0,.03f,.059999f},1}},0,.55f,1.1f,.03f,0);assert(grazing.hasContact&&grazing.thinContact);
 auto nearMiss=trace({-.4f,.03f,0},{1,0,0},{{{-.4f,.03f,0},0},{{0,.03f,.060001f},1}},0,.55f,1.1f,.03f,0);assert(!nearMiss.hasContact);
 float ax,ay,bx,by;
 assert(projectLine({0,0,0},{20,0,0},identity,identity,100,100,ax,ay,bx,by));
 assert(std::abs(ax-50)<.01&&std::abs(bx-100)<.01);
 assert(!projectLine({2,0,0},{20,0,0},identity,identity,100,100,ax,ay,bx,by));
 Matrix perspective{};perspective.m[0]=perspective.m[5]=1;perspective.m[14]=1;
 assert(projectLine({0,0,-1},{.2f,0,1},identity,perspective,100,100,ax,ay,bx,by));
 assert(std::isfinite(ax)&&std::isfinite(bx)&&ax<=100.01&&bx<=100.01);
 assert(!projectLine({0,0,-2},{0,0,-1},identity,perspective,100,100,ax,ay,bx,by));
 std::cout<<"geometry checks passed\n";
}

