#!/usr/bin/env python3
"""Control the explicit native SC4 diagnostic and export its exact raw trace.

This client never starts a producer, drives a kart, fabricates a grant or assigns
cargo. Preflight asks the native reader to check real, already installed evidence;
it does not create evidence. Commands require the expected producer and race.
Export requires an already closed recording and a held world. Credentials come
from an environment variable and are never printed or put in command arguments.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
import uuid

CONTRACT = "tholus.stk-scintilla-measurement/1"
SSD = Path("/Volumes/SanDisk 2TB")
MAX_TRACE = 64 * 1024 * 1024
MAX_REPLY = 256 * 1024
UUID = re.compile(r"[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\Z")


class Invalid(ValueError):
    pass


def require(condition, reason):
    if not condition:
        raise Invalid(reason)


def decimal(value, maximum=MAX_TRACE):
    require(isinstance(value, str) and re.fullmatch(r"0|[1-9][0-9]{0,19}", value), "invalid_decimal")
    result = int(value)
    require(result <= maximum, "decimal_out_of_bounds")
    return result


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate_json_key")
        result[key] = value
    return result


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, response, code, message, headers, new_url):
        raise Invalid("redirect_refused")


class Client:
    def __init__(self, base, token):
        url = urllib.parse.urlsplit(base)
        require(url.scheme == "http" and url.hostname in ("127.0.0.1", "::1", "localhost")
                and not url.username and not url.password and not url.query and not url.fragment
                and url.path.rstrip("/") == "/immersive/v1", "local_relay_url_required")
        require(isinstance(token, str) and token and "\n" not in token and "\r" not in token, "missing_token")
        self.base = base.rstrip("/") + "/scintilla/mission/measurement"
        self.token = token
        # Ignore proxy environment variables: the authenticated endpoint is local.
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())

    def call(self, path, body=None):
        request = urllib.request.Request(self.base + path,
            data=None if body is None else json.dumps(body, separators=(",", ":")).encode("utf-8"),
            headers={"X-STK-Relay-Token": self.token, "Content-Type": "application/json"},
            method="GET" if body is None else "POST")
        try:
            with self.opener.open(request, timeout=30) as response:
                require(response.status == 200, "http_status")
                raw = response.read(MAX_REPLY + 1)
        except urllib.error.HTTPError as error:
            # Do not echo response bodies or request headers containing secrets.
            raise Invalid("native_http_" + str(error.code)) from None
        require(len(raw) <= MAX_REPLY, "reply_too_large")
        try:
            result = json.loads(raw, object_pairs_hook=unique,
                parse_constant=lambda _: (_ for _ in ()).throw(Invalid("nonfinite_json")))
        except (ValueError, UnicodeError, RecursionError) as error:
            raise Invalid("invalid_reply") from error
        require(isinstance(result, dict) and result.get("ok") is True, "native_rejected")
        return result


def safe_output(path):
    require(SSD.is_mount() and os.access(SSD, os.W_OK), "ssd_not_mounted_or_writable")
    output = Path(path).expanduser()
    require(output.is_absolute() and output.parent.is_dir(), "existing_absolute_parent_required")
    parent = output.parent.resolve(strict=True)
    require(parent.is_relative_to(SSD.resolve(strict=True)) and not output.is_symlink(), "output_must_be_on_ssd")
    result = parent / output.name
    require(not result.exists(), "output_already_exists")
    return result


def export(client, output_path):
    output = safe_output(output_path)
    before = client.call("/state")
    require(before.get("contract") == CONTRACT and before.get("recording") is False
            and before.get("closed") is True, "recording_must_be_closed")
    identity = before.get("recordingID")
    require(isinstance(identity, str) and UUID.fullmatch(identity), "invalid_recording_id")
    size = decimal(before.get("byteCount"))
    expected_hash = before.get("sha256")
    require(size > 0 and isinstance(expected_hash, str) and re.fullmatch(r"[0-9a-f]{64}", expected_hash), "invalid_trace_metadata")
    temporary = output.with_name(output.name + ".partial-" + str(uuid.uuid4()))
    offset = 0
    hasher = hashlib.sha256()
    try:
        with temporary.open("xb") as stream:
            while offset < size:
                reply = client.call("/trace?offset=" + str(offset))
                require(reply.get("recordingID") == identity and decimal(reply.get("offset")) == offset,
                        "trace_identity_changed")
                text = reply.get("data")
                require(isinstance(text, str), "invalid_trace_chunk")
                raw = text.encode("utf-8", errors="strict")
                next_offset = decimal(reply.get("nextOffset"))
                require(0 < len(raw) <= 32768 and next_offset == offset + len(raw) <= size
                        and type(reply.get("eof")) is bool and reply["eof"] == (next_offset == size), "invalid_trace_chunk")
                stream.write(raw); hasher.update(raw); offset = next_offset
            stream.flush(); os.fsync(stream.fileno())
        after = client.call("/state")
        require(all(after.get(key) == before.get(key) for key in
                    ("recordingID", "producerUUID", "raceEpoch", "byteCount", "sha256", "closed", "recording")),
                "trace_identity_changed")
        require(hasher.hexdigest() == expected_hash, "trace_hash_mismatch")
        # Exclusive publication: a concurrent file is never overwritten.
        os.link(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)
    return {"path": str(output), "byteCount": str(size), "sha256": expected_hash, "recordingID": identity}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("preflight", "start", "stop", "state", "export"))
    parser.add_argument("--base", default="http://127.0.0.1:18890/immersive/v1")
    parser.add_argument("--token-env", default="STK_RELAY_TOKEN")
    parser.add_argument("--producer")
    parser.add_argument("--race")
    parser.add_argument("--request-id", help="Reuse the same UUID only to retry the same command")
    parser.add_argument("--phase", choices=("shell", "open"))
    parser.add_argument("--crew-profile", choices=("sc5_crew_standard_v1", "sc5_crew_party_v1"), help="Explicit diagnostic layout v3; does not enable gameplay intent")
    parser.add_argument("--output")
    args = parser.parse_args()
    try:
        require(args.crew_profile is None or args.operation == "start", "crew_profile_is_diagnostic_start_only")
        client = Client(args.base, os.environ.get(args.token_env))
        if args.operation in ("preflight", "start", "stop"):
            require(isinstance(args.producer, str) and UUID.fullmatch(args.producer), "producer_uuid_required")
            decimal(args.race, (1 << 64) - 1)
            request_id = args.request_id or str(uuid.uuid4())
            require(UUID.fullmatch(request_id), "invalid_request_id")
            body = dict(contract=CONTRACT, schemaVersion=1, requestId=request_id,
                        producerUUID=args.producer, raceEpoch=args.race)
            if args.operation == "start":
                require(args.phase is not None, "phase_required")
                body["phase"] = args.phase
                if args.crew_profile is not None:
                    body["crewProfile"] = args.crew_profile
            # Keep the receipt key even if the HTTP result is uncertain. It is
            # an operation identifier, never a credential.
            print("requestId: " + request_id, file=sys.stderr)
            result = client.call("/" + args.operation, body)
            result["requestId"] = request_id
        elif args.operation == "export":
            require(args.output is not None, "output_required")
            result = export(client, args.output)
        else:
            result = client.call("/state")
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
        return 0
    except (Invalid, OSError, urllib.error.URLError, UnicodeError) as error:
        print("measurement: " + (str(error) if isinstance(error, Invalid) else type(error).__name__), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
