#pragma once
#include <ArduinoJson.h>
template<class StreamType> struct PbpPrefixStream {
    StreamType &source;int prefix;
    PbpPrefixStream(StreamType &s,int first):source(s),prefix(first){}
    int read(){if(prefix>=0){int c=prefix;prefix=-1;return c;}char c;return source.readBytes(&c,1)==1?(unsigned char)c:-1;}
    size_t readBytes(char *buffer,size_t length){size_t n=0;for(;n<length;++n){int c=read();if(c<0)break;buffer[n]=c;}return n;}
};
// Consume one JSON value at a time. The NHL history can grow without growing RAM.
template<class StreamType,class PlayHandler>
bool readPbp(StreamType &stream,JsonDocument &metadata,JsonDocument &filter,PlayHandler play,String &error){
    auto next=[&](){char c;while(stream.readBytes(&c,1)==1){if(c!=' '&&c!='\r'&&c!='\n'&&c!='\t')return int((unsigned char)c);}return -1;};
    auto fail=[&](const char *message){error=message;return false;};
    if(next()!='{')return fail("PBP expected object");
    DynamicJsonDocument key(256),value(2048);
    for(;;){
        auto parsed=deserializeJson(key,stream);if(parsed||!key.is<const char*>())return fail("PBP invalid field name");
        String name=key.as<const char*>();if(next()!=':')return fail("PBP missing colon");
        int savedSeparator=-1;
        if(name=="plays"){
            if(next()!='[')return fail("PBP invalid plays array");
            // peek is used only after a blocking one-byte read; putback is avoided
            // by letting the JSON parser accept a prefix through a tiny adapter.
            int first=next();
            if(first!=']')for(;;){
                if(first!='{')return fail("PBP invalid play object");
                PbpPrefixStream<StreamType> input(stream,'{');
                parsed=deserializeJson(value,input,DeserializationOption::Filter(filter["plays"][0].as<JsonVariantConst>()),DeserializationOption::NestingLimit(14));
                if(parsed){error=String("PBP play JSON: ")+parsed.c_str();return false;}
                if(!play(value.as<JsonObject>()))return false;
                int separator=next();if(separator==']')break;if(separator!=',')return fail("PBP invalid play separator");first=next();
            }
            metadata["playsSeen"]=true;
        }else{
            int first=next();
            if(first<0)return fail("PBP truncated field");
            if(first!='{'&&first!='['&&first!='"'){
                char token[64];size_t length=0;token[length++]=char(first);
                for(;;){char c;if(stream.readBytes(&c,1)!=1)return fail("PBP truncated scalar");
                    if(c==','||c=='}'){savedSeparator=c;break;}
                    if(length>=sizeof(token)-1)return fail("PBP oversized scalar");token[length++]=c;
                }token[length]=0;
                parsed=deserializeJson(value,token,DeserializationOption::Filter(filter[name].as<JsonVariantConst>()));
            }else{
                PbpPrefixStream<StreamType> input(stream,first);
                parsed=deserializeJson(value,input,DeserializationOption::Filter(filter[name].as<JsonVariantConst>()),DeserializationOption::NestingLimit(14));
            }
            if(parsed){error=String("PBP field JSON: ")+parsed.c_str();return false;}
            if(filter[name].as<bool>()||filter[name].is<JsonObject>())metadata[name].set(value.as<JsonVariant>());
            if(metadata.overflowed())return fail("PBP metadata overflow");
        }
        int separator=savedSeparator>=0?savedSeparator:next();if(separator=='}')return true;if(separator!=',')return fail("PBP invalid field separator");
    }
}
