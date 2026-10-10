#pragma once
#include <string>
namespace pool {
inline int ballId(const std::string& name){
 if(name.compare(0,4,"ball")!=0||name.size()<5||name[4]<'0'||name[4]>'9')return -1;
 int n=0;size_t i=4;
 for(;i<name.size()&&name[i]>='0'&&name[i]<='9';i++){n=n*10+name[i]-'0';if(n>15)return -1;}
 if(i<name.size()&&name[i]!='_')return -1;
 return n;
}
inline std::string decodeName(std::string name){
 if(ballId(name)>=0||name.compare(0,7,"AimBall")==0)return name;
 for(char& c:name)c=(char)((unsigned char)c^0xc6);
 if(ballId(name)>=0||name.compare(0,7,"AimBall")==0)return name;
 return {};
}
}
