#!/usr/bin/env python3
"""Validate delivered SC4 media offline, with optional complete video decode.

No compiler, app, game, player UI or projection is started. Media decode is
evidence about these assets, not about the TholusOS playback integration.
"""
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[3]


def unique(pairs):
    result={}
    for key,value in pairs:
        if key in result: raise ValueError("duplicate JSON key: "+key)
        result[key]=value
    return result


def sha(path):
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024*1024), b""): digest.update(block)
    return digest.hexdigest()


def local(root, relative, expected, limit):
    assert isinstance(relative,str) and relative and not Path(relative).is_absolute()
    assert all(part not in ("", ".", "..") for part in relative.split("/")) and "\\" not in relative
    path=(root/relative).resolve(strict=True)
    assert path.is_relative_to(root.resolve()) and path.is_file() and path.stat().st_size<=limit
    assert len(expected)==64 and sha(path)==expected, relative
    return path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest",type=Path,default=ROOT/"data/immersive_relay/scintilla/intro_it_v1/manifest.json")
    parser.add_argument("--decode",action="store_true")
    parser.add_argument("--report",type=Path)
    args=parser.parse_args(); path=args.manifest.resolve(strict=True);root=path.parent
    assert path.stat().st_size<=65536
    manifest=json.loads(path.read_bytes(),object_pairs_hook=unique)
    crew=manifest["profileId"] in ("sc5_crew_standard_v1","sc5_crew_party_v1")
    common={"contract","schemaVersion","profileId","profileRevision","locale","durationSeconds","frameRateNumerator","frameRateDenominator","frameCount","width","height","license","author","variants"}
    assert set(manifest)==common|({"crewRulesRevision","layoutProfile","layoutRevision"} if crew else set())
    assert manifest["contract"]==("tholus.stk-scintilla-intro/2" if crew else "tholus.stk-scintilla-intro/1") and manifest["schemaVersion"]==(2 if crew else 1)
    assert manifest["profileId"] in ("sc4_cargo_three_slots_v1","sc5_crew_standard_v1","sc5_crew_party_v1") and manifest["profileRevision"]=="1" and manifest["locale"]=="it"
    if crew: assert (manifest["crewRulesRevision"],manifest["layoutProfile"],manifest["layoutRevision"])==("1","front_rear_crew_v3","3")
    assert [manifest[k] for k in ("durationSeconds","frameRateNumerator","frameRateDenominator","frameCount","width","height")]==[40,30,1,1200,1920,1080]
    assert manifest["license"]=="CC-BY-4.0" and manifest["author"]
    assert len(manifest["variants"])==3
    ffprobe=shutil.which("ffprobe");assert ffprobe
    report={"manifestSHA256":sha(path),"scope":"offline asset hash/metadata/image inspection"+(", complete decode" if args.decode else ""),
            "appBuildRun":False,"physicalProjection":False,"variants":[]}
    cuts=[0,6,12,19,26,33,40]
    for variant,(name,minimum,maximum) in zip(manifest["variants"],[("C1",1,1),("C2",2,2),("C3plus",3,5)]):
        assert [variant[k] for k in ("variantId","minCrew","maxCrew")]==[name,minimum,maximum]
        video=local(root,variant["videoPath"],variant["videoSHA256"],128*1024*1024)
        vtt=local(root,variant["subtitlesPath"],variant["subtitlesSHA256"],256*1024)
        subtitles=vtt.read_text();assert subtitles.startswith("WEBVTT\n") and subtitles.count(" --> ")==6
        for start,end in zip(cuts,cuts[1:]):
            assert f"00:00:{start:02d}.000 --> 00:00:{end:02d}.000" in subtitles
        assert len(variant["panels"])==6
        for panel in variant["panels"]:
            image=local(root,panel["path"],panel["sha256"],8*1024*1024)
            with Image.open(image) as decoded:
                assert decoded.size==(1920,1080) and decoded.format=="PNG"; decoded.verify()
        info=json.loads(subprocess.check_output([ffprobe,"-v","error","-show_streams","-show_format","-of","json",str(video)],text=True))
        assert len(info["streams"])==1
        stream=info["streams"][0]
        assert stream["codec_type"]=="video" and stream["codec_name"]=="h264" and stream["pix_fmt"]=="yuv420p"
        assert stream["width"]==1920 and stream["height"]==1080 and int(stream["nb_frames"])==1200
        assert Fraction(stream["avg_frame_rate"])==30 and Fraction(stream["r_frame_rate"])==30
        assert int(stream["duration_ts"])*Fraction(stream["time_base"])==40
        assert Fraction(info["format"]["duration"])==40
        if args.decode:
            ffmpeg=shutil.which("ffmpeg");assert ffmpeg
            subprocess.run([ffmpeg,"-v","error","-xerror","-i",str(video),"-map","0:v:0","-f","null","-"],check=True)
        report["variants"].append({"variantId":name,"videoSHA256":variant["videoSHA256"],"bytes":video.stat().st_size,
                                   "width":1920,"height":1080,"fps":"30/1","frames":1200,"durationSeconds":40,
                                   "panels":6,"subtitles":6,"completeDecode":args.decode})
    output=json.dumps(report,indent=2)+"\n"
    if args.report:
        report_path=args.report.resolve();ssd=Path("/Volumes/SanDisk 2TB")
        assert ssd.is_mount() and report_path.is_relative_to(ssd.resolve())
        report_path.parent.mkdir(parents=True,exist_ok=True);report_path.write_text(output)
    print(output,end="")


if __name__=="__main__":main()
