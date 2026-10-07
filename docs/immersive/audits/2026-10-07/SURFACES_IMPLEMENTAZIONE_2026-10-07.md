# SuperTuxKart: implementazione delle superfici immersive

Data: 7 ottobre 2026. Richiesta: implementare `PLAN (4).md`, senza build.
Stato: sorgenti implementati; controlli statici eseguiti; compilazione, ricezione
Syphon e qualifica in sala NONRUN. Le applicazioni installate non sono aggiornate
con questo intervento. Nessun commit o push.

## Risultato

STK produce dalla scena 3D un panorama equirettangolare nativo 360×180°, 2:1,
usando le sei facce e una proiezione GPU condivisa con il percorso fisheye.
Il domemaster resta disponibile. TholusOS negozia il formato dalle capabilities,
lo riceve con identità/proprietario/generazione espliciti e lo adatta alla
superficie realmente calibrata. L'esperienza resta cooperativa: 1–6 persone,
un pilota alla volta e telefoni facoltativi; non sono state aggiunte meccaniche
o selezione piste in questa fase.

| Superficie calibrata | Percorso implementato |
| --- | --- |
| Fulldome | Domemaster automatico storico; equirettangolare selezionabile |
| Segmento di cupola | Posizione sferica metrica, porzione calibrata |
| Cilindro completo / 180° | Geometria cilindrica, angoli e osservatore della sala |
| Piani, pareti, pavimento, soffitto | Posizioni metriche relative allo stesso osservatore e riferimento |
| Mesh metrica | Triangoli qualificati e cache GPU delle posizioni relative all'osservatore |
| UV con mappa delle direzioni | Mappa qualificata per sala/superficie/revisione, riferimento e origine supportati |
| Mesh/manuale con sole UV | Vera prospettiva frontale nel dominio/triangoli della zona selezionata |
| Point cloud grezza | Esclusa dalle superfici di produzione |

Il nome della stanza non certifica la calibrazione. La stanza rettangolare deve
fornire piani coerenti o una mesh metrica per le uscite desiderate. Il descriptor
canonico deve essere utilizzabile e descrivere la copertura di tutte le uscite
selezionate. Non vengono inventate pareti o direzioni dalle sole UV.

## Contratto video e impostazioni

- Equirectangular Debug: 2048×1024, facce 512; Safe: 2048×1024, facce 768;
  Target esplicito: 4096×2048, facce 1024. Nessuna prestazione certificata.
- Convenzione UV dall'alto: avanti(+Z) al centro; destra(+X) aU0,75;
  sinistra aU0,25; giunzione dietro; zenit V0 e nadir V1. Il flip Syphon
  esistente resta esplicito; i compositor raccordano la convenzione soltanto STK.
- CLI/API validano dimensioni prima dell'allocazione. Cambio adattatore e texture
  sul thread grafico, senza lettura della texture sulla CPU. Un errore mantiene
  le risorse precedenti dove possibile e impedisce di confermare il nuovo formato.
- Tholus espone Automatico/Domemaster/Equirettangolare, qualità, griglia,
  fronte relativo alla sala, zona principale, FOV orizzontale 90° regolabile 60–120°,
  formato richiesto/ricevuto e diagnosi. Preferenze salvate per sala, EN/IT.
- Guida, Pronto, countdown, turno successivo, punteggio e feedback usano la
  zona principale; la presentazione storica domemaster viene conservata.
  Quando la zona non mostra il fronte della guida viene usata la finestra frontale.

## Preparazione, cambi e proprietà

Il launcher di Tholus arma il gate di presentazione prima del primo Ready.
La preparazione chiude il gate, configura il rendering/sessione e carica la pista,
poi attende `configuration_applied=true` e formato/dimensioni API coerenti con
una texture Syphon recente (≤500 ms), mapping valido, proprietà e generazione.
Solo allora il gate consente Ready da tastiera/SDL/telefono e avvio/ripresa.
Un POST accettato con formato ancora pendente non autorizza la gara.

Cambio di impostazioni/calibrazione o errore di mapping invalida la preparazione,
chiude il gate e mette in pausa la gara quando applicabile. La nuova preparazione
solo video conserva sessione, punteggio e roster; la ripresa rimane esplicita.
Frame obsoleti rimuovono la readiness. Le risposte asincrone obsolete non possono
riaprire il gate. Chiusura e sostituzione del receiver rilasciano soltanto le
risorse ancora possedute dalla generazione STK.

La registrazione `supertuxkart` riutilizza il percorso live/compositor/MediaBus,
le maschere, il warp e il blending esistenti. Non cambia le convenzioni predefinite
di Stellarium, Celestia o video ordinari.

## Verifica eseguita

- Cinque test Python matematici/statici: cardinali, poli, seam, flip contrattuale,
  copertura delle sei facce e collegamenti gate/thread. Inclusi 4000 roundtrip
  del receiver matematico e 7381 punti di copertura del sampler.
- Sintassi zsh del launcher, AST degli script Python e controllo dei diff.
- Raccordo statico degli 11 campi Swift/Metal STK e slot texture della cache GPU.
- Review dei flussi di prima preparazione, riconnessione, cambio proprietario,
  invalidazione della calibrazione, perdita frame ed eventi Combine ritardati.
- Guard EN/IT differenziale rispetto ai sorgenti precedenti: zero nuove
  regressioni; 76 nuovi lookup distinti rispetto al baseline con EN/IT complete,
  e placeholder dell’intervento coerenti.
  Il guard globale restituisce 39 segnalazioni sul WIP complessivo rispetto a HEAD
  (41 prima dell’intervento); sono preesistenti, distinte dal delta.
- Test Swift predisposti e revisionati, NONRUN: profili/API, conferma pendente,
  cardinali, FOV/aspect, cupola/cilindro/piani, pavimento/soffitto, triangolo metrico
  con osservatore traslato, zona frontale troppo stretta, mappe direzionali qualificate, UV senza geometria e HUD non limitato alla cupola.
  Probe HTTP della proiezione predisposto, NONRUN.

La review del codice non dimostra la compilazione o la correttezza visiva di
shader e Syphon. L'assenza di regressioni reali con 1/6 partecipanti, telefoni
presenti/assenti e scollegamento controller resta da verificare sull'app aggiornata.
La regressione asincrona integrata della readiness non è stata eseguita né
presentata come coperta da simulazioni artificiali.

## Verifica successiva richiesta

Quando le build saranno autorizzate: una build alla volta; test mirati;
receiver reale, resize/reconnect/stale/cambio proprietà; griglia cardinale e
orientamento; superfici rappresentative e room con più pareti/pavimento/soffitto;
HUD e guida durante Ready/gara/handoff/pausa/risultati; prestazioni e 1/6 utenti
con telefoni facoltativi. Volante e pedaliera reali ancora da scegliere e collaudare.

I sorgenti consegnati e i relativi SHA256 sono elencati in
`SURFACES_MANIFEST_2026-10-07.json`. I checkout isolati sono conservati perché
contengono anche lavoro MVP precedente non committato; nessuna pulizia distruttiva.

Integrati localmente: 27 file STK e 37 file TholusOS. Le modifiche preesistenti
e le traduzioni concorrenti sono state preservate. Rimossi soltanto gli snapshot
temporanei di confronto creati per questa fase; conservati sorgenti, test e worktree.
