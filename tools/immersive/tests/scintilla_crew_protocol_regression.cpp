// SPDX-License-Identifier: GPL-3.0-or-later
// Native production parser/receipt regressions. No engine, GPU, assets or I/O.
#include "modes/immersive_relay/scintilla_crew_protocol.hpp"
#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace ScintillaCrewProtocol;
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
namespace {
const std::string id="00000000-0000-4000-8000-000000000001";
Identity validIdentity() {
    Identity i;i.producerUUID=id;i.raceEpoch="18446744073709551615";i.driverTurnId="9007199254740993";
    i.crewIntentRevision="1";i.rosterRevision="2";i.inputSessionID="input-1";i.inputEpoch="3";
    i.routeRevision="route-1";i.projectionContextId="projection-1";return i;
}
std::string proof(const std::string& person="person-1",const std::string& source="source-1") {
    return "{\"participantId\":"+quote(person)+",\"sourceId\":"+quote(source)+
        ",\"bindingRevision\":\"9007199254740993\",\"physicalFingerprint\":\""+std::string(64,'a')+
        "\",\"verification\":\"gesture\",\"challengeSequence\":\"18446744073709551615\"}";
}
std::string body(const std::string& operation,const Identity& expected=validIdentity()) {
    return "{\"contract\":\"tholus.stk-scintilla-crew/1\",\"schemaVersion\":1,\"requestId\":\""+id+
        "\",\"expected\":"+identityJSON(expected)+(operation=="intent"?",\"required\":true,\"profileId\":\"sc5_crew_standard_v1\"":
        operation=="bindings-confirm"?",\"challengeId\":\""+id+"\",\"bindings\":["+proof()+"]":"")+"}";
}
void replace(std::string& s,const std::string& old,const std::string& value) {
    const auto pos=s.find(old);CHECK(pos!=std::string::npos);s.replace(pos,old.size(),value);
}
template<class F>void rejected(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
void canonicalUInt64AndEveryAuthorityDimension() {
    const auto a=request(body("prepare"),"prepare").expected;
    CHECK(a==validIdentity());CHECK(unsignedValue(a.raceEpoch)==UINT64_MAX);
    CHECK(physicalSourceCanonical("source-1","binding-a","space-b","room-c","grip-d")=="source-1|binding-a|space-b|room-c|grip-d");
    rejected([]{physicalSourceCanonical("source|1","binding-a","space-b","room-c","grip-d");});
    CHECK(unsignedValue(a.driverTurnId)==UINT64_C(9007199254740993));
    const std::array<std::string Identity::*,13> fields={{&Identity::producerUUID,&Identity::raceEpoch,&Identity::driverTurnId,
        &Identity::crewIntentRevision,&Identity::crewRulesRevision,&Identity::rosterRevision,&Identity::inputSessionID,
        &Identity::inputEpoch,&Identity::missionRevision,&Identity::routeRevision,&Identity::projectionContextId,
        &Identity::layoutProfile,&Identity::layoutRevision}};
    for(auto field:fields){auto b=a;b.*field+="x";CHECK(a!=b);}
    for(const auto& bad:{"\"00\"","\"01\"","\"-1\"","\"1e0\"","\"18446744073709551616\"","1","\" 1\"","\"\""}) {
        auto b=body("prepare");replace(b,"\"raceEpoch\":\"18446744073709551615\"","\"raceEpoch\":"+std::string(bad));
        rejected([&]{request(b,"prepare");});
    }
    rejected([]{unsignedValue("18446744073709551616");});
    auto zero=validIdentity();zero.driverTurnId="0";CHECK(request(body("prepare",zero),"prepare").expected.driverTurnId=="0");
}
void closedOperationAndProfileSchemas() {
    for(const std::string op:{"intent","prepare","bindings-confirm"}) {
        CHECK(request(body(op),op).operation==op);
        auto b=body(op);b.insert(1,"\"unknown\":1,");rejected([&]{request(b,op);});
        b=body(op);b.insert(1,"\"schemaVersio\\u006e\":1,");rejected([&]{request(b,op);});
        b=body(op);replace(b,"\"expected\":{","\"expected\":{\"unknown\":1,");rejected([&]{request(b,op);});
        b=body(op);replace(b,"\"schemaVersion\":1","\"schemaVersion\":1.5");rejected([&]{request(b,op);});
    }
    auto b=body("intent");replace(b,"sc5_crew_standard_v1","sc5_crew_party_v1");CHECK(request(b,"intent").profileId==partyProfile());
    b=body("intent");replace(b,"\"required\":true","\"required\":false");CHECK(!request(b,"intent").required);
    b=body("intent");replace(b,"sc5_crew_standard_v1","crew_standard_sc2_v1");rejected([&]{request(b,"intent");});
    b=body("intent");replace(b,"\"required\":true","\"required\":1");rejected([&]{request(b,"intent");});
    b=body("prepare");b.insert(1,"\"profileId\":\"sc5_crew_party_v1\",");rejected([&]{request(b,"prepare");});
    b=body("intent");replace(b,id,"00000000-0000-4000-8000-00000000000A");rejected([&]{request(b,"intent");});
    rejected([&]{request(body("prepare"),"activate");});
    rejected([&]{request(std::string(16385,' '),"prepare");});
    b=body("intent");b.append(16384-b.size(),' ');CHECK(request(b,"intent").required);
    CHECK(std::string(resultBucket(false))=="scintilla_sc5_standard_v1");
    CHECK(std::string(resultBucket(true))=="scintilla_sc5_party_v1");
}
void onlyIntentAcceptsIncompleteAndLegacyAuthority() {
    Identity i=validIdentity();i.raceEpoch="0";i.crewIntentRevision="0";i.crewRulesRevision="0";
    i.rosterRevision="0";i.inputSessionID.clear();i.inputEpoch="0";i.missionRevision="0";
    i.routeRevision.clear();i.projectionContextId.clear();i.layoutProfile.clear();i.layoutRevision="0";
    CHECK(request(body("intent",i),"intent").expected==i);
    for(const std::string op:{"bindings-confirm","prepare"})rejected([&]{request(body(op,i),op);});
    i=validIdentity();i.layoutProfile="front_rear_readonly_v1";i.layoutRevision="1";
    CHECK(request(body("intent",i),"intent").expected==i);
    rejected([&]{request(body("prepare",i),"prepare");});
    i.layoutProfile="front_rear_cargo_v2";i.layoutRevision="2";CHECK(request(body("intent",i),"intent").expected==i);
    i.layoutRevision="3";rejected([&]{request(body("intent",i),"intent");});
    i=validIdentity();i.layoutProfile.clear();i.layoutRevision="0";rejected([&]{request(body("intent",i),"intent");});
    const std::array<std::string Identity::*,7> required={{&Identity::raceEpoch,&Identity::crewIntentRevision,
        &Identity::rosterRevision,&Identity::inputEpoch,&Identity::inputSessionID,&Identity::routeRevision,&Identity::projectionContextId}};
    for(size_t n=0;n<required.size();++n){i=validIdentity();i.*required[n]=n<4?"0":"";rejected([&]{request(body("prepare",i),"prepare");});}
}
void bindingProofsAreClosedBoundedAndBijective() {
    const auto r=request(body("bindings-confirm"),"bindings-confirm");
    CHECK(r.bindings.size()==1&&r.bindings[0].bindingRevision=="9007199254740993");
    CHECK(r.bindings[0].challengeSequence=="18446744073709551615");
    auto b=body("bindings-confirm");replace(b,"gesture","retained");CHECK(request(b,"bindings-confirm").bindings[0].verification=="retained");
    for(unsigned mutation=0;mutation<7;++mutation) {
        b=body("bindings-confirm");
        if(mutation==0)replace(b,"gesture","assumed");
        if(mutation==1)replace(b,std::string(64,'a'),std::string(64,'A'));
        if(mutation==2)replace(b,"\"participantId\":","\"unknown\":true,\"participantId\":");
        if(mutation==3)replace(b,"person-1","");
        if(mutation==4)replace(b,"source-1","source/1");
        if(mutation==5)replace(b,"\"challengeId\":\""+id+"\"","\"challengeId\":\"\"");
        if(mutation==6)replace(b,"\"bindingRevision\":\"9007199254740993\"","\"bindingRevision\":9007199254740993");
        rejected([&]{request(b,"bindings-confirm");});
    }
    for(unsigned n=0;n<=6;++n) {
        b=body("bindings-confirm");std::string proofs;
        for(unsigned x=0;x<n;++x){if(x)proofs+=',';proofs+=proof("person-"+std::to_string(x),"source-"+std::to_string(x));}
        replace(b,"["+proof()+"]","["+proofs+"]");
        if(n==0||n>5)rejected([&]{request(b,"bindings-confirm");});else CHECK(request(b,"bindings-confirm").bindings.size()==n);
    }
    for(bool duplicatePerson:{false,true}) {
        b=body("bindings-confirm");replace(b,"["+proof()+"]","["+proof()+","+
            proof(duplicatePerson?"person-1":"person-2",duplicatePerson?"source-2":"source-1")+"]");
        rejected([&]{request(b,"bindings-confirm");});
    }
}
void exactReceiptSurvivesAuthorityChange() {
    Receipts receipts;const auto r=request(body("prepare"),"prepare");
    const std::string exact="{\"ok\":true,\"prepared\":true,\"identity\":"+identityJSON(r.expected)+"}";
    CHECK(receipts.check(r)==Receipts::New&&receipts.response(r)==nullptr);
    CHECK(receipts.commit(r,exact));CHECK(receipts.check(r)==Receipts::Duplicate);
    Identity current=r.expected;current.driverTurnId="9007199254740994";current.inputEpoch="4";
    CHECK(current!=r.expected);
    const auto reply=receipts.response(r);CHECK(reply&&reply->body==exact&&reply->httpStatus==200);
    CHECK(!receipts.commit(r,"replacement"));CHECK(receipts.response(r)->body==exact);
    auto conflict=r;conflict.fingerprint+="different";CHECK(receipts.check(conflict)==Receipts::Conflict);
    CHECK(receipts.response(conflict)==nullptr&&!receipts.commit(conflict,"conflict"));
    for(unsigned n=2;n<4098;++n) {
        auto next=r;std::ostringstream serial;serial<<std::hex<<std::setfill('0')<<std::setw(12)<<n;
        next.requestId="00000000-0000-4000-8000-"+serial.str();next.fingerprint=next.requestId;
        CHECK(receipts.commit(next,std::to_string(n),409));CHECK(receipts.size()<=64);
    }
    CHECK(receipts.size()==64&&receipts.check(r)==Receipts::New&&receipts.response(r)==nullptr);
    receipts.reset();CHECK(receipts.size()==0);
}
struct Case{const char* name;void(*run)();};const Case cases[]={
    {"canonicalUInt64AndEveryAuthorityDimension",canonicalUInt64AndEveryAuthorityDimension},
    {"closedOperationAndProfileSchemas",closedOperationAndProfileSchemas},
    {"onlyIntentAcceptsIncompleteAndLegacyAuthority",onlyIntentAcceptsIncompleteAndLegacyAuthority},
    {"bindingProofsAreClosedBoundedAndBijective",bindingProofsAreClosedBoundedAndBijective},
    {"exactReceiptSurvivesAuthorityChange",exactReceiptSurvivesAuthorityChange}};
}
int main(int argc,char** argv) {
    if(argc==2&&std::string(argv[1])=="--list") {
        for(const auto& c:cases)std::cout<<c.name<<'\n';return 0;
    }
    if(argc!=1&&(argc!=3||std::string(argv[1])!="--case"))return 2;
    bool found=false;
    for(const auto& c:cases){if(argc==3&&std::string(argv[2])!=c.name)continue;c.run();found=true;}
    return found?0:2;
}
