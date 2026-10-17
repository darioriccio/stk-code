# SC5 — Implementazione e riesame software

Data: **16 ottobre 2026**. Riferimento: [PIANO_SC5.md](../sc5-plan-2026-10-16/PIANO_SC5.md), contratto e tutti gli allegati della stessa cartella.

**Implementati e ricontrollati i passi software 0–10. Nessuna build, configurazione CMake, compilazione, typecheck, esecuzione C++/Swift, avvio app/gioco, prova GPU o hardware. SC6 non avviata.** Il passo 11 di qualifica nativa resta aperto. Questa consegna non dichiara SC5 pronta per l'impiego in sala.

Il [contratto implementato](CONTRATTO_IMPLEMENTATO.md) descrive i campi effettivi, le estensioni report/qualifica e i gate adottati; la proposta del piano resta conservata.

Le modifiche sono consegnate nei repository:

- STK: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code`.
- Host: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController`.

## Lavoro per fase

| Passo | Risultato nel codice | Riesame svolto |
|---|---|---|
| 0 | Baseline SC0–SC4 importata con hash, compreso WIP non committato, in due worktree isolati sul SSD | HEAD non assunto equivalente al checkout; 56 fingerprint del piano verificati prima delle modifiche; stato Git e autorizzazioni conservati |
| 1 | Core/contratto SC5, identità a 13 dimensioni, profili standard e Party, DTO chiusi e ricevute bounded64 | Canonicalità uint64, campi sconosciuti, replay esatto prima del confronto live, compatibilità SC4 e capability esplicite |
| 2 | Competenze consigliate e kit universale, turno derivato esclusivamente dal cambio nativo realmente completato | P2–6, C1–2 universali, modulo senza overflow a UInt64 max, anomalie amministrative, reset/reprepare |
| 3 | Opportunità da fatti/prove correnti, occupazioni reali, scheduler bounded e crediti su commit dei core proprietari | Massimo 7 bundle, 5 mani, 15.360 assegnamenti; 4 query GPU e 32 fatti rivale; attesa saturata/int64; nessun potere esclusivo per ruolo |
| 4 | Handoff A nativo e B di preparazione; proposta biiettiva, associazioni visibili, verifica possesso nel pump SI5 unico | Gate distinti per avanzamento cambio e gameplay; 200ms di neutralità, aim fresco, nuovo gesto completo, timeout30s; continuità fisica e grant sano preservati |
| 5 | Party defaultoff, quorum1/2 persone, hold0,75s, finestra3s, quota per turno e arbitraggio con boost SC3 | Deadlines esclusive e tick originali attraverso query pendenti; negative sticky; normal boost prioritario; costo15/riserva/cooldown solo su successo fisico |
| 6 | Impulso tangenziale Party dopo Bullet, cap totale e freno pilota; gate di localizzazione e zone | Controesempio t4/n3, conversioni float/double, nessun floor o modifica angolare, lease freno locale, veto, esclusione stacking; test fisici nativi ancora aperti |
| 7 | Layout v3 CPU/shader/history/qualification, feedback, atlas testo bounded, due intro distinte | Maschera Party riservata anche standard, costo prima dell'uso, feedback SC1–4 conservato, trace/report v2 per SC5, manifest/2 e sei video reali con sei pannelli per variante |
| 8 | Flow host di produzione intent→preflight→intro→SC1→possesso→SC2/3/4→bindings-confirm→SC5→Ready | Identità ricontrollata ai confini async, discovery prima di Ready, retry/cancellazioni, uscita preview, nuova gara sullo stesso grant, Party fermato prima del challenge senza zone qualificate |
| 9 | Report congelato, serializer unico GET/report/file, report UI/proiezione, bucket separati e strumenti zone | Tuple SC4 esatte, nessun credito tardivo, Party resta Party anche con zero usi, abort/reset/nuova gara e handoff→abort prima della successiva osservazione engine; proof reali richiesti per qualifica |
| 10 | Registrazione test, controlli Python/statici, matrice64 e consegna selettiva | [Matrice e verifiche aperte](MATRICE_VERIFICHE.md), [registro finale](registration-final.json), igiene sorgenti, catalogo e hash di trasferimento |
| 11 | Non eseguito: qualifica nativa geometria/zone e ciclo completo | Build/test app e nativi, GL/font/readback, freno/Bullet, due cicli completi di handoff, fine/reset e prove in sala richiedono un incarico autorizzato |

## Superfici principali

Nel repository STK, `src/modes/immersive_relay/scintilla_crew.*` contiene il core puro; `scintilla_crew_protocol.hpp`, `scintilla_crew_result*.hpp` e `scintilla_crew_report_file.hpp` fissano protocollo e rapporto. I file `scintilla_crew_boost_math.hpp`, `scintilla_crew_physics.*`, `scintilla_crew_zones.*` separano matematica, commit fisico e gate delle zone. Gli include `spatial/scintilla_crew_{adapter,services,wire,zones_native,text_native}.hpp` collegano questi moduli all'engine senza un altro consumer o server.

Nell'host, `Modules/GameShowControl/ImmersiveSTKCrewPreparationFlow.swift` è il flow effettivamente chiamato dal Runtime e condiviso dai test con trasporto/clock controllati. `ImmersiveSTKSpatialInputSession.swift` espone il confine minimo dell'adattatore. Il package `Packages/TholusScintillaMission` contiene identità/DTO/possesso/risultato puri. `Workspace/GamesSTKSpatialInputView.swift` e `GamesSTKCrewResultView.swift` espongono associazioni, preparazione e contributi. Il catalogo riceve esclusivamente le 51 chiavi SC5 nuove, preservando le modifiche concorrenti agli altri testi.

Il report conserva parametri effettivi, frequenza fisica, nove categorie di contributo e cinque metriche di tempo in tick/F. Il controller usa una copia tipizzata e la propria rotazione quando deve congelare un abort immediatamente dopo il cambio pilota: nessuna callback nell'engine sotto il suo mutex. Archivio con nome producer/race/result e creazione esclusiva, senza sovrascrivere rapporti precedenti. L'esito e il criterio di vittoria SC4 restano invariati.

## Controlli effettivamente eseguiti

[software-checks-final.json](software-checks-final.json) contiene comandi, cwd, durate ed esiti. I log `*-final.txt` conservano gli output. Tutti gli esiti sotto sono **Python offline, lettura sorgenti o controllo asset**, senza esecuzione del codice C++/Swift implementato.

| Controllo | Esito |
|---|---|
| Modello matematico del piano | 57.925 asserzioni passate; è verifica del modello, non prova del runtime |
| Validator qualifica / collector / intro / zone Party | 37 + 10 + 3 + 9 = **59 test Python passati**, fixture dichiarate sintetiche |
| Projection, crew physics/wire/render/report, SC4 HUD/API/finish/lifecycle/presentation | **123 controlli sorgente/asset passati**, distinti dai test funzionali nativi |
| Intro standard e Party C1/C2/C3plus | Sei MP4 1920×1080, H.264 yuv420p, 30fps, 1.200 frame/40s, decodificati integralmente; 36 PNG e sei VTT verificati contro manifest/hash |
| Localizzazione host | 1.671 file Swift, **zero nuove segnalazioni**; testi nuovi EN/IT |
| Registri CTest e runner app | Parità sorgente/registri verificata senza configurare CMake né caricare Swift |
| Igiene | `git diff --check` nei due worktree; nuove righe senza whitespace finale/conflitti; AST dei 13 Python toccati e JSON validi |

[phase7-software-verification.md](phase7-software-verification.md) e i due rapporti intro descrivono gli strumenti di authoring/decodifica utilizzati. Non è stata avviata la riproduzione nell'app. L'encoding/decoding offline degli asset non costituisce una build del progetto.

Test nuovi **scritti/registrati, non eseguiti**: 71 casi nei sette target C++ crew, 18 casi crew nel registro input esistente, quattro casi servizi Party nella difesa, un caso manifest intro e uno measurement v3 nei target missione. Nel package Swift ci sono 40 metodi crew; nell'app nove metodi HTTP e 20 ulteriori metodi di flow/orchestration/report con 39 invocazioni parametrizzate per questi ultimi. I registri conservano le suite precedenti. Numero di righe/casi e sourcecheck non dimostrano che compilino o passino.

## Zone Party e qualifica

`data/immersive_relay/scintilla/snowtuxpeak_party_zones_v1.json` conserva i sei candidati 6,17,26,45,59,71 come **development_unqualified**, con `zones: []`. Il +1m/s, le distanze di arresto illustrative e la curvatura sparsa non certificano una zona né il comfort umano. Il profilo Party è quindi implementato ma **non ammesso operativamente** dai gate finché mancano zone e qualifica reali; nessun fallback promuove dati sintetici.

Gli strumenti e le istruzioni per completare la qualifica sono [QUALIFICA_SC5.md](../../../../tools/immersive/scintilla/QUALIFICA_SC5.md) e [QUALIFICA_ZONE_PARTY_SC5.md](../../../../tools/immersive/scintilla/QUALIFICA_ZONE_PARTY_SC5.md). Si congela prima il manifest authored; le prove successive ne legano hash, eseguibile, asset pista, FPS e artefatti nativi. Il loader nativo rilegge e verifica gli artefatti proof prima di ammettere la qualifica. Il validator verifica schema/provenienza dichiarata/integrità, non può dimostrare da solo che un operatore abbia realmente eseguito una prova.

Anche il profilo standard richiede nuove misure native di layout v3. Descriptor v2/SC4 non viene rinominato o accettato come qualifica v3. Disponibilità ottica `[readbackUs,captureUs+100000]` dal 17 ottobre (prima 50000); buchi e prefetch futuro non diventano prove ammesse.

## Limiti e consegna

Restano da eseguire build relay/vanilla, test C++ e Swift registrati, avvio host/native, GL compile/link e font atlas, prove GPU, freno e interazioni pad/nitro/pendenze, ciclo operativo con due rotazioni complete, quattro esiti naturali, reset e qualifica di geometria/zone. Possesso biologico, tracking/calibrazione reali, leggibilità, comfort e divertimento richiedono hardware e persone; nessuna fixture software li certifica.

Non sono state modificate impostazioni globali, pubblicate revisioni o avviate fasi successive. I sorgenti SC5 sono stati sviluppati isolatamente rispetto al WIP e trasferiti con confronto SHA256 dei soli file del task; il catalogo è riconciliato per chiave. Il [manifest di consegna](source-manifest.json) descrive il delta rispetto alla baseline WIP iniziale, non rispetto al solo HEAD Git. Gli artefatti temporanei di authoring e i worktree isolati vengono rimossi dopo la verifica della consegna; documenti, sorgenti, test, asset finali ed evidenze sono conservati.
