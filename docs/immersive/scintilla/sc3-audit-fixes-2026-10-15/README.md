# SC3 — Quattro correzioni dell'audit

15 ottobre 2026. Correzioni software dei quattro finding SC3, affrontate e
ricontrollate nell'ordine 1→2→3→4. Nessuna build/configure/typecheck, esecuzione
nativa, avvio o prova hardware. SC4 non avviata. [Piano](../sc3-plan-2026-10-15/PIANO_SC3.md).

## Comportamenti corretti

1. **Handoff operatore.** La conferma conserva il grant SI5 come il percorso
   telefono. Solo la transizione autoritativa sospende le associazioni logiche.
   Un comando rifiutato non chiude i dispositivi. Regressione controller con
   osservazione del vero confine di invalidazione, accettazione e rifiuto.
2. **Socket.** La destinazione è calcolata prima della query. Un helper Bullet
   condiviso da produzione e test verifica tutto il segmento, con sfera r=.122 m
   conservativa e controllo overlap a entrambi gli estremi; conserva esclusioni
   sorgente/umano e no-contact-response. Il raggio del corpo reale e la soluzione
   balistica definitiva restano verificati. Dichiarato anche `interpolate` nel
   relativo header: il consumer fisico lo chiama da una translation unit diversa.
   Quattro casi Bullet: ostacolo intermedio statico/dinamico fuori dal ray centrale,
   overlap iniziale/finale, esclusioni e segmenti nulli/non finiti.
3. **SC2 durante allarme.** Le prove `WorldOffense` usano threat contestuale zero:
   una nuova minaccia non interrompe mira/bersaglio/controlEpoch invariati, né
   rende ambiguo un intervallo ottico che attraversa la nascita dell'allarme.
   Nodi difesa/boost restano vincolati alla minaccia; cambi target, contesto e
   negativi persistenti conservano il riarmo obbligatorio. Due regressioni consumer.
4. **Nuova gara.** Il reset in-place SC3 conserva il grant fisico e il trasporto;
   annulla gameplay, held/pending, vecchie prove e query. Anche con crew identica
   richiede rebind → SC2 → SC3 prima di Ready. La nuova raceEpoch ricarica il core
   come previsto (E100/H0); pausa/handoff non ricaricano. Un vecchio update ottico
   già in volo fallisce in modo recuperabile. Il riuso conserva sequenze,
   anti-replay, clock e lease: un grant scaduto/revocato non viene riattivato.
   Il normale reset conserva stream/frameId monotoni dello stesso processo;
   distruzione/ricreazione o cambi fisici restano invalidazioni complete.

Il reset da Pronto nei risultati è consentito a video held perché esegue solo
reset. L'avvio continua a richiedere tutti i gate. Host e server mantengono lo
stato sospeso in caso di rifiuto o risposta persa e richiedono preparazione
esplicita. Anche forza fine/annulla mantengono il grant SC3 per la gara successiva.
Il polling osserva reset esterni tramite raceEpoch. Un preflight aggiorna tale
autorità prima di fissare una nuova operazione di preparazione. Il percorso
`prepareNextSession` riusa una configurazione già confermata dopo il reset.

## Ricontrollo ed evidenze

- Lettura dei chiamanti reali: pannello operatore, telefono, router HTTP, reset
  World/RaceManager, hook fisici, consumer e transazione di preparazione host.
- Revisione indipendente mirata dei fix STK e revisione del lifecycle host.
- Controlli Python di sorgenti/geometria: **44** casi complessivi (6 depth,
  6 volume SC2, 10 combat, 6 feedback, 16 difesa), tutti superati.
- Test nativi scritti/registrati: consumer **80** casi (6 nuovi), Bullet **10**
  casi (4 nuovi), regressioni controller Swift: **11** nuove varianti per handoff, reset,
  risposte incerte, polling e fine/annulla→reset.
  **Non compilati né eseguiti.** Lo spy del controller prova il confine di teardown;
  non simula né certifica un grant SI5 fisico o una proiezione funzionante.
- `git diff --check`, localizzazione en/it e hash di consegna: dettagli in
  [verification.json](verification.json) e [source-manifest.json](source-manifest.json).
  [changes.patch.txt](changes.patch.txt) conserva il delta rispetto al WIP iniziale.

I controlli Python non eseguono C++, Swift, GLSL o Bullet. Rimangono da qualificare
compilazione, regressioni native, receiver/clock/stream durante reset e handoff,
GPU, dispositivi e flusso umano completo. Le correzioni sono consegnate a livello
sorgenti, senza dichiarazione di perfezione runtime.
