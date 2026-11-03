#include <string>
#include <cassert>
#include <iostream>
using String=std::string;
#include "PbpStream.h"
struct Input {std::string text;size_t pos=0;int read(){return pos<text.size()?(unsigned char)text[pos++]:-1;}size_t readBytes(char *p,size_t n){size_t i=0;for(;i<n&&pos<text.size();++i)p[i]=text[pos++];return i;}};
int main(){
 DynamicJsonDocument filter(2048),metadata(4096);filter["id"]=true;filter["awayTeam"]["id"]=true;filter["plays"][0]["eventId"]=true;filter["plays"][0]["typeDescKey"]=true;
 Input input;input.text="{\"plays\":[";for(int i=0;i<3000;i++){if(i)input.text+=",";input.text+="{\"eventId\":"+std::to_string(i+1)+",\"typeDescKey\":\"shot-on-goal\",\"ignored\":{\"long\":\"text with } and , brackets\"}}";}input.text+="],\"unknown\":[{\"a\":[1,2,3]}],\"id\":2026020001,\"awayTeam\":{\"id\":20}}";
 int seen=0;String error;bool ok=readPbp(input,metadata,filter,[&](JsonObject play){assert(play["eventId"].as<int>()==++seen);return true;},error);
 if(!ok)std::cerr<<error<<" at "<<input.pos<<"\n";assert(ok);assert(seen==3000);assert(metadata["id"].as<unsigned>()==2026020001);assert(metadata["awayTeam"]["id"]==20);
 for(auto text:{"{\"plays\":[]}","{\"id\":2026020001,\"plays\":[]}"}){Input empty{text};metadata.clear();assert(readPbp(empty,metadata,filter,[](JsonObject){return false;},error));}
 for(auto text:{"{\"plays\":[{\"eventId\":1}","{\"plays\":[{} {}]}","{\"plays\":null}"}){Input broken{text};metadata.clear();assert(!readPbp(broken,metadata,filter,[](JsonObject){return true;},error));}
 std::cout<<"PASS: 3000 plays, metadata after plays, unknown fields, empty and malformed feeds\n";
}
