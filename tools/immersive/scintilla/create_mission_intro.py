#!/usr/bin/env python3
"""Author the final SC4 Italian 2D tutorial offline. No app/game is built or run.

Original procedural illustrations and wording: CC-BY-4.0, Immersive Relay
contributors. Pillow and an installed font are authoring dependencies only;
neither a font file nor a new playback engine is distributed.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[3]
SSD = Path("/Volumes/SanDisk 2TB")
CUTS = [0, 180, 360, 570, 780, 990, 1200]
W, H = 1920, 1080
PAPER, INK, TEAL, RED, GOLD, PURPLE = "#F7F0DC", "#183343", "#087F80", "#D44931", "#F0B62D", "#6650A0"
LIGHT, MUTED = "#FFFCF3", "#647A7C"
VARIANTS = [("C1", 1, 1), ("C2", 2, 2), ("C3plus", 3, 5)]


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def font(path, size):
    return ImageFont.truetype(str(path), size)


def text(draw, at, value, face, fill=INK, anchor=None):
    draw.text(at, value, font=face, fill=fill, anchor=anchor, stroke_width=0)


def arrow(draw, start, end, color=TEAL, width=12):
    draw.line([start, end], fill=color, width=width)
    angle = math.atan2(end[1] - start[1], end[0] - start[0])
    points = [end] + [(end[0] - 32 * math.cos(angle + side * .55),
                      end[1] - 32 * math.sin(angle + side * .55)) for side in (-1, 1)]
    draw.polygon(points, fill=color)


def module(draw, center, radius=42, color=TEAL):
    x, y = center
    draw.polygon([(x + radius * math.cos(a * math.pi / 3), y + radius * math.sin(a * math.pi / 3))
                  for a in range(6)], fill=color, outline=INK, width=5)
    draw.polygon([(x, y-radius*.55), (x+radius*.35, y), (x, y+radius*.55), (x-radius*.35, y)], fill=LIGHT)


def capsule(draw, center, radius=100, opened=False):
    x, y = center
    draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=GOLD, outline=INK, width=8)
    if opened:
        draw.ellipse((x-radius*.80, y-radius*.80, x+radius*.80, y+radius*.80), fill=LIGHT, outline=INK, width=5)
        module(draw, center, radius*.5)
        for side in (-1, 1):
            draw.arc((x-radius*1.12, y-radius*1.12, x+radius*1.12, y+radius*1.12),
                     120 if side < 0 else 300, 240 if side < 0 else 420, fill=RED, width=14)
    else:
        draw.line([(x-15, y-radius*.82), (x+20, y-35), (x-18, y), (x+18, y+35), (x-8, y+radius*.82)], fill=INK, width=8)


def beacon(draw, center, lit=False, scale=1):
    x, y = center
    def xy(dx, dy): return (x+dx*scale, y+dy*scale)
    draw.polygon([xy(-38, 100), xy(-24, -20), xy(24, -20), xy(38, 100)], fill=TEAL if lit else "#D4DDCF", outline=INK, width=6)
    draw.rounded_rectangle((*xy(-42, -60), *xy(42, -5)), radius=12*scale, fill=GOLD if lit else PAPER, outline=INK, width=6)
    draw.polygon([xy(-52, -62), xy(0, -100), xy(52, -62)], fill=RED, outline=INK, width=5)
    draw.line([xy(-55, 104), xy(55, 104)], fill=INK, width=8)
    if lit:
        for side in (-1, 1):
            draw.polygon([xy(side*50, -48), xy(side*160, -90), xy(side*160, 5), xy(side*50, -24)], fill="#F4D875")
    else:
        draw.line([xy(-18, -46), xy(18, -18)], fill=MUTED, width=6)
        draw.line([xy(18, -46), xy(-18, -18)], fill=MUTED, width=6)


def orb(draw, center, radius=75):
    x, y = center
    draw.ellipse((x-radius-14, y-radius-14, x+radius+14, y+radius+14), outline=GOLD, width=4)
    draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=GOLD, outline=INK, width=6)
    for dx in (-23, 23): draw.ellipse((x+dx-6, y-16, x+dx+6, y-4), fill=INK)
    draw.arc((x-29, y-17, x+29, y+34), 15, 165, fill=INK, width=5)
    draw.polygon([(x+radius-7, y+10), (x+radius+48, y+30), (x+radius+19, y+44),
                  (x+radius+54, y+76), (x+radius-12, y+45)], fill=RED, outline=INK, width=4)


def kart(draw, center, secured=0, scale=1, show_slots=True):
    x, y = center
    def box(a, b, c, d): return (x+a*scale, y+b*scale, x+c*scale, y+d*scale)
    for dx in (-132, 132):
        draw.rounded_rectangle(box(dx-32, -24, dx+32, 78), radius=20*scale, fill=INK)
    draw.rounded_rectangle(box(-165, -20, 165, 75), radius=35*scale, fill=TEAL, outline=INK, width=7)
    draw.polygon([(x-100*scale, y-20*scale), (x-66*scale, y-108*scale),
                  (x+66*scale, y-108*scale), (x+100*scale, y-20*scale)], fill="#A5DCD0", outline=INK, width=6)
    draw.rounded_rectangle(box(-174, 36, 174, 68), radius=12*scale, fill=GOLD, outline=INK, width=5)
    for dx in (-120, 120): draw.ellipse(box(dx-17, 4, dx+17, 28), fill=LIGHT, outline=INK, width=3)
    for i, dx in enumerate((-64, 0, 64) if show_slots else ()):
        draw.ellipse(box(dx-25, -172, dx+25, -122), fill=LIGHT, outline=INK, width=4)
        if i < secured: module(draw, (x+dx*scale, y-147*scale), 17*scale)
        else: draw.ellipse(box(dx-8, -155, dx+8, -139), fill="#C9D2C3")


def person(draw, center, label, face, color=PURPLE):
    x, y = center
    draw.ellipse((x-38, y-38, x+38, y+38), fill=color, outline=INK, width=5)
    text(draw, center, label, face, LIGHT, "mm")
    draw.rounded_rectangle((x-55, y+45, x+55, y+86), radius=18, fill=color, outline=INK, width=4)


def panel(variant, shot, bold, regular):
    image = Image.new("RGB", (W, H), PAPER); draw = ImageDraw.Draw(image)
    text(draw, (110, 62), "THOLUS  /  MISSIONE SCINTILLA", font(bold, 34))
    draw.rounded_rectangle((1640, 54, 1810, 110), radius=25, fill=INK)
    text(draw, (1725, 82), f"{shot+1:02d} / 06", font(bold, 29), LIGHT, "mm")
    headings = [
        ["La rete dei fari", "è fuori uso."], ["Recuperate", "tre moduli."],
        ["Un rivale.", "Cinque colpi."] if variant == "C1" else
        ["Apri lo scudo.", "Coordina il colpo."] if variant == "C2" else ["Apri lo scudo.", "Coordinate i colpi."],
        ["Rompi il guscio.", "Recupera il modulo."], ["Proteggete il carico.", "Seguite l’allarme."],
        ["Primi al traguardo.", "Con tutti e tre i moduli."]]
    captions = [
        "Riaccendetela portando tre moduli Relay al traguardo.", "Ogni modulo occupa uno slot sul vostro kart.",
        "In solitaria, concentra cinque colpi sullo stesso rivale." if variant == "C1" else
        "Un supporter mantiene il fascio. L’altro colpisce." if variant == "C2" else
        "Un supporter mantiene il fascio. Altri due colpiscono.",
        "Prima spara. Poi tieni il fascio per 0,6 secondi.",
        "Difendi il settore indicato. Recupera il modulo sul cavo.",
        "Vincere la gara e consegnare il carico: servono entrambi."]
    notes = ["Una squadra, un kart, una missione.", "Gli slot pieni viaggiano con voi.",
             "Ciascuno può usare sparo e fascio: coordinate le azioni.",
             "Anche una sola persona può aprire e raccogliere la capsula.",
             "Sul cavo: 3 secondi per recuperare, con 0,6 secondi di fascio.",
             "Un modulo ancora sul cavo non conta come consegnato."]
    for row, line in enumerate(headings[shot]):
        face = font(bold, 78)
        assert draw.textlength(line, font=face) <= 1700
        text(draw, (110, 155+row*90), line, face)
    draw.rounded_rectangle((108, 382, 1812, 855), radius=38, fill=LIGHT, outline="#D7DDC9", width=3)
    if shot == 0:
        for x in (420, 960, 1500): beacon(draw, (x, 625), scale=1.12)
        for x in range(520, 1400, 90): draw.line((x, 728, x+35, 728), fill=MUTED, width=6)
        orb(draw, (960, 490), 56)
    elif shot == 1:
        kart(draw, (960, 690), scale=1.55)
        for x in (400, 1520): module(draw, (x, 610), 65)
        arrow(draw, (505, 610), (685, 610)); arrow(draw, (1415, 610), (1235, 610))
        text(draw, (960, 833), "3 SLOT  /  3 MODULI", font(bold, 36), INK, "mm")
    elif shot == 2:
        draw.polygon([(960, 440), (1075, 490), (1060, 635), (960, 710), (860, 635), (845, 490)], fill="#9DDBD0", outline=TEAL, width=9)
        kart(draw, (960, 660), scale=.73, show_slots=False)
        if variant == "C1":
            for i in range(5):
                x=450+i*245; draw.ellipse((x-33, 770, x+33, 836), fill=RED, outline=INK, width=4)
                text(draw, (x, 803), str(i+1), font(bold, 35), LIGHT, "mm")
            person(draw, (400, 555), "A", font(bold, 35), RED)
            arrow(draw, (475, 555), (810, 555), RED)
            text(draw, (1460, 575), "5 COLPI", font(bold, 53), RED, "mm")
        else:
            person(draw, (350, 570), "A", font(bold, 35), TEAL)
            for dy in (-12, 12): draw.line((425, 570+dy, 825, 570+dy), fill=TEAL, width=7)
            text(draw, (350, 740), "FASCIO", font(bold, 36), TEAL, "mm")
            labels = [(1480, 530, "B")] if variant == "C2" else [(1450, 505, "B"), (1560, 690, "C")]
            for x,y,label in labels:
                person(draw, (x,y), label, font(bold, 35), RED if label=="B" else PURPLE)
                arrow(draw, (x-70,y), (1115,y), RED if label=="B" else PURPLE, 10)
            text(draw, (1460, 805), "1 COLPO" if variant=="C2" else "2 PERSONE, 2 COLPI", font(bold, 34), INK, "mm")
    elif shot == 3:
        capsule(draw, (430, 580)); capsule(draw, (960, 580), opened=True); kart(draw, (1510, 688), 1, .93)
        arrow(draw, (565, 580), (800, 580), RED); arrow(draw, (1090, 580), (1310, 580), TEAL)
        for x,label,color in ((430,"1. SPARO",RED),(960,"2. FASCIO · 0,6 s",TEAL),(1510,"MODULO AL SICURO",INK)):
            text(draw, (x, 794), label, font(bold, 31), color, "mm")
    elif shot == 4:
        draw.rounded_rectangle((230, 445, 865, 570), radius=24, fill=INK)
        for i in range(3):
            left=245+i*202; draw.rounded_rectangle((left,460,left+190,555), radius=14, fill=RED if i==1 else MUTED)
        text(draw, (547, 510), "!", font(bold, 68), LIGHT, "mm")
        text(draw, (547, 624), "DIFENDI IL SETTORE", font(bold, 33), INK, "mm")
        draw.polygon([(548,670),(608,698),(597,768),(548,798),(499,768),(488,698)], fill=TEAL, outline=INK, width=5)
        kart(draw, (1250, 697), 2, .9)
        draw.line([(1350,625),(1440,595),(1540,570)], fill=INK, width=8)
        module(draw, (1550,560), 53, GOLD)
        draw.arc((1470,480,1630,640), 200, 485, fill=RED, width=9)
        text(draw, (1550, 748), "3 s", font(bold, 65), RED, "mm")
    else:
        for x in range(260, 1700, 80):
            for row in range(2):
                if (x//80+row)%2==0: draw.rectangle((x,420+row*40,x+80,460+row*40), fill=INK)
        draw.rounded_rectangle((350,575,650,790), radius=28, fill=GOLD, outline=INK, width=7)
        text(draw, (500, 683), "1", font(bold, 142), INK, "mm")
        kart(draw, (1170, 705), 3, 1.1)
        text(draw, (1170, 820), "3 / 3 CONSEGNATI", font(bold, 35), TEAL, "mm")
    caption_face=font(regular, 42)
    assert draw.textlength(captions[shot], font=caption_face) <= 1700
    text(draw, (110, 904), captions[shot], caption_face)
    note_face=font(regular, 30)
    assert draw.textlength(notes[shot], font=note_face) <= 1700
    text(draw, (110, 975), notes[shot], note_face, MUTED)
    for i in range(6): draw.rounded_rectangle((1535+i*46,1017,1564+i*46,1026), radius=4, fill=TEAL if i<=shot else "#C6D2C0")
    label="1 SUPPORTER" if variant=="C1" else "2 SUPPORTER" if variant=="C2" else "3–5 SUPPORTER"
    text(draw, (110, 1020), label+"  /  GUIDA PRIMA DELLA GARA", font(bold, 23), MUTED)
    return image, " ".join(headings[shot])+" "+captions[shot]+" "+notes[shot]


def crew_roles_for_variant(variant):
    if variant == "C1": return ["Universale"]
    if variant == "C2": return ["Universale", "Universale"]
    if variant != "C3plus": raise ValueError("unknown crew variant")
    return ["Hacker", "Cannoniere", "Difensore", "Ingegnere", "Ricognitore"]


def crew_panel(variant, shot, bold, regular, party):
    """Final SC5 art; six self-contained panels, never a Party-labelled SC4 copy."""
    base_shot = (0, 1, 2, 4, 1, 5)[shot]
    image, _ = panel(variant, base_shot, bold, regular)
    draw = ImageDraw.Draw(image)
    draw.rectangle((100, 144, 1820, 365), fill=PAPER)
    draw.rectangle((100, 876, 1820, 1008), fill=PAPER)
    headings = [("Una squadra, un kart.", "Tre moduli al traguardo."),
                ("Il kit è universale.", "Tutti possono agire.") if variant in ("C1","C2") else
                ("Le competenze ruotano.", "Tutti possono agire."),
                ("Segui il suggerimento.", "Mira al bersaglio reale."),
                ("Allarme: difendi.", "Carico sul cavo: recupera."),
                ("Cambio pilota.", "Puntatore verificato, poi Pronto."),
                ("Turbo crew.", "Il pilota può frenare.") if party else
                ("Primi al traguardo.", "Con tutti e tre i moduli.")]
    captions = ["Vincete la gara e consegnate tre moduli Relay.",
                "Nome, colore e competenza aiutano a coordinarsi.",
                "Sparo e fascio restano disponibili a ogni supporter.",
                "Mantieni il fascio sul settore o sul modulo indicato.",
                "Confermate il cambio. Verificate il nuovo possessore.",
                ("1 persona mantiene" if variant == "C1" else "2 persone distinte mantengono") + " il nodo per 0,75 s." if party else
                "Il successo richiede vittoria e carico completo."]
    notes = ["I moduli si aprono con lo sparo; si raccolgono con 0,6 s di fascio.",
             "Una competenza è un suggerimento: non blocca nessuna azione.",
             "Cinque colpi da soli; in squadra, aprite e combinate i colpi.",
             "Per il cavo restano 3 s. Dopo un'interruzione, rilascia e ripremi.",
             "Lasciate entrambi i pulsanti. Dopo la preparazione, il pilota preme Pronto.",
             "Costo 15; una volta per turno; consenso entro 3 s; solo nelle zone ammesse." if party else
             "Nel cambio, verificate la persona e il puntatore che riceve." if variant in ("C1","C2") else
             "Al prossimo cambio, anche le competenze passano alla persona successiva."]
    if shot in (1, 4) or (shot == 5 and party):
        draw.rounded_rectangle((108, 382, 1812, 855), radius=38, fill=LIGHT, outline="#D7DDC9", width=3)
    if shot == 1:
        roles = crew_roles_for_variant(variant)
        for i, role in enumerate(roles):
            x = 960 + (i - (len(roles)-1)/2) * 295
            person(draw, (x, 528), chr(65+i), font(bold, 35), [TEAL, RED, PURPLE, GOLD, INK][i])
            text(draw, (x, 680), role, font(bold, 30), INK, "mm")
        text(draw, (960, 790), "SPARO  +  FASCIO  +  DIFESA  +  RACCOLTA", font(bold, 35), TEAL, "mm")
    elif shot == 4:
        for x, value, label in [(400, "1", "CONFERMA CAMBIO"), (960, "2", "VERIFICA PUNTATORE"), (1520, "3", "PILOTA: PRONTO")]:
            draw.ellipse((x-64, 455, x+64, 583), fill=TEAL, outline=INK, width=5)
            text(draw, (x, 519), value, font(bold, 65), LIGHT, "mm")
            text(draw, (x, 679), label, font(bold, 28), INK, "mm")
        arrow(draw, (515, 520), (815, 520)); arrow(draw, (1075, 520), (1375, 520))
        text(draw, (960, 790), "PULSANTI RILASCIATI  /  GIOCO IN ATTESA", font(bold, 33), MUTED, "mm")
    elif shot == 5 and party:
        count = 1 if variant == "C1" else 2
        for i in range(count):
            y = 520 + i*185
            person(draw, (360, y), chr(65+i), font(bold, 35), TEAL if i == 0 else PURPLE)
            arrow(draw, (440, y), (710, 610), TEAL if i == 0 else PURPLE)
        draw.ellipse((755, 470, 1045, 760), fill=PURPLE, outline=INK, width=7)
        text(draw, (900, 565), "TURBO CREW", font(bold, 34), LIGHT, "mm")
        text(draw, (900, 640), "15 ENERGIA", font(bold, 38), LIGHT, "mm")
        draw.rounded_rectangle((1210, 480, 1690, 740), radius=32, fill=RED, outline=INK, width=6)
        text(draw, (1450, 560), "FRENO PILOTA", font(bold, 37), LIGHT, "mm")
        text(draw, (1450, 650), "VETO", font(bold, 60), LIGHT, "mm")
        text(draw, (960, 820), "SCADUTO O INTERROTTO? RILASCIA E RIPREMI.", font(bold, 30), INK, "mm")
    for row, line in enumerate(headings[shot]):
        assert draw.textlength(line, font=font(bold, 70)) <= 1700
        text(draw, (110, 155+row*90), line, font(bold, 70))
    for y, line, size, color in [(904, captions[shot], 38, INK), (975, notes[shot], 28, MUTED)]:
        assert draw.textlength(line, font=font(regular, size)) <= 1700
        text(draw, (110, y), line, font(regular, size), color)
    draw.rounded_rectangle((1640, 54, 1810, 110), radius=25, fill=INK)
    text(draw, (1725, 82), f"{shot+1:02d} / 06", font(bold, 29), LIGHT, "mm")
    for i in range(6): draw.rounded_rectangle((1535+i*46,1017,1564+i*46,1026), radius=4, fill=TEAL if i<=shot else "#C6D2C0")
    return image, " ".join(headings[shot])+" "+captions[shot]+" "+notes[shot]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT/"data/immersive_relay/scintilla/intro_it_v1")
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--crew-profile", choices=("sc5_crew_standard_v1", "sc5_crew_party_v1"))
    parser.add_argument("--font-bold", type=Path, default=Path("/System/Library/Fonts/Supplemental/Arial Bold.ttf"))
    parser.add_argument("--font-regular", type=Path, default=Path("/System/Library/Fonts/Supplemental/Arial.ttf"))
    args=parser.parse_args()
    if not SSD.is_mount(): raise SystemExit("The SSD must be mounted; no internal-disk fallback.")
    for folder in (args.output,args.work_dir):
        if not folder.resolve().is_relative_to(SSD.resolve()): raise SystemExit("Outputs must remain on the SSD.")
        folder.mkdir(parents=True,exist_ok=True)
    ffmpeg=shutil.which("ffmpeg")
    if not ffmpeg: raise SystemExit("Offline ffmpeg authoring tool unavailable.")
    manifest={"contract":"tholus.stk-scintilla-intro/1","schemaVersion":1,"profileId":"sc4_cargo_three_slots_v1",
              "profileRevision":"1","locale":"it","durationSeconds":40,"frameRateNumerator":30,
              "frameRateDenominator":1,"frameCount":1200,"width":W,"height":H,
              "license":"CC-BY-4.0","author":"Immersive Relay contributors (Codex-assisted)","variants":[]}
    if args.crew_profile:
        manifest.update(contract="tholus.stk-scintilla-intro/2", schemaVersion=2, profileId=args.crew_profile,
                        crewRulesRevision="1", layoutProfile="front_rear_crew_v3", layoutRevision="3")
    for variant,minimum,maximum in VARIANTS:
        directory=args.output/variant; directory.mkdir(exist_ok=True)
        panels=[]; segments=[]; subtitles=["WEBVTT", ""]
        for shot in range(6):
            image,caption=crew_panel(variant,shot,args.font_bold,args.font_regular,args.crew_profile=="sc5_crew_party_v1") if args.crew_profile else panel(variant,shot,args.font_bold,args.font_regular)
            target=directory/f"panel_{shot+1:02d}.png";image.save(target,optimize=True)
            panels.append({"path":str(target.relative_to(args.output)),"sha256":sha(target)})
            start,end=CUTS[shot]//30,CUTS[shot+1]//30; duration=end-start
            subtitles += [f"{shot+1}",f"00:{start//60:02d}:{start%60:02d}.000 --> 00:{end//60:02d}:{end%60:02d}.000",caption,""]
            segment=args.work_dir/f"{variant}_{shot+1:02d}.mp4"; segments.append(segment)
            subprocess.run([ffmpeg,"-hide_banner","-loglevel","error","-y","-loop","1","-framerate","30","-i",str(target),
                            "-vf",f"fade=t=in:st=0:d=0.25:color={PAPER.replace('#','0x')},fade=t=out:st={duration-.25}:d=0.25:color={PAPER.replace('#','0x')}",
                            "-frames:v",str(CUTS[shot+1]-CUTS[shot]),"-c:v","libx264","-preset","medium","-crf","18",
                            "-pix_fmt","yuv420p","-g","60","-an",str(segment)],check=True)
        video=directory/"intro_it.mp4"; vtt=directory/"intro_it.vtt";vtt.write_text("\n".join(subtitles),encoding="utf-8")
        listing=args.work_dir/f"{variant}.ffconcat"
        listing.write_text("ffconcat version 1.0\n"+"\n".join("file '"+str(p).replace("'","'\\''")+"'" for p in segments)+"\n")
        subprocess.run([ffmpeg,"-hide_banner","-loglevel","error","-y","-f","concat","-safe","0","-i",str(listing),
                        "-c","copy","-movflags","+faststart",str(video)],check=True)
        manifest["variants"].append({"variantId":variant,"minCrew":minimum,"maxCrew":maximum,
            "videoPath":str(video.relative_to(args.output)),"videoSHA256":sha(video),
            "subtitlesPath":str(vtt.relative_to(args.output)),"subtitlesSHA256":sha(vtt),"panels":panels})
    path=args.output/"manifest.json";path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"manifest":str(path),"sha256":sha(path),"variants":3,"panels":18,"frames_per_variant":1200}))


if __name__=="__main__": main()
