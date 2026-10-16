// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_telemetry.hpp"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace ScintillaTelemetry
{
const char* const Schema="tholus.stk-scintilla-telemetry/1";

std::string escape(const std::string& value)
{
    static const char hex[]="0123456789abcdef";
    std::string out="\"";
    for(unsigned char c:value)
    {
        if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
        else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    return out+'"';
}

Line::Line(const char* type):m_body("{\"type\":"+escape(type)) {}
void Line::key(const char* key) {m_body+=",";m_body+=escape(key);m_body+=":";}
Line& Line::text(const char* k,const std::string& value) {key(k);m_body+=escape(value);return *this;}
Line& Line::number(const char* k,double value)
{
    key(k);
    if(!std::isfinite(value)){m_body+="null";return *this;}
    // Shortest decimal that reads back exactly.
    char buffer[40];
    for(int precision=6;precision<=17;++precision)
    {
        std::snprintf(buffer,sizeof(buffer),"%.*g",precision,value);
        if(std::strtod(buffer,nullptr)==value) break;
    }
    m_body+=buffer;return *this;
}
Line& Line::integer(const char* k,int64_t value) {key(k);m_body+=std::to_string(value);return *this;}
Line& Line::unsignedInteger(const char* k,uint64_t value) {key(k);m_body+=std::to_string(value);return *this;}
Line& Line::flag(const char* k,bool value) {key(k);m_body+=value?"true":"false";return *this;}
Line& Line::raw(const char* k,const std::string& value) {key(k);m_body+=value;return *this;}

Writer::~Writer() {close();}

bool Writer::open(const std::string& path,std::string* error)
{
    std::string ignored;if(!error)error=&ignored;
    close();
    // Exclusive creation: an existing session file is never overwritten.
    std::FILE* file=std::fopen(path.c_str(),"wbx");
    if(!file){*error="telemetry_file_unavailable";return false;}
    std::lock_guard<std::mutex> lock(m_mutex);
    m_file=file;m_path=path;m_queue.clear();m_queuedBytes=0;
    m_dropped=m_written=m_accepted=0;m_stop=false;m_open=true;
    m_thread=std::thread(&Writer::run,this);
    return true;
}

bool Writer::write(const std::string& line)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_open||m_stop) return false;
        if(m_queuedBytes+line.size()+1>MaxQueuedBytes){++m_dropped;return false;}
        m_queue.push_back(line);m_queuedBytes+=line.size()+1;++m_accepted;
    }
    m_wake.notify_one();
    return true;
}

void Writer::run()
{
    std::vector<std::string> batch;
    while(true)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait(lock,[this]{return m_stop||!m_queue.empty();});
            if(m_queue.empty()&&m_stop) return;
            batch.assign(m_queue.begin(),m_queue.end());m_queue.clear();m_queuedBytes=0;
        }
        for(const auto& line:batch)
        {
            std::fwrite(line.data(),1,line.size(),m_file);std::fputc('\n',m_file);
        }
        std::fflush(m_file);
        std::lock_guard<std::mutex> lock(m_mutex);m_written+=batch.size();
    }
}

void Writer::close()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_open) return;
        // The footer bypasses the byte budget: it reports what was lost.
        m_queue.push_back(Line("footer").text("schema",Schema).unsignedInteger("accepted",m_accepted)
            .unsignedInteger("dropped",m_dropped).str());
        m_stop=true;
    }
    m_wake.notify_one();
    if(m_thread.joinable()) m_thread.join();
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_file){std::fclose(m_file);m_file=nullptr;}
    m_open=false;
}

uint64_t Writer::dropped() const {std::lock_guard<std::mutex> lock(m_mutex);return m_dropped;}
uint64_t Writer::written() const {std::lock_guard<std::mutex> lock(m_mutex);return m_written;}
}
