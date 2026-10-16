// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_TELEMETRY_HPP
#define HEADER_SCINTILLA_TELEMETRY_HPP
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// Session telemetry as NDJSON, one file per race. The game thread only formats
// a line and enqueues it; a background thread owns all file I/O. A full queue
// drops lines (counted in the footer) instead of ever blocking rendering.
// Records are observations for balancing, never gameplay authority or proof.
namespace ScintillaTelemetry
{
extern const char* const Schema;

// Compact JSON object builder. Strings are escaped; non-finite numbers become null.
class Line
{
public:
    explicit Line(const char* type);
    Line& text(const char* key, const std::string& value);
    Line& number(const char* key, double value);
    Line& integer(const char* key, int64_t value);
    Line& unsignedInteger(const char* key, uint64_t value);
    Line& flag(const char* key, bool value);
    // value must already be valid JSON.
    Line& raw(const char* key, const std::string& value);
    std::string str() const { return m_body+"}"; }
private:
    std::string m_body;
    void key(const char* key);
};
std::string escape(const std::string& value);

class Writer
{
public:
    Writer() {}
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    // Creates a new file (never overwrites) and starts the writer thread.
    bool open(const std::string& path, std::string* error);
    bool isOpen() const { return m_open; }
    const std::string& path() const { return m_path; }
    // Never blocks on I/O. Returns false when the line was dropped.
    bool write(const std::string& line);
    // Writes a footer, flushes and joins. Safe to call more than once.
    void close();
    uint64_t dropped() const;
    uint64_t written() const;
private:
    void run();
    static const size_t MaxQueuedBytes = 8u << 20;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<std::string> m_queue;
    size_t m_queuedBytes = 0;
    uint64_t m_dropped = 0, m_written = 0, m_accepted = 0;
    bool m_open = false, m_stop = false;
    std::FILE* m_file = nullptr;
    std::string m_path;
    std::thread m_thread;
};
}
#endif
