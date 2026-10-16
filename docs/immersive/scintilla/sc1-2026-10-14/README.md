# Missione Scintilla SC1 — implementazione sorgente

Data: 2026-10-14. Ambito autorizzato: SC1 a livello di codice, senza build.
Design: [v2](../../../Tholus_STK_Missione_Scintilla_Design_v2.md).
Baseline storica: [SC0](../sc0-2026-10-13/README.md), conservata senza riscriverne gli esiti.

SC1 aggiunge il consumer C++ e l'adattatore TholusOS per la mira in corsa.
La consegna riguarda i sorgenti: compilazione, test C++/Swift, shader, ricezione
Syphon e percorso fisico non sono stati eseguiti. SC2 non è iniziata.

## Comportamento implementato

- Da uno a cinque puntatori indipendenti, due nel caso SC0 P=3. Ogni sorgente
  corrisponde a un partecipante approvato; l'insieme deve coincidere con tutta
  la crew corrente, escluso il pilota, indipendentemente dal tracking presente.
- Il servizio Spatial Input comune resta l'unico proprietario dell'acquisizione.
  Sono richieste roomRay6dof, primaryAction, eventTimePose e secondaryAction.
  Un backend non qualificato o privo del secondo comando viene rifiutato.
- Primary genera un impulso fino a 4 Hz; secondary mantiene il feedback tool.
  Rilascio fisico valido necessario prima del riarmo. Nessun autofire aggiunto.
- Reticoli con colore e numero di punti distinto; marker visibili sopra i kart
  avversari e i bonus box disponibili. I marker sono le primitive bersagliabili
  effettive, senza volumi invisibili più grandi. La camera continua a seguire
  il kart umano: nessun aggancio al rivale e nessun fallback al primo avversario.
- Il raggio comune viene intersecato con la superficie fisica calibrata e con
  la copertura realmente renderizzabile. Il mapping usa la trasformazione sala,
  l'osservatore e la configurazione effettiva del renderer. L'unica riflessione
  fra front-minus-Z comune e convenzione legacy è al confine dell'adattatore.
- Il resolver conserva camera, proiezione, identità/istanza e posizione dei
  bersagli per ogni frame pubblicato. Un timbro THB1 in alpha, con stream128,
  frame64 e CRC32, attraversa il percorso Syphon/Metal esistente per osservare
  la presentazione. Le righe superiore e inferiore duplicano il timbro;
  l'immagine RGB resta intatta.
- I colpi sono valutati solo con prove di presentazione che coprono tutto
  l'intervallo temporale incerto. Ogni frame plausibile deve concordare sullo
  stesso bersaglio e sulla stessa istanza ancora viva. Occlusione verificata
  contro le sei depth texture storiche, usando query GPU asincrone di un pixel;
  dati mancanti, ritardi e ambiguità producono rifiuto, mai una stima favorevole.
- Cancel, tracking perso, continuità cambiata, pausa, handoff, reset, modifica
  della route o della proiezione disarmano. Un press scaduto viene consumato
  anche quando è rifiutato: non può riapparire dopo un rinnovo degli orologi.
- Nessun danno, breccia, combo, energia, modifica ai powerup o carico missione.
  Gli esiti hit/miss sono feedback SC1, non un'autorizzazione per danni SC2.

## Ingresso host e preparazione

I sorgenti host sono in SocialTableController:
`Modules/GameShowControl/ImmersiveSTKSpatialInput.swift`, con composizione in
`ImmersiveSuperTuxKartRuntimeController.swift` e `AppRuntimeComposition.swift`.
L'API interna esplicita è
`prepareScintillaSpatialInput(bindings:projectionBinding:timing:)`.
Richiede un produttore posseduto, mondo preparato e trattenuto, crew legacy
`disabled`, numero partecipanti coerente, associazione sala/calibrazione misurata
ed evidenza temporale qualificata. Il successivo intervento di attivazione aggiunge
la sezione operatore Scintilla nella configurazione del gioco.

La preparazione abilita il produttore, ottiene il vero UUID Syphon, prepara
nuovamente l'output con il protocollo esistente `flock-gpu-v1` e richiede un
endpoint SI5 approvato `external.immersive_supertuxkart`. Le coordinate del
messaggio canonico sono preservate; il punto UV associato è un dato derivato
nel dominio STK, non una nuova modalità del driver comune.

Sono supportati equirettangolare nativo e domemaster calibrato fulldome esatto.
Il percorso domemaster rifiuta content region, finestra frontale forzata e
geometrie senza misura metrica. Non è implementata una vista posteriore separata.
Il preset iniziale resta quello scelto da SC0; nessuna qualifica a 2048 è implicita.

Correzione audit 3 (14 ottobre): la pausa mantiene endpoint, grant e rinnovi SI5.
STK disarma controlli e colpi pendenti; la ripresa richiede un rilascio acquisito
successivamente all'attivazione. Una revisione e un timestamp protetti dal mutex
del controller preservano la transizione anche fra due frame; il consumer viene
aggiornato soltanto sul game thread. Eventi acquisiti prima della ripresa vengono
consumati e rifiutati, senza riarmo o colpi ritardati.

Gli errori di preparazione consentono il retry senza riavvio soltanto se
l'endpoint prova atomicamente di non avere mai acquisito un grant e di non avere
comandi in corso. La revoca dopo un grant conserva il vincolo di uscita osservata
del produttore, anche dopo stop/handoff o errori successivi al grant. Un ACK HTTP
non costituisce prova di isolamento. Le regressioni C++ e Swift sono sorgenti
revisionati: build, esecuzione e qualifica runtime/hardware restano non eseguite.

## Estensione API autenticata

La superficie congelata precedente rimane valida. L'estensione opt-in è
`/immersive/v1/spatial/*`, contratto di dominio `tholus.stk-spatial/1`; gli input
interni mantengono `tholus.spatial-input/1`. Si usa la stessa autenticazione
locale `X-STK-Relay-Token`. Le operazioni spaziali entrano nella coda game thread;
la rete è gestita dal worker HTTP e da URLSession. La coda è limitata a 64 job.

| Metodo e suffisso | Contenuto e risultato |
|---|---|
| POST enable / disable | Oggetto vuoto; enable restituisce serverUUID reale e richiede mondo trattenuto con crew legacy disabilitata. |
| GET clock | serverReceivedUs/serverSentUs del clock monotono STK. |
| POST prepare | Roster canonico, bindings, issuedAtHostUs, almeno tre clockSamples, routeRevision, projection e inputUncertaintyUs. |
| POST update | Un batch limitato a 32 input, con renew/presentation opzionali elaborati prima degli input. Restituisce esiti individuali e nuovi timestamp clock. |
| POST input | Messaggio canonico con point UV opzionale associato a sourceId/sequence/projectionContextId e viewId main. |
| POST presentation | Sessione/generazione, frameStream, timingRevision, proofFromUs/proofUntilUs e runs dei frame presentati. |
| POST renew / release | Rinnovo vincolato a clockSamples e scadenza canonica; revoca della sessione. |
| GET state | Stato, motivo dell'ultimo arresto e journal limitato degli esiti. |

Prepare valida il consumer, l'insieme esatto dei partecipanti, capabilities,
identità complete delle sorgenti, proiezione reale del produttore e lease.
Il parser chiuso rifiuta campi sconosciuti, chiavi duplicate anche dopo escape,
non-finiti, valori fuori intervallo, UTF-8 non valido e payload oltre i limiti.
Le sequenze sono monotone per tipo; gli eventId sono deduplicati nella sessione.
Clock a quattro timestamp: almeno tre probe distinti e ordinati, RTT massimo
50 ms, intervalli conservativi con deriva 100 ppm; rinnovi incompatibili vengono
rifiutati. L'host applica un limite iniziale più stretto di 20 ms.

L'intero intervallo evento deve avere età <=60 ms e incertezza <=20 ms; la
misura ottica input deve essere <=5 ms. Sono limiti di ammissione, non prestazioni
misurate. Il feedback può attendere la depth asincrona fino a 250 ms: questo
ritardo non amplia l'età ammissibile e non definisce una futura finestra SC2.

Le risorse sono limitate: metadata 64 frame, fino a 12 storie depth entro
192 MiB di budget logico, 64 query/pending, massimo otto frame plausibili per
colpo e 256 esiti. Gli UUID recenti restano per un secondo dalla ricezione con
quota di 512 per sorgente; la sequenza per sorgente protegge dal replay per
l’intera sessione. La saturazione disarma solo la sorgente interessata. L'output usa fence GPU e
flock non bloccanti per evitare copie mentre il consumer legge la superficie.
Le prestazioni e l'occupazione effettiva del driver GPU restano da misurare.
I lock runtime seguono il protocollo condiviso già esistente nella directory
Application Support/TholusOS/FrameLeases; questa sessione non li ha creati.
Worktree, temporanei ed evidenze della lavorazione sono stati sul SSD esterno.

## Aggiornamento del sorgente — 15 ottobre 2026

La [correzione dei problemi residui](../sc1-remediation-2026-10-15/README.md)
descrive confronto ottico nel clock host, replay limitato, ammissione basata
sugli storici realmente disponibili e feedback distinti dell’esito. Il nuovo
`GET /spatial/history` restituisce `tholus.stk-spatial-history/1`, stream,
`metadataFrameIds` (massimo 64) e `depthFrameIds` (massimo 12, sottoinsieme dei
metadata). Prima del grant l’host richiede copertura completa dei candidati.

Gli update restituiscono `history`; la perdita di risorse ottiche usa
`presentationAccepted:false` e `presentationReason`, senza revocare il grant.
`historyReady:false` disarma; il solo valore `true` non può sostituire una prova
valida. Il recupero richiede un rilascio successivo alla nuova attivazione.

## Verifiche e limiti

Eseguito `python3 -B tools/immersive/CheckRelayProjectionSource.py`: cinque
controlli sorgente/matematici superati, inclusi 4000 round trip del mapping
ricevente e copertura delle sei viste. È una verifica matematica, non GPU.
La revisione statica ha coperto contratti, lifecycle, replay, intervalli clock,
mapping, occlusione, lease e confini dei flag. Verificati whitespace e link locali.
Il controllo localizzazione host non ha segnalato nuove anomalie.

Regressioni C++ in `tools/immersive/tests/scintilla_input_regression.cpp` e
regressioni Swift di mapping, ammissione, timeline, history e trasporto scritte
ma NON COMPILATE/NON ESEGUITE, rispettando la richiesta di non fare build.

NON ESEGUITI: build vanilla/relay/TholusOS, typecheck, compilazione shader,
regressioni native, avvio gioco/app, API live, output Syphon, presentazione Metal,
secondo comando fisico, pairing/calibrazione, mira durante curve reali,
cancel/rearm hardware, latenza, memoria GPU, prestazioni e accettazione venue.
I gate hardware SC0 restano aperti. SC1 è implementata nei sorgenti e ancora
priva di qualifica runtime; SC2 non è stata avviata.

`source-manifest.json` e `verification.txt` fotografano la consegna iniziale
del 14 ottobre. I checkpoint e gli hash delle correzioni successive sono nel
[registro del 15 ottobre](../sc1-remediation-2026-10-15/verification.json).
WIP estraneo preservato.
