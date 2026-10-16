// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_JSON_HPP
#define HEADER_SCINTILLA_JSON_HPP
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Shared strict bounded parser. Escapes are decoded before duplicate-key checks;
// numeric token text is retained for exact integer normalization. No I/O.
namespace ScintillaJSON
{
struct Invalid:std::runtime_error { explicit Invalid(const char* s):std::runtime_error(s){} };
struct J
{
    enum Kind { Null, Bool, Number, String, Array, Object } kind=Null;
    bool boolean=false;
    double number=0;
    std::string string, numberText;
    std::vector<J> array;
    std::map<std::string,J> object;
};
// Independent bounded parser because the legacy command parser accepts duplicate
// keys and unbounded nesting. Decode escapes BEFORE checking object-key uniqueness.
class Parser
{
    const std::string& s;
    size_t p=0;
    void ws() { while(p<s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) ++p; }
    char get() { if(p>=s.size()) throw Invalid("json_truncated"); return s[p++]; }
    bool eat(char c) { ws(); if(p<s.size()&&s[p]==c) {++p;return true;}return false; }
    unsigned hex()
    {
        unsigned n=0;
        for(int i=0;i<4;++i) { char c=get(); n*=16;
            if(c>='0'&&c<='9') n+=c-'0'; else if(c>='a'&&c<='f')n+=c-'a'+10;
            else if(c>='A'&&c<='F')n+=c-'A'+10; else throw Invalid("json_unicode"); }
        return n;
    }
    static void utf8(std::string& v,unsigned n)
    {
        if(n<0x80)v+=char(n);
        else if(n<0x800) {v+=char(0xc0|(n>>6));v+=char(0x80|(n&63));}
        else if(n<0x10000) {v+=char(0xe0|(n>>12));v+=char(0x80|((n>>6)&63));v+=char(0x80|(n&63));}
        else {v+=char(0xf0|(n>>18));v+=char(0x80|((n>>12)&63));v+=char(0x80|((n>>6)&63));v+=char(0x80|(n&63));}
    }
    std::string string()
    {
        if(get()!='"')throw Invalid("json_string");
        std::string v;
        while(true)
        {
            unsigned char c=static_cast<unsigned char>(get());
            if(c=='"')return v;
            if(c<32)throw Invalid("json_control");
            if(c=='\\')
            {
                char e=get();
                switch(e)
                {
                case '"':case '\\':case '/':v+=e;break;
                case 'b':v+='\b';break;case 'f':v+='\f';break;
                case 'n':v+='\n';break;case 'r':v+='\r';break;case 't':v+='\t';break;
                case 'u':
                {
                    unsigned n=hex();
                    if(n>=0xd800&&n<=0xdbff)
                    { if(get()!='\\'||get()!='u')throw Invalid("json_surrogate");
                      unsigned l=hex();if(l<0xdc00||l>0xdfff)throw Invalid("json_surrogate");
                      n=0x10000+((n-0xd800)<<10)+(l-0xdc00); }
                    else if(n>=0xdc00&&n<=0xdfff)throw Invalid("json_surrogate");
                    utf8(v,n);break;
                }
                default:throw Invalid("json_escape");
                }
            }
            else if(c<128)v+=char(c);
            else
            {
                unsigned n=0,remaining=0,minimum=0;
                if(c>=0xc2&&c<=0xdf){n=c&31;remaining=1;minimum=0x80;}
                else if(c>=0xe0&&c<=0xef){n=c&15;remaining=2;minimum=0x800;}
                else if(c>=0xf0&&c<=0xf4){n=c&7;remaining=3;minimum=0x10000;}
                else throw Invalid("json_utf8");
                for(unsigned i=0;i<remaining;++i){unsigned char b=static_cast<unsigned char>(get());
                    if((b&0xc0)!=0x80)throw Invalid("json_utf8"); n=(n<<6)|(b&63);}
                if(n<minimum||n>0x10ffff||(n>=0xd800&&n<=0xdfff))throw Invalid("json_utf8");
                utf8(v,n);
            }
        }
    }
    J value(unsigned depth)
    {
        if(depth>32)throw Invalid("json_depth"); ws(); J j;
        if(p>=s.size())throw Invalid("json_truncated");
        if(s[p]=='{')
        {
            ++p;j.kind=J::Object;if(eat('}'))return j;
            do {ws();std::string k=string();if(!eat(':'))throw Invalid("json_colon");
                J v=value(depth+1);if(!j.object.emplace(k,std::move(v)).second)throw Invalid("json_duplicate_key");
                if(eat('}'))return j; }while(eat(','));throw Invalid("json_object");
        }
        if(s[p]=='[')
        {
            ++p;j.kind=J::Array;if(eat(']'))return j;
            do {j.array.push_back(value(depth+1));if(eat(']'))return j;}while(eat(','));throw Invalid("json_array");
        }
        if(s[p]=='"'){j.kind=J::String;j.string=string();return j;}
        if(s.compare(p,4,"null")==0){p+=4;return j;}
        if(s.compare(p,4,"true")==0){p+=4;j.kind=J::Bool;j.boolean=true;return j;}
        if(s.compare(p,5,"false")==0){p+=5;j.kind=J::Bool;return j;}
        size_t start=p;if(s[p]=='-')++p;
        if(p>=s.size())throw Invalid("json_number");
        if(s[p]=='0')++p;else {if(s[p]<'1'||s[p]>'9')throw Invalid("json_number");while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;}
        if(p<s.size()&&s[p]=='.'){++p;size_t f=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(f==p)throw Invalid("json_number");}
        if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;size_t f=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;if(f==p)throw Invalid("json_number");}
        j.kind=J::Number;j.numberText=s.substr(start,p-start);char* end=nullptr;
        j.number=std::strtod(j.numberText.c_str(),&end);
        if(!end||*end||!std::isfinite(j.number))throw Invalid("json_number");return j;
    }
public:
    explicit Parser(const std::string& v):s(v){}
    J parse() {if(s.size()>65536)throw Invalid("json_size");J j=value(0);ws();if(p!=s.size()||j.kind!=J::Object)throw Invalid("json_root");return j;}
};
inline const J& at(const J& j,const char* k)
{auto i=j.object.find(k);if(j.kind!=J::Object||i==j.object.end())throw Invalid("missing_field");return i->second;}
inline bool has(const J& j,const char* k){return j.object.count(k)!=0;}
inline void closed(const J& j,std::initializer_list<const char*> required,std::initializer_list<const char*> optional={})
{
    if(j.kind!=J::Object)throw Invalid("expected_object");
    std::set<std::string> keys;for(auto k:required){keys.insert(k);at(j,k);}for(auto k:optional)keys.insert(k);
    for(const auto& kv:j.object)if(!keys.count(kv.first))throw Invalid("unknown_field");
}
inline std::string str(const J& j){if(j.kind!=J::String)throw Invalid("expected_string");return j.string;}
inline std::string text(const J& j,const char* k){return str(at(j,k));}
inline std::string token(const J& j)
{
    std::string s=str(j);if(s.empty()||s.size()>128)throw Invalid("invalid_token");
    for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c==':'||c=='-'))throw Invalid("invalid_token");return s;
}
inline std::string tok(const J& j,const char* k){return token(at(j,k));}
inline int64_t integer(const J& j)
{
    if(j.kind!=J::Number||j.number<0)throw Invalid("invalid_integer");
    // Exact decimal/exponent normalization: neither double nor long double may
    // round a fractional or >2^53-1 JSON literal into an admissible integer.
    const std::string& raw=j.numberText;size_t pos=0;
    bool negative=false;if(raw[pos]=='-'){negative=true;++pos;}
    std::string digits;int64_t fractional=0;bool afterDot=false;
    while(pos<raw.size()&&raw[pos]!='e'&&raw[pos]!='E')
    {char c=raw[pos++];if(c=='.')afterDot=true;else{digits+=c;if(afterDot)++fractional;}}
    int64_t exponent=0;bool minus=false;
    if(pos<raw.size())
    {
        ++pos;if(pos<raw.size()&&(raw[pos]=='+'||raw[pos]=='-'))minus=raw[pos++]=='-';
        while(pos<raw.size()) {if(exponent>100000)throw Invalid("invalid_integer");exponent=exponent*10+(raw[pos++]-'0');}
        if(minus)exponent=-exponent;
    }
    size_t leading=digits.find_first_not_of('0');
    if(leading==std::string::npos)return 0;
    if(negative)throw Invalid("invalid_integer");digits.erase(0,leading);
    int64_t scale=exponent-fractional;
    while(scale<0&&!digits.empty()&&digits.back()=='0'){digits.pop_back();++scale;}
    if(scale<0||scale>16||digits.size()+static_cast<size_t>(scale)>16)throw Invalid("invalid_integer");
    digits.append(static_cast<size_t>(scale),'0');
    const std::string maximum="9007199254740991";
    if(digits.size()==maximum.size()&&digits>maximum)throw Invalid("invalid_integer");
    int64_t result=0;for(char c:digits)result=result*10+(c-'0');return result;
}
inline int64_t num(const J& j,const char* k){return integer(at(j,k));}
inline double real(const J& j,double lo,double hi)
{if(j.kind!=J::Number||!std::isfinite(j.number)||j.number<lo||j.number>hi)throw Invalid("invalid_number");return j.number;}
inline const std::vector<J>& arr(const J& j,size_t lo,size_t hi)
{if(j.kind!=J::Array||j.array.size()<lo||j.array.size()>hi)throw Invalid("invalid_array");return j.array;}
inline std::string quote(const std::string& s)
{std::string r="\"";for(char c:s){if(c=='"'||c=='\\')r+='\\';if(static_cast<unsigned char>(c)<32)r+=' ';else r+=c;}return r+'"';}
inline std::string serialize(const J& j)
{
    if(j.kind==J::Null)return "null";
    if(j.kind==J::Bool)return j.boolean?"true":"false";
    if(j.kind==J::Number)return j.numberText;
    if(j.kind==J::String)
    {
        const char* hex="0123456789abcdef";std::string r="\"";
        for(unsigned char c:j.string)
        {
            if(c=='"'||c=='\\'){r+='\\';r+=char(c);}
            else if(c<32){r+="\\u00";r+=hex[c>>4];r+=hex[c&15];}
            else r+=char(c);
        }
        return r+'"';
    }
    std::string result=j.kind==J::Array?"[":"{";bool first=true;
    if(j.kind==J::Array)for(const auto& child:j.array){if(!first)result+=',';first=false;result+=serialize(child);}
    else for(const auto& child:j.object){if(!first)result+=',';first=false;J key;key.kind=J::String;key.string=child.first;result+=serialize(key)+":"+serialize(child.second);}
    return result+(j.kind==J::Array?"]":"}");
}
}
#endif
