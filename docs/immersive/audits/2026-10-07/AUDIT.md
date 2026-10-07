# Audit MVP SuperTuxKart ↔ TholusOS
Data: 7 ottobre 2026. “Torso S” è stato interpretato come TholusOS.

## Esito
**Il gioco locale parte e carica piste diverse. Il percorso integrato TholusOS → scelta pista → gara → risultato → nuova gara non è ancora una MVP affidabile.** Esistono già molti componenti: non serve riscrivere il gioco. Mancano soprattutto il raccordo fra stato API e mondo di gara, un avvio riproducibile e la chiusura del percorso operatore/video.

Questo incarico è un audit: nessuna modifica al codice dei due prodotti, nessun aggiornamento upstream, nessuna build/installazione. Sono stati eseguiti controlli API e un avvio grafico della build esistente, con preferenze temporanee e token casuale locale. Nessuna prova fisica di volante, telefoni o cupola.

## Snapshot verificato
- STK: branch/fork basato su release **1.5**, commit upstream `1fb491f507216c5d181ccd85f29ff08eca003827`; HEAD fork `2a038720e25e75ed775b3b0e72fd66db3fb9e9ec` del 14 giugno 2026.
- CMake dichiara 1.5; anche l'eseguibile risponde “SuperTuxKart, 1.5”.
- Build disponibile: `cmake_build_relay_phase1`, eseguibile datato 9 giugno. Non è stata ricompilata: le osservazioni runtime qualificano questo binario, quelle statiche il codice corrente.
- Cache build: Relay/API/test tools ON, **Syphon OFF**. La build Syphon indicata dal contratto (`cmake_build_phase45_syphon`) non è presente.
- Syphon.framework è disponibile nella Library dell'utente: il framework non è il blocco attuale.
- Asset locali: 44 manifest `track.xml`, inclusi contenuti non destinati alle normali corse. Non equivale a 44 piste gareggiabili.
- TholusOS: HEAD `1581038fcd41aef9c79c60e53785a80a24ab45c9`, checkout con WIP esteso. Analizzato il codice attuale; non attribuita questa revisione all'app installata.
- Modifiche preesistenti STK: `docs/AGENTS.md`, brief difficoltà, master spec e `AGENTS.md` non tracciato. Preservate. Anche il WIP TholusOS è rimasto intatto.

## Prove effettuate oggi
| Prova | Esito | Cosa dimostra |
| --- | --- | --- |
| Avvio diretto eseguibile, versione | OK | Il binario e le sue dipendenze si caricano su questo Mac |
| Avvio grafico Relay, Snow Peak (`snowtuxpeak`) | OK | Scena, kart, avversari, HUD e cronometro visibili |
| Dal menu STK: pausa → nuova gara → Around the Lighthouse → Start Race | OK | Catalogo e caricamento di una seconda pista effettivamente funzionanti |
| API capabilities/stato/reset/configure/start/countdown | HTTP 200 nei casi validi | API disponibile; non certifica il mondo di gioco |
| Cambio Snow Peak → Lighthouse via API | **FAIL funzionale** | API dichiara Lighthouse, schermo ancora Snow Peak, timer non azzerato |
| Richiesta output Syphon alla build disponibile | HTTP 400 | `immersive_output_unavailable:syphon` |
| Configurazione pista inesistente | **HTTP 200 errato** | Mancata verifica pista installata |
| Configurazione API un partecipante | HTTP 400 | `invalid_player_count`; CLI invece ammette uno |
| Collegamento “Apri Immersive Relay Sala Giochi.command” dalla root | **FAIL** | Cerca la build nella directory sbagliata |

Non è stata completata una corsa naturale fino al traguardo; non sono stati verificati audio, input continuo al volante, telefoni reali, Syphon live o proiezione. Le immagini delle verifiche grafiche sono nella conversazione; gli stati e i log sono nella directory `evidence/`.

## Problemi da risolvere per la MVP

### 1. P1 — I comandi operatore non governano il ciclo della gara reale
**Confermato nel codice e con verifica grafica.**

`configureArcadeLocked` copia configurazione e prepara la macchina a stati; `operatorStart` entra in briefing; countdown avvia la sessione logica; `operatorReset` ripulisce il controller. In questi percorsi manca il caricamento/riavvio del mondo STK tramite RaceManager.

Riproduzione di oggi:
1. Avvio Snow Peak.
2. `reset`, `configure(track_id=lighthouse)`, `start`, `countdown(skip=true)`.
3. Stato prima: Snow Peak, elapsed 27.508 s.
4. Stato dopo: Lighthouse, elapsed 30.541 s.
5. Sullo schermo resta la stessa scena innevata. Il cambio reale avviene soltanto usando il menu STK.

Fonti: [configure/start](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/relay_session_controller.cpp:978), [reset](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/relay_session_controller.cpp:1316), [router](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/remote/relay_command_router.cpp:918). Evidenze: `graphical-before.json`, `graphical-after.json`.

**Intervento necessario:** raccordare configure/start/reset/abort/nuova gara al thread di gioco e al ciclo RaceManager; distinguere configurazione richiesta da pista realmente caricata. Stato pronto solo dopo conferma del mondo. Reset deve riportare anche kart, timer e gara al punto concordato.

### 2. P1 — Output video TholusOS non disponibile nel pacchetto corrente
**Confermato a runtime.**

La build ha soltanto output none/window. I launcher usano questa build e non attivano Syphon. Il comando ufficiale del contratto punta a una build oggi assente.

**Intervento necessario:** produrre una build Relay con Syphon, launcher unico con asset e dipendenze verificati e preset iniziale 1536/768; controllare ricezione reale in TholusOS, frame aggiornati e riconnessione dopo reset. I valori 2048/1024 restano da qualificare sul dispositivo di destinazione.

**Rischio aggiuntivo da verificare live:** TholusOS richiede esattamente applicationName “Immersive SuperTuxKart”, ma il bundle STK corrente dichiara CFBundleName “SuperTuxKart”. Il publisher usa il nome server senza override applicazione. Il receiver confronta esattamente entrambi. È una discrepanza statica concreta, ma non è stata osservata una discovery Syphon di questa build, che non lo supporta.

Fonti: [descriptor](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/ImmersiveSuperTuxKartRuntimeController.swift:250), [matching receiver](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/VideoInputs/Syphon/SyphonMetalTextureProvider.swift:77), [publisher](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/output/syphon_mac/syphon_output_adapter_mac.mm:38). Evidenze: `capabilities.json`, `syphon-request.json`.

### 3. P1 — Avvio da cartella rotto; avvio da TholusOS ancora manuale
**Launcher riprodotto; assenza gestione processo verificata nel codice TholusOS.**

I due .command della cartella principale sono link simbolici. Gli script ricavano il repository da `dirname "$0"` senza risolvere il link. Il launcher sala giochi cerca quindi l'app in `/Volumes/SanDisk 2TB/the usual neXt/cmake_build_relay_phase1/...`, anziché dentro questo progetto.

In TholusOS “prepara” effettua la connessione REST, non avvia STK. Il runbook richiede avviare prima il producer manualmente.

**Intervento necessario:** correggere la risoluzione dei launcher e consegnare un punto di avvio affidabile. Per una prima MVP è sufficiente anche un launcher esterno chiaramente integrato nel flusso; per il requisito “parti da TholusOS” servono lancio processo, attesa API/video, errori leggibili e recupero. Evitare doppie istanze/porte occupate.

Fonti: [launcher](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/tools/immersive/RunRelayArcadeMenu.command:4), [adapter](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/ExternalImmersiveGameAdapter.swift:228), [connessione](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/ImmersiveSuperTuxKartRuntimeController.swift:281).

### 4. P1 — Passaggio briefing/countdown incoerente nel consumer
**Difetto ricostruito dal codice, con risposta STK start→briefing confermata via API. UI TholusOS non provata live.**

TholusOS traduce anche “briefing” in connectionState running. Il workspace interpreta running come playing e mostra pausa/stop, mentre il pulsante Countdown è esposto soltanto nella fase ready. L'azione Start invia soltanto start, non countdown. Il pannello tecnico dispone dei comandi separati, ma il flusso consumer non è coerente.

Inoltre il loop STK può riscrivere gli stati logici in funzione del mondo già attivo: la soluzione va verificata insieme al problema 1, non solo cambiando l'etichetta UI.

Fonti: [mappatura stato](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/ImmersiveSuperTuxKartRuntimeController.swift:694), [fase workspace](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/Workspace/GamesWorkspaceStore.swift:381), [azioni](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/Workspace/GamesWorkspaceStore.swift:432).

**Intervento necessario:** distinguere pronto, briefing, countdown e gara realmente in esecuzione; coprire l'intera sequenza Start → briefing → Countdown → partenza.

### 5. P1 — Scelta piste non pronta per l'operatore
**Verificato nel codice e tramite API.**

Il menu nativo STK mostra il catalogo con miniature e funziona. TholusOS chiede invece un ID testuale; lato STK basta che non sia vuoto: `audit_track_does_not_exist` è stato accettato. Non esiste nel contratto congelato un endpoint catalogo piste.

**Intervento necessario:** per la MVP basta una piccola lista di piste installate, con nome leggibile e ID validato da STK; non serve progettare uno store. Proposta iniziale: Snow Peak, Around the Lighthouse, Oliver's Math Class. Le prime due sono state caricate graficamente oggi; Oliver's Math Class ha asset e voce di catalogo presenti ma non è stata avviata in questa verifica. La selezione per cupola va qualificata per luminosità e comfort; Lighthouse è visivamente scura.

Crew: c'è uno script specifico Snow Peak e uno generico. Il director cerca il file della pista e ripiega sul generico; l'esistenza di altre piste non certifica encounter dedicati. Per la prima guida base si può mantenere crew disabilitata; se si abilita Relay, collaudare gli encounter delle piste ammesse.

Fonti: [campo pista](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/Workspace/GamesWorkspaceStore.swift:528), [validazione STK](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/relay_session_controller.cpp:1043), [selezione script](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/crew_event_director.cpp:568).

### 6. Scelta di perimetro — La configurazione attuale obbliga 2–6 partecipanti e telefoni
**Confermato nel codice e per il minimo API. Non è necessariamente un bug del prodotto Relay originale.**

CLI permette un pilota; configure operatore rifiuta uno. TholusOS limita a 2–6, forza crew enabled e blocca Start finché non risultano connessi tanti telefoni quanti i partecipanti. I telefoni hanno azioni crew, non sterzo/acceleratore/freno; la guida resta input locale STK.

Per la MVP richiesta suggerisco **un pilota, tastiera/gamepad/volante, alcune piste, avversari, video in TholusOS; crew facoltativa**. La staffetta 2–6 rimane un percorso successivo da provare, senza renderla un requisito per accendere e usare la prima demo.

La lunghezza attuale è fissata a 6 giri per 1–3 persone, 8/10/12 per 4/5/6; anche `--laps` viene ignorato dalla policy Relay. Se serve una demo breve, va resa esplicita una policy compatibile invece di presumere che il parametro abbia effetto.

Fonti: [gate telefoni](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/AmbientPlay/Debug/AmbientPlayOperatorController.swift:1401), [payload](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/Modules/GameShowControl/ImmersiveSuperTuxKartAPIModels.swift:1056), [giri](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/relay_session_controller.cpp:737).

### 7. P1 da riprodurre — Traguardo naturale e rapporto finale non sufficientemente collegati
**Rischio forte da analisi sorgente; gara completa non eseguita.**

`RelayRaceMode::newLap` chiama prima la base, poi notifica Relay soltanto se il kart NON ha già finito. La base marca il kart finito al traguardo: il ramo `notifyLap(completed_laps >= total_laps)` può quindi non essere raggiunto. Il percorso alternativo richiede lap_progress ≥ 1, mentre al passaggio sul traguardo la distanza lungo pista torna all'inizio. Non si può certificare la conclusione naturale sulla base dei test che forzano progresso/finish.

Fonti: [hook giro](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/relay_race_mode.cpp:499), [fine gara base](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/linear_world.cpp:486), [fine Relay](/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code/src/modes/immersive_relay/relay_session_controller.cpp:1929).

**Intervento necessario:** verificare una corsa naturale e notificare in modo univoco il termine effettivo; poi controllare risultato in TholusOS e seconda gara su pista diversa. Force-finish e test progress non soddisfano questo criterio.

## Cosa è già utilizzabile
- Motore 3D, asset, menu nativo e avvio locale di più piste.
- Moduli Relay separati, state machine, handoff, azioni crew, difficoltà adattiva e API autenticata.
- Render cubemap → fisheye domemaster e publisher Syphon implementati nel sorgente.
- Client REST TholusOS, comandi operatore, raccordo video al renderer e bridge LAN telefoni.
- Fixture e script diagnostici utili per contratti e controlli negativi.

Questa base riduce il lavoro necessario, ma non sostituisce il collegamento funzionale fra i componenti.

## MVP proposta e ordine di lavoro
| Passo | Consegna | Criterio di accettazione |
| --- | --- | --- |
| 1 | Build Syphon e launcher riproducibile | Da chiuso, avvio senza Terminale; API pronta e frame reali ricevuti |
| 2 | Ciclo gara reale collegato all'API | Scelta pista cambia la scena; countdown governa la partenza; reset azzera il mondo |
| 3 | Percorso TholusOS essenziale | 3 piste nominate, avvio/countdown chiari, modalità un pilota senza telefoni obbligatori |
| 4 | Traguardo, risultato e ripetizione | Completare naturalmente almeno una gara, vedere il risultato, iniziarne altre due senza riavviare app |
| 5 | Collaudo sul setup reale | Input scelto, audio, frame stabili, stop/reset e recupero dopo perdita producer |

Test prioritari: pista inesistente rifiutata; configure coerente con pista caricata; nessuna guida prima della partenza; reset fisico; fine naturale; tre gare consecutive con due cambi pista; producer chiuso/riavviato; Syphon riconnesso. Aggiungere staffetta e crew solo al relativo percorso. Non usare test simulati per dichiarare il playtest completato.

Il target di accettazione hardware va registrato: prova sul Mac corrente, Mac mini M1 e proiezione reale sono evidenze diverse. Nessun benchmark attuale autorizza a promettere 2048 o una particolare frequenza di rendering.

## Aggiornamenti upstream
Verifica live del 7 ottobre 2026 tramite sito e API ufficiali:
- **Ultima release stabile: 1.5, pubblicata il 20 ottobre 2025. È già la base del fork.** [Release ufficiale](https://github.com/supertuxkart/stk-code/releases/tag/1.5)
- Il ramo master è avanzato fino a `5e7a54420a2748ae5a5e743555dd00ddf513302d`, 6 ottobre 2026. [Commit verificato](https://github.com/supertuxkart/stk-code/commit/5e7a54420a2748ae5a5e743555dd00ddf513302d)
- Il confronto GitHub rispetto a 1.5 è **divergente: 157 commit lato master e 5 lato release**, non un semplice aggiornamento lineare. Fra quei 157, 39 hanno data committer dal 14 giugno 2026 incluso. La lista file restituita è limitata a 300: non usata come inventario completo dei conflitti. [Confronto ufficiale](https://github.com/supertuxkart/stk-code/compare/1.5...5e7a54420a2748ae5a5e743555dd00ddf513302d)
- Sono presenti correzioni a estrazione ZIP degli add-on, validazione rete e rendering, oltre a un aggiornamento del changelog 1.5.1. Questo non equivale a una release stabile 1.5.1 pubblicata. [Correzione add-on](https://github.com/supertuxkart/stk-code/commit/c6b12779)

**Indicazione:** mantenere 1.5 come base per chiudere la MVP e valutare separatamente backport mirati. Non serve una migrazione generale a master per riparare i blocchi trovati. Un rebase dovrà controllare i renderer/camera e i file condivisi già modificati; il fork tocca anche `frame_buffer.hpp` e `skybox.cpp`, non elencati nelle vecchie note dei soli hook.

## Evidenze e limiti
- `probe.py`: API su porta temporanea, nessun avanzamento simulato; eseguito.
- `graphical_probe.py`: avvio grafico e richiesta cambio pista; eseguito, poi menu nativo usato via CUA.
- `evidence/`: risposte API, stati prima/dopo, log grafico, inventario manifest e snapshot API GitHub.
- Test TholusOS esistenti sono principalmente fixture/fake. La documentazione della revisione Giochi del 6 ottobre distingue check isolati da integrazione live non eseguita; non è stata promossa a prova del flusso completo.
- Nessuna nuova build dei prodotti. Nessun end-to-end STK↔TholusOS↔cupola completato.
- Processi avviati dall'audit terminati; preferenze temporanee eliminate. Conservati soltanto rapporto, riproduttori ed evidenze. Nessun worktree creato, nessuna cache condivisa rimossa.
