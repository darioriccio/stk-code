# SC4 — copertura M01–M40

La tabella mappa requisiti del piano a codice e regressioni consegnati. **Tutti i test C++, Swift e GPU sono scritti/revisionati ma non eseguiti in questa sessione.** Le prove naturali e hardware restano aperte. “Source” indica il solo controllo del percorso nel codice; non costituisce un pass runtime.

Riferimenti nativi:

- [Core missione: 32 casi](../../../../tools/immersive/tests/scintilla_mission_cases.inc), [implementazione](../../../../src/modes/immersive_relay/scintilla_mission.cpp).
- [Consumer reale: 103 casi](../../../../tools/immersive/tests/scintilla_input_cases.inc), inclusi 23 casi missione/layout aggiunti.
- [Anchor: 16 casi](../../../../tools/immersive/tests/scintilla_mission_anchors_cases.inc), [protocollo](../../../../tools/immersive/tests/scintilla_mission_protocol_regression.cpp) sei casi, [misure](../../../../tools/immersive/tests/scintilla_mission_measurement_regression.cpp) otto casi.
- [Runner GPU](../../../../tools/immersive/tests/run_scintilla_depth_gpu.py): depth, stili missione, cavo, decorazioni e HUD.
- Host: package `TholusScintillaMission`, suite `ImmersiveSTKMissionHTTPTests`, `ImmersiveSTKIntroMediaTests` e casi missione aggiunti a `ImmersiveSuperTuxKartIntegrationTests`. La discovery del profilo scintilla è aggiornata.

| ID | Invariante e copertura consegnata | Verifica eseguibile ancora richiesta |
|---|---|---|
| M01 | Core `sequentialSoloCollectionConservesModuleIdentity`: stessa persona, primary poi nuova pressione tool. | C=1 nel gioco con sorgente reale/sintetica dichiarata. |
| M02 | Receipt apertura idempotente; consumer `missionPendingEarlierPressKeepsOwnershipOrder`. | Riordino GPU/HTTP nel consumer compilato. |
| M03 | `exclusiveOwnershipUsesPressSerialThenParticipant`; feedback target_busy. | Due sorgenti con esiti GPU di ordine diverso. |
| M04 | `twoCapsulesCompeteForTheLastEmptySlot`. | Concorrenza con un solo slot libero e feedback visivo. |
| M05 | Tool pre-Open, vecchia press e phase change nei core/input cases. | Guscio visibile→Open→nuova prova nel renderer. |
| M06 | Core/input sticky interruption e aggiornamento dei checker combat/feedback. | Miss→hit tra due tick, con query reali. |
| M07 | Istanza/fase/epoch/participant e release receipt isolati. | Tracking/rebind reali e feedback di riarmo. |
| M08 | Live oracle sul core corrente, non ultimo registro render; consumer retirement. | Risultati GPU tardivi dopo raccolta/ritiro/reuse. |
| M09 | `appliedImpactDetachesAtMostOneAndConsumesEmptyReceipts`; bridge dopo applicazione fisica. | Impatto applicato/rifiutato/difeso nel gioco. |
| M10 | Stesso caso con slot 0–3 e priorità del più alto Secured. | Verifica integrata della conservazione dopo impatto. |
| M11 | `impactDuringAnExistingTetherCannotDetachAgain`. | Secondo impatto con cavo già attivo. |
| M12 | `recoveryWindowIsHalfOpenAtAllSupportedFrequencies`; tick 60/120/144. | Casi compilati alla deadline−1/deadline. |
| M13 | `presentationStartsTheRecoveryWindowExactlyOnce`; source watchdog/presentazione e fullmask per frame. | Ritardo, occlusione e perdita output; nessun timer anticipato. |
| M14 | `pausePreservesDeadlineAndRequiresRelease`; gate held prima di Bullet, rebind host. | Pausa/handoff durante recovery, mondo/AI fermi. |
| M15 | `sourceTrackingLossDoesNotPauseTheWindow`; disarmo fonte e deadline gameplay. | Perdita reale/sintetica dichiarata di una sola sorgente. |
| M16 | Core receipt e anchor `backwardAndRepeatedCrossingsCannotFarm`. | Retromarcia e ripassaggio sul tracciato. |
| M17 | `discontinuousForwardSegmentsConsumeSkippedOccurrences`; animazione/rescue e progress guard. | Rescue/teleport attraverso capsule e stazioni. |
| M18 | Crossing multipli ordinati, expiry prima di nuova activation, giro ufficiale. | Confine nativo del giro e più punti nello stesso step. |
| M19 | `beaconSurvivesLossWithoutBecomingAVictoryCondition`. | Faro prima/dopo acquisizione e perdita successiva. |
| M20 | `crossingBeforeSameTickCollectionNeedsAFutureOccurrence`; source commit order. | Uguaglianza di tick nel loop nativo. |
| M21 | `naturalFinishProducesFourImmutableOutcomes`; result JSON/DTO concordi. | Quattro gare/outcome naturali con classifica ufficiale. |
| M22 | `tetherAtFinishIsIncompleteAndLateInputsCannotChangeIt`. | Arrivo con modulo Tethered effettivamente visibile. |
| M23 | `expiryBeforeFinishPreventsSameTickRecovery`; source barrier 20 controlli. | GPU pronta e impatto/raccolta al tick d'arrivo. |
| M24 | `artificialTerminationNeverDeclaresNaturalVictory`; router/operator paths. | Force/early finish, timeout/abort e test accelerato. |
| M25 | Record congelato condiviso state/report, finishSequence1 per race e receipt bounded. | Retry/reconnect/report su processo nativo. |
| M26 | Ready UI/telefono/API e risultati: native admission, host mode/async authority guards; source API/lifecycle. | Tutti i chiamanti reali e recupero completo. |
| M27 | `sameRaceRepreparePreservesStateAndNewRaceResetsIt`; reset nativo collegato. | Seconda gara sulla stessa istanza produttore. |
| M28 | Grant fisico mantenuto; upgrade commissioning layout1→2 limitato a identità fisica invariata. Due fixture host aggiunte. | Operatore e telefono con controller/grant effettivo. |
| M29 | Consumer `missionNewDefenseWarningDoesNotInterruptTheSameCapsule`. | Minaccia SC3 durante un hold world invariato. |
| M30 | `missionMixedPrimaryQueuePreservesBothTypedDeliveries`; unico drain e dispatch per kind. | Batch misti risolti dal consumer e dai tre core. |
| M31 | Contatto/clipping coerenti con baseline, minimo definitivo/ambiguità, strict depth; otto controlli Python depth passati. | Eseguire shader/resolver/GPU e casi con occludenti. |
| M32 | Capsule prima di AI/pickup nel budget32; ghost diagnostici esclusi da input/core; fullmask include occludenti semantici. | Saturazione32 con due capsule e cavo reali. |
| M33 | Otto casi layout input, regressioni shader HUD, atlas italiano e geometria offline. | 256, 1536², 2048×1024, resize e leggibilità reale. |
| M34 | Recorder nativo + reader C++/Python; 25 prove Python, inclusa oracle quadratica indipendente per maximin. | **Misure reali per ogni oggetto/fase/giro/C, velocità e attivazione.** |
| M35 | Tre intro finali, decode/hash offline passati; token media reale e skip esplicito. | Player/app/composizione e passaggio a preparazione. |
| M36 | Reducer package, casi runtime e media error/cancel/stale authority; restore owner limitato. | End tardivo, cancel, error e cambio producer nell'app. |
| M37 | Parser chiusi, identità oltre2^53, slot array safe, path/hash/asset bounded; protocol/DTO/unit Python negativi. | Eseguire decoder C++/Swift e trasporto autenticato. |
| M38 | Core/anchor maximumSession, ring64 con4096 retry e nessun registro press illimitato. | Sessione 12 giri/5 supporter e lunga durata nativa. |
| M39 | Flag Relay e requirement SC4; layout1 preservato; checker combat10/defense20/feedback6 passati. | Build vanilla/Relay/SERVER_ONLY e suite SC0–SC3 compilate. |
| M40 | Reset/Pronto risultati, intro replay, lease/history e identity reset nei chiamanti reali; source lifecycle16. | Gara naturale→results→nuova gara e output perso/ripristinato. |

I 10 test offline dell'esportatore usano mock e un confine mount sintetico; nessuna risposta costituisce un vero producer/grant. Le 25 prove del validatore dichiarano fixture sintetiche e non producono un descriptor qualificato. Le prove originali di SC0–SC3 non sono state rieseguite su questo codice.
