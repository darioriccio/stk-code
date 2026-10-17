# Qualifica geometrica SC5

Le prove SC5 usano trace `tholus.stk-scintilla-measurement/2`, schema 2,
layout `front_rear_crew_v3`. SC4 conserva trace e report `/1`, maschera v2 e
directory `qualification_v1`. Un vecchio report SC4 non ammette SC5.

Il recorder diagnostico si avvia in una gara locale held ancora senza intent
SC2–SC5, consumer preparato o grant gameplay. Il parametro opzionale di `start`
seleziona soltanto la geometria diagnostica, senza abilitare il coordinatore:

```sh
python3 tools/immersive/scintilla/collect_mission_measurement.py start \
  --producer "$PRODUCER_UUID" --race "$RACE_EPOCH" --phase shell \
  --crew-profile sc5_crew_standard_v1
```

Registrare entrambe le fasi `shell` e `open`, con gara naturale e stress nativo
misurato. Party usa `--crew-profile sc5_crew_party_v1`; le finestre devono coprire
anche l'inviluppo `nativeBaseSpeed + 1 m/s`. I sei `speedIncreaseTicks` nativi
restano invariati. Nome dello scenario e dati sintetici non sono prove native.

Fermare ed esportare ogni trace in held con i comandi `stop` ed `export` dello
stesso collector. Tenere trace e descriptor sul SSD, in directory distinte per
profilo. Il lettore nativo cerca precisamente:

| Profilo | Descriptor rispetto a `data/immersive_relay/scintilla` |
|---|---|
| `sc5_crew_standard_v1` | `qualification_v2/sc5_crew_standard_v1/report.json` |
| `sc5_crew_party_v1` | `qualification_v2/sc5_crew_party_v1/report.json` |

Generare il report con `validate_mission_qualification.py --anchors <manifest
effettivo> --trace <trace-shell> --trace <trace-open> --report <percorso sopra>`.
Il validator sceglie revisione e profilo dai trace verificati; non li promuove
rinominando un file. Standard e Party possono essere installati insieme.

Il fingerprint comprende profilo, regole SC5, arrotondamenti dei parametri
Party alla frequenza fisica effettiva, hash eseguibile/asset/shader e hash del
manifest zone. `qualification_v1` e `qualification_v2` sono esclusi dall'hash
degli asset per evitare autoreferenze. Ogni cambio di contenuto applicato
richiede nuove prove. La disponibilità delle capsule è esattamente
`[readbackUs, captureUs + 100000]` (un gap di cattura più la lettura delle
query: con 50 ms nessuna prova reale restava viva fino alla successiva); il
cavo conserva il proprio contratto. Con l'autopilota l'intera procedura è
`qualify_mission_autopilot.py --crew-profile sc5_crew_standard_v1`.

Il preflight operativo avviene dopo il latch `crew/intent`, prima della
preparazione: seleziona il report installato dello stesso profilo. La qualifica
geometrica non ammette automaticamente le zone Party, il freno, l'ottica o
l'esperienza fisica della sala. I manifest zone in authoring restano esclusi.
