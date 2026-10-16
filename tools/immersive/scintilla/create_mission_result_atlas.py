#!/usr/bin/env python3
"""Author the Italian SC4 result atlas. No runtime text generation or font bundle."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont
TEXTS = [
    'Missione compiuta: primi, con tutti i moduli.',
    'Gara vinta. Consegna incompleta.',
    'Carico consegnato. Primo posto mancato.',
    'Missione non completata.',
    'Sessione interrotta. Missione non completata.',
]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--font',type=Path,default=Path('/System/Library/Fonts/Supplemental/Arial Bold.ttf'))
    p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[3]/'data/immersive_relay/scintilla/mission_results_it_v1.png')
    a=p.parse_args();font=ImageFont.truetype(str(a.font),68);label=ImageFont.truetype(str(a.font),34)
    image=Image.new('RGBA',(2048,800),(0,0,0,0));draw=ImageDraw.Draw(image)
    for row,text in enumerate(TEXTS):
        box=draw.textbbox((0,0),text,font=font);assert box[2]-box[0]<=1920
        draw.text((1024,row*160+56),text,font=font,anchor='mm',fill=(255,255,255,255))
        draw.text((600,row*160+128),'Posizione',font=label,anchor='mm',fill=(255,255,255,255))
        draw.text((1400,row*160+128),'Moduli',font=label,anchor='mm',fill=(255,255,255,255))
    a.output.parent.mkdir(parents=True,exist_ok=True);image.save(a.output)
    print(a.output)
if __name__=='__main__':main()
