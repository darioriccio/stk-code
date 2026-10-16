// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_REPORT_FILE_HPP
#define HEADER_SCINTILLA_CREW_REPORT_FILE_HPP
#include "modes/immersive_relay/scintilla_crew_result.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace ScintillaCrew {
inline std::string reportFileName(const std::string& session,const Result& result) {
    if(!result.valid||!ScintillaMissionProtocol::uuid(result.producerUUID))return std::string();
    // The readable stem is bounded and never a path. The session fingerprint
    // includes every byte; producer/race/result remain the authoritative key.
    uint64_t fingerprint=UINT64_C(14695981039346656037);std::string stem;
    for(unsigned char c:session){fingerprint^=c;fingerprint*=UINT64_C(1099511628211);
        if(stem.size()<32)stem+=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')?char(c):'_';}
    if(stem.empty())stem="session";
    std::ostringstream out;out<<stem<<'-'<<std::hex<<std::setfill('0')<<std::setw(16)<<fingerprint<<std::dec
        <<'-'<<result.producerUUID<<'-'<<result.raceEpoch<<'-'<<result.missionRevision<<'-'<<result.finishSequence<<".json";
    return out.str();
}
// Exclusive creation is the invariant: retries and reset can never truncate an
// earlier SC5 record, including after this process has restarted.
inline bool writeReportExclusive(const std::string& path,const std::string& bytes) {
#ifdef _WIN32
    const int file=_open(path.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
#else
    const int file=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
#endif
    if(file<0)return false;std::size_t offset=0;bool success=true;
    while(offset<bytes.size()) {
        const std::size_t count=std::min(bytes.size()-offset,std::size_t(std::numeric_limits<int>::max()));
#ifdef _WIN32
        const int written=_write(file,bytes.data()+offset,unsigned(count));
#else
        const ssize_t written=write(file,bytes.data()+offset,count);
#endif
        if(written<0&&errno==EINTR)continue;
        if(written<=0){success=false;break;}offset+=std::size_t(written);
    }
#ifdef _WIN32
    if(_close(file)!=0)success=false;
#else
    if(close(file)!=0)success=false;
#endif
    if(!success)std::remove(path.c_str()); // Only the file created above is ours.
    return success;
}
}
#endif
