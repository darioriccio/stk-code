# SC4 — preparazione e prove successive

**In questa consegna il codice non è stato compilato o avviato.** Le procedure native seguenti richiedono una successiva autorizzazione a build/test. Il profilo non diventa giocabile cancellando il gate o impostando un booleano nel JSON: servono misure native reali e una qualifica coerente.

## 1. Qualificare le posizioni senza puntatori

1. Compilare la versione consegnata quando autorizzato, con asset locali e flag Relay normali. Configurare snowtuxpeak forward, Tux umano, tre AI easy, P/C e numero di giri da qualificare, camera/output/proiezione finali. Usare una gara diagnostica held, senza policy/consumer SC1–SC4 preparati. Niente progresso accelerato.
2. Impostare il token API nell'ambiente `STK_RELAY_TOKEN` mediante il normale sistema credenziali; non scriverlo nei log. Il client accetta soltanto un endpoint loopback.
3. Leggere `state` con `collect_mission_measurement.py`, annotare producerUUID e raceEpoch esatti. Eseguire `start --phase shell --producer "$SC4_PRODUCER_UUID" --race "$SC4_RACE_EPOCH"`. La variabile requestId stampata permette di riprovare lo stesso comando in caso di risposta incerta.
4. Avviare esplicitamente la gara tramite il normale flusso operatore e guidare il kart nativamente. Registrare tutti i giri, velocità normali e stress reali nitro/zipper. I ghost sono diagnostici; non consegnano carico e non provano un puntatore fisico. Arrivare naturalmente almeno in una delle catture ammesse.
5. Riportare il mondo held e chiamare `stop`. Se la GPU è ancora pending, attendere e riprovare; controllare `closed: true`. Esportare con `export --output "$SC4_EVIDENCE_DIR/shell.ndjson"` in una directory esistente sul SSD. Il client non sovrascrive file, controlla identità, numero di byte e SHA-256.
6. Nuova gara diagnostica con lo stesso fingerprint e `--phase open`; ripetere il percorso. Servono finestre indipendenti per ciascuna fase, capsula, lato del cavo e giro. Fino a otto trace possono coprire più prove; non unire configurazioni diverse.
7. Eseguire il validatore offline e leggere tutti i motivi di rifiuto. Un risultato unqualified richiede nuove misure o una correzione degli ancoraggi e nuova acquisizione; non modificare lo status manualmente.

Esempio, dalla radice STK, dopo aver assegnato le variabili alle directory reali sul SSD:

```sh
python3 -B tools/immersive/scintilla/collect_mission_measurement.py state
python3 -B tools/immersive/scintilla/collect_mission_measurement.py start --phase shell --producer "$SC4_PRODUCER_UUID" --race "$SC4_RACE_EPOCH"
python3 -B tools/immersive/scintilla/collect_mission_measurement.py stop --producer "$SC4_PRODUCER_UUID" --race "$SC4_RACE_EPOCH"
python3 -B tools/immersive/scintilla/collect_mission_measurement.py export --output "$SC4_EVIDENCE_DIR/shell.ndjson"
python3 -B tools/immersive/scintilla/validate_mission_qualification.py --anchors data/immersive_relay/scintilla/snowtuxpeak_mission_v1.json --trace "$SC4_EVIDENCE_DIR/shell.ndjson" --trace "$SC4_EVIDENCE_DIR/open.ndjson" --report "$SC4_EVIDENCE_DIR/report.json"
```

L'esempio non guida il kart né crea la seconda cattura automaticamente. Ogni start/stop deve usare l'epoch corrente. Le trace devono trovarsi sotto la directory del report, con nomi relativi sicuri e nessun symlink. Python 3.10 o successivo.

Criteri conservativi: sagoma completa ammessa/visibile, Shell e Open ciascuna per più di 2 s, cavo almeno 3 s per lato/stile, normale velocità nativa coperta, attivazione Shell osservata con prefetch ammesso e crossing fisico. Devono qualificarsi tutti i gruppi del manifest ammessi nel giro, comunque almeno quattro (tre più rimpiazzo). Un ritardo o un picco di velocità non sostituisce una finestra continua. Anche una finestra lunga successiva non ripara un'attivazione già fuori range.

Le nuove acquisizioni devono includere readbackUs/readbackTick e activationUs: le trace anteriori a questa correzione non sono ammesse. Per le capsule la qualifica controlla insieme frequenza di cattura e disponibilità effettiva GPU, oltre alla geometria. Un risultato tardivo rispetto al crossing, o intervalli di freschezza separati da un vuoto, richiede di risolvere la causa di prestazioni e acquisire di nuovo; aumentare il timeout di esportazione non ripara la qualifica. Il cavo conserva il proprio contratto di prima presentazione e finestra geometrica.

Installare descriptor e relative trace nella directory locale `data/immersive_relay/scintilla/qualification_v1/`, mantenendo i path relativi. Per questa consegna **non esistono** report e trace reali da installare. Non copiare le fixture sintetiche dei test.

## 2. Preflight e preparazione di una sessione

1. Mondo pre-gara held: il percorso Missione SC4 nell'host esegue il preflight se manca una qualifica in cache. In alternativa, prima di concedere un consumer, chiamare `preflight --producer … --race …` con il client diagnostico. Il processo ricontrolla report e file grezzi; `qualified: false` mantiene chiuso l'avvio. Ripetere dopo riavvio del produttore o vero caricamento di mondo/risorse, e dopo cambi alla configurazione qualificata.
2. Nell'host selezionare Missione SC4. Se SC3 è già preparata, il preflight usa una manutenzione nativa disarmata e conserva il grant SI5, senza richiedere la chiusura del gioco. Al termine servono prove video fresche e rebind. Il requisito attiva atomicamente SC2/SC3 e SC4. Evidenze mancanti/non ammesse mantengono chiuso il gate; dopo averle installate usare la riprova di verifica missione proposta nell'host. Non avviene una scansione pesante a ogni polling.
3. Importare il commissioning operativo corretto per layout cargo v2. Il passaggio da SC3 preparata può conservare il grant soltanto con identità fisica invariata e upgrade held esplicitamente ammesso. Roster, calibrazione, associazioni, route, timing e provider reali continuano a essere controllati.
4. Riprodurre l'introduzione italiana della variante C corretta attraverso TholusOS. In alternativa usare l'azione esplicita di salto dopo aver letto l'obiettivo. Un errore media non equivale a skip.
5. Attendere il ritorno alla sorgente STK e le nuove prove video/ottiche. Preparare SC1, SC2, SC3 e SC4 con le identità correnti. Solo lo stato preparato completo ammette Pronto/countdown.
6. Giocare: primary apre il guscio, poi un nuovo tool press mantiene il fascio per almeno 0,6 s. Stessa persona ammessa con C=1. Se il feedback chiede rilascio, rilasciare e premere di nuovo. Dopo impatto applicato, recuperare il modulo sul cavo entro 3 s dalla prima presentazione valida.

La gara deve finire naturalmente: posizione ufficiale 1 e tre slot Secured determinano vittoria. I fari raccontano le acquisizioni ma non aggiungono un requisito. Pronto nei risultati crea una nuova gara; stessa pista/reset conserva il certificato delle risorse immutate, carico e fari ripartono vuoti, intro nuovamente richiesta o saltata esplicitamente.

## 3. Pausa, errori e recupero

- Pausa e cambio pilota conservano carico, fari, residuo del cavo e grant fisico valido; azzerano progresso dei fasci e richiedono release/press. Riprendere con binding e prove fresche.
- Intro interrotta: annullare o usare Riprova ritorno. Ogni tentativo richiede una nuova conferma di rendering anche se la route STK coincide con quella precedente. Il recupero conserva l'operazione e non sostituisce una composizione posseduta da un altro contenuto. Nessun countdown automatico su callback vecchie.
- Cavo non ancora presentabile: il watchdog di 250 ms può fermare tecnicamente il mondo, preservando il modulo. La prima presentazione effettiva può ripristinare il percorso; non genera un recupero automatico.
- Cambio di camera/proiezione o caricamento effettivo di nuove risorse: la preparazione/qualifica interessata si chiude. Non usare un vecchio report con un nuovo fingerprint. Non cambiare asset mentre si gioca; una manutenzione completa del produttore è distinta da pausa o reset della stessa pista.
- Un termine artificiale resta `not_completed`; non ricostruire una vittoria a mano. I normali risultati e retry conservano finishSequence ed esito congelati.

## 4. Build e test da eseguire quando autorizzati

Creare output/cache separati e nuovi sotto `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/`, verificando mount e scrivibilità; non riusare directory mutabili di altre chat. Il presente task non ha eseguito alcuno dei comandi nativi sotto.

```sh
cmake -S tools/immersive/tests -B "$SC4_TEST_BUILD" -DENABLE_RELAY_TEST_TOOLS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build "$SC4_TEST_BUILD" --parallel 6
ctest --test-dir "$SC4_TEST_BUILD" --parallel 4 --output-on-failure
python3 -B tools/immersive/tests/run_scintilla_depth_gpu.py --output-dir "$SC4_GPU_EVIDENCE"
```

Il runner GPU compila un helper: era vietato in questa sessione. CTest comprende anche le due suite Python offline; i checker sorgente non sostituiscono i casi C++ compilati. Servono inoltre build del gioco vanilla e Relay/SERVER_ONLY pertinenti.

Host: usare `scripts/ci/test-app-isolated.sh` con il profilo `scintilla`, ambiente/cache esterni e directory di evidenza nuova secondo il runner già documentato nel [checkpoint host](../sc0-sc3-host-tests-2026-10-16/README.md). La discovery include le nuove suite mission/intro e i casi runtime; eseguire separatamente anche i dieci test del package `Packages/TholusScintillaMission`, con `--scratch-path` sul SSD. Conservare firma, deployment target e modalità del linguaggio dell'app.

Chiudere infine M01–M40 con API reale, intro/ritorno reale, quattro outcome naturali, pausa/handoff, results→nuova gara e perdita/ripristino output. Le verifiche venue (puntatori, ottica, audio, comfort, leggibilità e bilanciamento umano) restano ulteriori e separate.
