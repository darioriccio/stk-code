// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_telemetry.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#define CHECK(x) do { if(!(x)){std::cerr<<__LINE__<<": "<<#x<<std::endl;std::exit(1);} } while(false)
using namespace ScintillaTelemetry;
namespace {
std::string temporary(const char* name)
{
    const char* base=std::getenv("TMPDIR");std::string path=std::string(base&&*base?base:"/tmp")+"/"+name+"-"+std::to_string(getpid())+".ndjson";
    unlink(path.c_str());return path;
}
std::vector<std::string> lines(const std::string& path)
{std::ifstream file(path);std::vector<std::string> out;std::string line;while(std::getline(file,line))out.push_back(line);return out;}
void linesAreEscapedCompactJSON()
{
    const std::string line=Line("shot").text("reason","a\"b\\c\n").number("tenth",0.1).number("missing",NAN)
        .integer("negative",-5).unsignedInteger("big",18446744073709551615ull).flag("ok",true).raw("list","[1,2]").str();
    CHECK(line=="{\"type\":\"shot\",\"reason\":\"a\\\"b\\\\c\\u000a\",\"tenth\":0.1,\"missing\":null,\"negative\":-5,\"big\":18446744073709551615,\"ok\":true,\"list\":[1,2]}");
    const ScintillaJSON::J parsed=ScintillaJSON::Parser(line).parse();
    CHECK(ScintillaJSON::text(parsed,"reason")=="a\"b\\c\n"&&ScintillaJSON::at(parsed,"missing").kind==ScintillaJSON::J::Null);
}
void writerKeepsOrderAndReportsInFooter()
{
    const std::string path=temporary("scintilla-telemetry-order");
    Writer w;std::string error;CHECK(w.open(path,&error)&&error.empty()&&w.isOpen());
    for(int i=0;i<5000;++i)CHECK(w.write(Line("n").integer("i",i).str()));
    w.close();w.close();CHECK(!w.isOpen()&&!w.write("{}"));
    const auto written=lines(path);CHECK(written.size()==5001);
    for(int i=0;i<5000;++i)CHECK(written[i]==Line("n").integer("i",i).str());
    const auto footer=ScintillaJSON::Parser(written.back()).parse();
    CHECK(ScintillaJSON::text(footer,"type")=="footer"&&ScintillaJSON::at(footer,"accepted").number==5000&&ScintillaJSON::at(footer,"dropped").number==0);
    // Exclusive creation: an existing session file is never overwritten.
    Writer again;CHECK(!again.open(path,&error)&&error=="telemetry_file_unavailable");CHECK(lines(path).size()==5001);
    unlink(path.c_str());
}
void oversizedLinesAreDroppedNotBlocking()
{
    const std::string path=temporary("scintilla-telemetry-drop");
    Writer w;CHECK(w.open(path,nullptr));CHECK(!w.write(std::string(9u<<20,'x')));CHECK(w.write("{\"type\":\"small\"}"));
    CHECK(w.dropped()==1);w.close();
    const auto written=lines(path);CHECK(written.size()==2&&written[0]=="{\"type\":\"small\"}");
    CHECK(ScintillaJSON::at(ScintillaJSON::Parser(written[1]).parse(),"dropped").number==1);
    unlink(path.c_str());
}
}
int main(int argc,char** argv)
{
    struct Case{const char* name;void(*run)();};
    const Case cases[]={
#define TELEMETRY_CASE(name) {#name,name},
#include "scintilla_telemetry_cases.inc"
#undef TELEMETRY_CASE
    };
    const bool selected=argc==3&&std::string(argv[1])=="--case";
    if(argc!=1&&!selected)return 2;
    bool found=false;
    for(const auto& c:cases)if(!selected||c.name==std::string(argv[2])){found=true;std::cout<<c.name<<std::endl;c.run();}
    return found?0:2;
}
