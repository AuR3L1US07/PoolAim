#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
namespace pool {
struct Vec { float x=0,y=0,z=0; Vec operator+(Vec b)const{return{x+b.x,y+b.y,z+b.z};} Vec operator-(Vec b)const{return{x-b.x,y-b.y,z-b.z};} Vec operator*(float s)const{return{x*s,y*s,z*s};} };
inline float dot(Vec a,Vec b){return a.x*b.x+a.z*b.z;}
inline Vec unit(Vec a){float n=std::sqrt(dot(a,a));return n>1e-7f?a*(1/n):Vec{};}
inline bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
struct Ball {Vec p; int id=-1;};
struct Segment {Vec a,b; int kind=0; int id=-1;};
struct Trace {std::vector<Segment> lines; Vec contact{}; int hit=-1; bool hasContact=false; std::vector<int> potted; bool thinContact=false;};
// Keep the memory-facing Vec layout unchanged; simulate with double precision.
struct PreciseVec {
 double x=0,y=0,z=0;
 PreciseVec()=default;PreciseVec(Vec p):x(p.x),y(p.y),z(p.z){}
 PreciseVec(double a,double b,double c):x(a),y(b),z(c){}
 operator Vec()const{return{(float)x,(float)y,(float)z};}
 PreciseVec operator+(PreciseVec b)const{return{x+b.x,y+b.y,z+b.z};}
 PreciseVec operator-(PreciseVec b)const{return{x-b.x,y-b.y,z-b.z};}
 PreciseVec operator*(double v)const{return{x*v,y*v,z*v};}
};
inline double preciseDot(PreciseVec a,PreciseVec b){return a.x*b.x+a.z*b.z;}
inline PreciseVec preciseUnit(PreciseVec a){double n=std::sqrt(preciseDot(a,a));return n>1e-14?a*(1/n):PreciseVec{};}
inline double collisionTime(PreciseVec rel,PreciseVec velocity,double radius){
 double vv=preciseDot(velocity,velocity),rv=preciseDot(rel,velocity);
 if(vv<1e-24||rv>=-1e-14)return INFINITY;
 double cross=rel.x*velocity.z-rel.z*velocity.x;
 double disc=vv*radius*radius-cross*cross;
 // A true miss stays a miss. Exact tangency transfers no normal impulse.
 if(disc<=0)return INFINITY;
 double c=preciseDot(rel,rel)-radius*radius;
 double time=c/(-rv+std::sqrt(disc));
 if(time<0){if(c < -2*radius*1e-8)return INFINITY;return 0;}
 return time;
}
inline float sphere(Vec origin,Vec direction,Vec center,float radius){
 return (float)collisionTime(PreciseVec(origin)-PreciseVec(center),PreciseVec(direction),radius);
}
inline bool blocked(Vec a,Vec b,const std::vector<Ball>& balls,int skipA,int skipB,float r){
 Vec v=b-a;float len=std::sqrt(dot(v,v));if(len<1e-6f)return true;
 for(auto& ball:balls)if(ball.id!=skipA&&ball.id!=skipB&&sphere(a,v*(1/len),ball.p,2*r)<len-1e-5f)return true;
 return false;
}
// Event-driven equal-mass, no-spin geometric preview. All balls move together.
inline Trace trace(Vec origin,Vec direction,const std::vector<Ball>& balls,int cue,float halfX,float halfZ,float r,int banks){
 Trace result; Vec d=unit(direction); float x=halfX-r,z=halfZ-r;
 if(dot(d,d)<.5f||x<=0||z<=0||std::abs(origin.x)>x+.01f||std::abs(origin.z)>z+.01f)return result;
 struct State {PreciseVec p,v;int id,bounces=0;bool active=true;};
 std::vector<State> states;
 for(const auto& b:balls)states.push_back({b.id==cue?origin:b.p,b.id==cue?d:Vec{},b.id});
 if(std::none_of(states.begin(),states.end(),[&](const State&s){return s.id==cue;}))states.push_back({origin,d,cue});
 // Bound work by collision count, not synthetic travel time: thin cuts
 // transfer little speed but can still have a clear geometric pocket path.
 for(int event=0;event<96;event++){
  double dt=INFINITY;int a=-1,b=-1,axis=-1;
  for(size_t i=0;i<states.size();i++){
   auto&s=states[i];if(!s.active)continue;
   for(int ax=0;ax<2;ax++){
    double v=ax?s.v.z:s.v.x,p=ax?s.p.z:s.p.x,bound=ax?z:x;
    if(std::abs(v)<1e-12)continue;
    double t=((v>0?bound:-bound)-p)/v;
    if(t>=-1e-5f&&t<dt){dt=std::max(0.0,t);a=(int)i;b=-1;axis=ax;}
   }
   for(size_t j=i+1;j<states.size();j++){
    auto&t=states[j];if(!t.active)continue;
    double time=collisionTime(s.p-t.p,s.v-t.v,2.0*r);
    if(time<dt){dt=time;a=(int)i;b=(int)j;axis=-1;}
   }
  }
  if(a<0||!std::isfinite(dt))break;
  for(auto&s:states)if(s.active){PreciseVec end=s.p+s.v*dt;if(preciseDot(end-s.p,end-s.p)>1e-14)result.lines.push_back({s.p,end,s.id==cue?0:1,s.id});s.p=end;}
  auto&s=states[a];
  if(b>=0){
   auto&t=states[b];PreciseVec normal=preciseUnit(t.p-s.p);double impulse=preciseDot(s.v-t.v,normal);
   if(impulse<=1e-14)break;
   if(!result.hasContact&&(s.id==cue||t.id==cue)){double relativeSpeed=std::sqrt(preciseDot(s.v-t.v,s.v-t.v));result.thinContact=relativeSpeed>0&&impulse/relativeSpeed<.12;result.hasContact=true;result.hit=s.id==cue?t.id:s.id;result.contact=s.id==cue?s.p:t.p;}
   s.v=s.v-normal*impulse;t.v=t.v+normal*impulse;
   // Separate by a tiny symmetric epsilon to avoid repeating a contact.
   s.p=s.p-normal*1e-9;t.p=t.p+normal*1e-9;
  }else{
   // Pocket mouths are approximate until the table geometry is calibrated.
   bool pocket=axis==0 ? (std::abs(s.p.z)<2*r||std::abs(s.p.z)>z-2*r) : std::abs(s.p.x)>x-2*r;
   if(pocket){result.potted.push_back(s.id);s.active=false;s.v={};continue;}
   if(s.bounces++>=banks){s.active=false;s.v={};continue;}
   if(axis==0){s.v.x=-s.v.x;s.p.x+=(s.v.x>0?1:-1)*1e-9;}else{s.v.z=-s.v.z;s.p.z+=(s.v.z>0?1:-1)*1e-9;}
  }
 }
 return result;
}
// Keep cue, first target and one combination ball; merge event subdivisions.
inline std::vector<Segment> essentialLines(const Trace& path,int cue,int maxTurns=2){
 std::vector<Segment> out; std::vector<int> ids{cue};
 if(path.hit>=0)ids.push_back(path.hit);
 for(const auto& s:path.lines)if(s.kind && s.id!=path.hit && ids.size()<3 && std::find(ids.begin(),ids.end(),s.id)==ids.end())ids.push_back(s.id);
 for(int id:ids){int count=0;for(const auto&s:path.lines){
  if(s.id!=id||dot(s.b-s.a,s.b-s.a)<1e-8f)continue;
  if(!out.empty()&&out.back().id==id&&dot(out.back().b-s.a,out.back().b-s.a)<1e-7f&&dot(unit(out.back().b-out.back().a),unit(s.b-s.a))>.9999f){out.back().b=s.b;continue;}
  if(count>=maxTurns)break;
  out.push_back(s);count++;
 }}return out;
}
struct Matrix {float m[16];};
inline bool project(Vec p,const Matrix& view,const Matrix& projection,float width,float height,float& x,float& y){
 float a[4]={p.x,p.y,p.z,1},b[4]={},c[4]={};
 for(int row=0;row<4;row++)for(int col=0;col<4;col++)b[row]+=view.m[row*4+col]*a[col];
 for(int row=0;row<4;row++)for(int col=0;col<4;col++)c[row]+=projection.m[row*4+col]*b[col];
 if(!std::isfinite(c[3])||c[3]<=.001f)return false;
 float nx=c[0]/c[3],ny=c[1]/c[3];if(!std::isfinite(nx)||!std::isfinite(ny))return false;
 x=(nx+1)*width/2;y=(1-ny)*height/2;return std::abs(nx)<10&&std::abs(ny)<10;
}
}
