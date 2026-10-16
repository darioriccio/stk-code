# Contratto additivo SC4

Stato del 16 ottobre 2026: implementato e revisionato nei sorgenti, **HTTP/app/gioco non eseguiti**. Resta valida la superficie congelata [STK/TholusOS](../../../immersive_relay_tholustos_integration.md); questa estensione non cambia il protocollo delle sessioni non SC4.

## Identità e preparazione

Contratto `tholus.stk-scintilla-mission/1`, schema 1, profilo `sc4_cargo_three_slots_v1`, profileRevision e missionRevision `"1"`. Le capability espongono `scintilla_mission_sc4`; la scoperta host sceglie `missionSC4` e richiede anche combat e defense. Layout esatto `front_rear_cargo_v2`/`"2"`.

Le richieste missione contengono `contract`, `schemaVersion`, `requestId` UUID, `profileId`, `profileRevision` e `expected`. La tupla attesa è:

```text
producerUUID, raceEpoch, intentRevision, missionRevision, rosterRevision,
inputSessionID, inputEpoch, routeRevision, projectionContextId,
anchorManifestHash, introManifestHash, layoutProfile, layoutRevision
```

Epoch, revisioni e identità numeriche larghe viaggiano come stringhe decimali canoniche, senza conversione in Double. Campi sconosciuti, duplicati anche dopo escape, frazioni, interi fuori limite e identità obsolete sono rifiutati. Il producer UUID appartiene al processo; non coincide per assunzione con il server Syphon.

Tutti gli endpoint passano dall'API locale autenticata esistente (`X-STK-Relay-Token`) e dal dispatcher del game thread. Un hash identifica contenuto; non autorizza una richiesta.

| Metodo e percorso sotto `/immersive/v1/scintilla/mission/` | Semantica |
|---|---|
| `GET state` | Snapshot: policy, identità, preparazione, sospensione, qualifica ancore, intro, tick, slot, fari, cavo, feedback e risultato. Non prepara il gioco e non carica file. |
| `POST intent` | Campo aggiuntivo `required`. Preflight di copie dei tre core e commit atomico: SC4 implica SC2/SC3. Held pre-gara; stessa richiesta/identità è idempotente. Stesso valore con nuovo requestId aggiorna gli asset dopo reset senza incrementare intentRevision. |
| `POST prepare` | Richiede intro completata, qualifica geometrica locale, profilo nativo ammesso, SC1/SC2/SC3 preparate e frame freschi realmente presentati con layout2. Stessa gara conserva carico e fari; la preparazione di un risultato terminale ripristina solo risorse UI. |
| `POST intro-complete` | Campi `variantId`, `completion` (`played` o `operator_skipped`) e `playbackID` UUID. Receipt immutabile per producer/gara/hash/variante; non abilita countdown da sola. |

La ring delle richieste recenti è limitata a 64: exact retry precede il controllo dell'identità corrente; requestId riusato con altro contenuto è conflitto. Non esiste un contatore permanente che esaurisce una normale sessione. Una richiesta vecchia rimossa dalla ring deve comunque superare le revisioni correnti.

Il profilo confronta realmente gara locale, Tux umano, snowtuxpeak forward, tre AI easy, P=2–6, C=P−1, giri nativi 1–12, assenza di progresso accelerato, adaptive difficulty e azioni crew legacy. Binding, fingerprint di rendering e gate vengono ricontrollati prima della fisica.

## Risultato e lifecycle

Il record condiviso fra state e report contiene schemaVersion, raceEpoch, missionRevision, finishSequence, finishTick, naturalFinish, nativeFinishTime, officialTeamRacePosition, securedCount, cargoCompleteAtFinish, slotsAtFinish, beaconsAtFinish, terminationReason e outcome. Le identità e i tick sono stringhe; posizione e tempo ufficiali sono `null` per un termine artificiale.

Gli outcome naturali sono `mission_victory`, `race_won_cargo_incomplete`, `cargo_delivered_race_lost`, `mission_failed`; il quinto stato operativo è `not_completed`. Il risultato si congela nel vero `newLap` umano prima di `notifyLap` e del report. Non deriva dal crew score. Una callback GPU successiva non può cambiarlo.

Pausa/handoff disarmano hold e richiedono rilascio mantenendo carico, fari e deadline gameplay. `/session/reset`, `/operator/reset` e Pronto nei risultati effettuano il reset nativo e un nuovo raceEpoch; il grant fisico valido può essere riusato con nuove prove/binding. I valori SC4 della gara precedente non riaprono Ready. Un ricaricamento effettivo delle risorse invalida anche la relativa qualifica locale.

## Introduzione locale

Manifest `data/immersive_relay/scintilla/intro_it_v1/manifest.json`, contratto `tholus.stk-scintilla-intro/1`, schema 1, italiano, 1920×1080, 30/1 fps, 1200 frame, 40 s. Varianti `C1`, `C2`, `C3plus`; ogni variante ha sei pannelli PNG, video H.264 senza traccia audio e WebVTT, ciascuno con hash. Testi e grafica rendono il briefing comprensibile senza voce. Provenienza/licenza nel README degli asset; nessun TTS live.

Il package host valida il manifest; l'app verifica path locali, assenza di symlink/traversal, dimensioni, hash e metadata AVAsset fuori dal MainActor. Il playback usa l'unico trasporto e owner MediaBus esistente, con licenza playback e accesso STK realmente pubblicati. `played` richiede il termine dell'AVPlayerItem posseduto, correlato con operation/generation/producer/race. Skip è un'azione operatore esplicita; errori o media mancanti mantengono l'avvio chiuso.

Durante la composizione temporanea, SI5 resta disarmato: rinnova clock/lease e drena gli eventi senza produrre nuove prove ottiche. Il ritorno verifica route, calibrazione, owner e nuove evidenze video; riprova una restituzione fallita conservando il proprio contesto. Non ripristina una composizione sopra l'owner di un'altra attività. Ogni restituzione o riprova richiede una nuova conferma di rendering, identificata da generation, preparationID e cycle UUID, anche sulla stessa route. Solo il completamento Metal della richiesta corrente conferma il ritorno; una callback vecchia o successiva al fallimento non riapre il gate.

## Registrazione e qualifica geometrica

Contratto trace `tholus.stk-scintilla-measurement/1`; endpoint sotto `measurement/`:

| Metodo | Body o risposta |
|---|---|
| `POST start` | Contratto/schema1/requestId/producerUUID/raceEpoch e `phase: shell|open`. Held, profilo nativo valido, nessun consumer o core SC2–SC4 preparato/richiesto. Attiva soltanto diagnostica. |
| `POST stop` | Stessi campi comuni senza phase; held, chiusura dopo le query pendenti. Un rifiuto `measurement_gpu_pending` richiede polling/riprova, non esportazione incompleta. |
| `GET state` | recordingID, recording, closed, phase, reason, producerUUID/raceEpoch, frameCount e byteCount canonici, SHA-256 della trace chiusa. |
| `GET trace?offset=…` | Solo trace chiusa e mondo held: recordingID, offset/nextOffset canonici, eof e data UTF-8. Chunk massimo 32768 byte, senza tagliare un codepoint. |
| `POST preflight` | Campi comuni; pre-gara held. Senza grant oppure con `inputMaintenanceOperationID` esattamente corrispondente alla manutenzione attiva. Verifica asset/eseguibile e descriptor/tracce installati, risponde `ok`, `qualified`, `reason`. Non genera misure. |

Limiti: 18000 frame per trace, otto frame-query pendenti, quattro primitive diagnostiche, 64 KiB per riga, 64 MiB per trace, massimo otto trace. Il campionamento nominale è 30 Hz con deadline accumulata e 2 ms di tolleranza di quantizzazione; ogni riga riporta il tempo reale. Gap oltre 50 ms, pause anche fra due catture, inversioni, rescue e discontinuità interrompono le finestre. Le velocità minime includono i tick fisici fra le catture.

NDJSON: header con identità/provenance, fingerprint JSON esatto e hash, gruppi, limiti, `phasePolicy: diagnostic_constant_per_trace`, fase e actualLayoutRevision; frame con posa, camera, progresso ufficiale, velocità/flag nativi, stato e conteggi raster; footer con complete/reason/naturalFinish/officialLap. EvaluatedLayoutRevision2 appartiene al fingerprint; il layout effettivamente mostrato durante diagnostica è dichiarato separatamente. Catture di processi/gare diversi si possono unire solo con fingerprint identico.

Dal riesame del 16 ottobre, il limite header `liveProofMaxAgeUs: 50000` e i campi frame `readbackUs`/`readbackTick` sono obbligatori. Sono stringhe canoniche nel dominio monotono/tick del produttore, rilevate dopo la lettura delle query; possono coincidere fra frame letti nello stesso batch, ma non arretrare né precedere captureUs/tick. Ogni oggetto contiene anche `activationUs`, zero se activation è false, altrimenti il tempo monotono del crossing fisico osservato. Le trace precedenti prive di questi dati sono rifiutate e richiedono una nuova acquisizione.

Per le capsule, una prova i è utilizzabile nell'intervallo chiuso `[readbackUs_i, captureUs_i + 50000]`. La qualifica richiede intervalli non vuoti e sovrapposti fra catture consecutive della finestra: `readbackUs_i ≤ captureUs_(i−1) + 50000`. A 30 Hz, una latenza di 20 ms lascia intervalli scoperti pur essendo inferiore a 50 ms; 90 ms non produce alcun intervallo utilizzabile. Durata e tick della finestra partono dal primo readback effettivo e terminano all'ultima cattura, senza accreditare il ritardo GPU; le minime velocità restano calcolate sull'intero intervallo di cattura, in modo conservativo. L'attivazione richiede inoltre una prova prefetch già letta e ancora fresca all'esatto activationUs: un risultato arrivato dopo il crossing non lo qualifica retroattivamente. Il runtime usa la stessa funzione di disponibilità e conserva il limite di 50 ms.

Il limite di disponibilità delle capsule è separato dal gap massimo di campionamento e dal contratto del cavo. Il cavo mantiene la finestra geometrica di almeno 3 s e il proprio gate di prima presentazione; nessun nuovo limite di 50 ms viene imposto al cavo o alla difesa SC3. Il timeout di drenaggio query di 250 ms rimane un limite di risorse, senza rendere una prova tardiva qualificante per le capsule.

Il fingerprint copre eseguibile, asset, driveline, shader, ancore, camera applicata, output/proiezione, risoluzioni, near/far, F, P/C/giri, raggi e layout valutato. Il preflight attuale usa un hash conservativo dei due alberi asset, fino a 4 GiB complessivi; esclude la directory delle prove e opera solo nel percorso esplicito held. Il percorso ordinario di preparazione non ripete tale scansione.

Il descriptor locale è `data/immersive_relay/scintilla/qualification_v1/report.json`, contratto `tholus.stk-scintilla-qualification/1`, schema1, scope `native_render_geometry`. Contiene status (`qualified_geometry`/`unqualified`), provenance, fingerprintJSON/Hash, traceFiles con path relativo/hash/byte e reasons. Il reader nativo ricontrolla ogni trace e ricalcola finestre/attivazioni/envelope; **non si fida del solo status**. La qualifica non dimostra SI5 ottico, transazione Shell→Open effettivamente giocata, gara SC4 naturale o comfort venue.

Nessun descriptor qualificato o trace reale è stato creato in questa sessione. Il JSON di authoring rimane non qualificato e Ready resta chiuso.

## Preflight con grant fisico conservato

L'upgrade SC3→SC4 esegue il preflight prima dell'intent missione quando manca la qualifica in cache. Lo snapshot pubblica anchorQualified solo per il fingerprint corrente; una cache obsoleta espone preflight_required senza I/O o mutazioni nel GET. L'host chiude il gate di presentazione, attende le richieste input native già partite e usa `POST input-maintenance/begin` / `POST input-maintenance/resume` nello stesso namespace missione autenticato. È ammesso solo nel mondo locale pre-gara held, con profilo nativo effettivamente compatibile.

Contratto `tholus.stk-scintilla-input-maintenance/1`, schemaVersion 1. Entrambe le richieste contengono producerUUID, raceEpoch, inputSessionID, routingGeneration, expectedInputEpoch e operationID UUID stabile. Race e input epoch sono stringhe decimali canoniche. Begin incrementa inputEpoch una sola volta, disarma i controlli e invalida binding e prove ottiche, conservando grant fisico, sequenze, continuità delle sorgenti e difese anti-replay. La risposta contiene ok, contract, schemaVersion, producerUUID, raceEpoch, inputSessionID, routingGeneration, inputEpoch, operationID e suspended. Retry esatti non aprono una seconda transazione; un begin già completato restituisce suspended false.

Durante la scansione l'host rinnova e drena SI5, conserva gli ultimi eventi di lifecycle necessari e scarta azioni/mire pregresse. L'input nativo ordinario è sospeso e Pronto resta chiuso. Resume richiede inoltre expiresAtHostUs da un rinnovo SI5 effettivo e tre clockSamples freschi; ricontrolla identità, lease, offset e drift. Non azzera alcun registro anti-replay. Un resume già completato è idempotente e non estende il grant. La ripresa richiede rinnovo nativo immediato, nuove prove video e rebind logico, poi la normale preparazione SC2–SC4.

Il timeout HTTP esteso a 35 s riguarda soltanto measurement/preflight; i controlli di clock e input mantengono i limiti ordinari. Un errore di trasporto attiva il recupero della medesima transazione; un esito non qualificato resta visibile e offre una riprova esplicita, senza scansioni automatiche a ogni polling. Revoca fisica, cambio route o produttore seguono l'isolamento esistente. La registrazione diagnostica di nuove trace resta separata e richiede un mondo senza core preparati.

Reset, abort e termine forzato attendono la conclusione della verifica in corso prima di cambiare l’autorità della gara. Se la manutenzione non è stata recuperata, il comando resta bloccato e il grant SI5 non viene chiuso. Un nuovo cambio di modalità non può interrompere una manutenzione in corso.
