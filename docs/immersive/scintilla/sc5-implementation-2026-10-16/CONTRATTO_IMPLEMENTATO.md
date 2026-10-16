# SC5 — Contratto del codice implementato

Versione del codice: consegna SC5 del 16 ottobre 2026. Questo documento descrive il contratto collegato dai sorgenti; build e disponibilità operativa non sono state verificate. Il [contratto allegato al piano](../sc5-plan-2026-10-16/CONTRATTO_SC5.md) resta la proposta storica. Le seguenti precisazioni fissano i dettagli adottati congiuntamente dal native e dai DTO host.

## Discovery e profili

La risposta GET `/immersive/v1/capabilities` include il campo top-level `scintilla_crew_sc5` con esattamente dieci campi: `contract`, `schemaVersion`, `implemented`, `crewRoles`, `crewHandoffVerification`, `partyBoost`, `layoutProfile`, `layoutRevision`, `producerUUID`, `profiles`. Contratto `tholus.stk-scintilla-crew/1`, schemaVersion1, layout `front_rear_crew_v3`, layoutRevision stringa `"3"`. Profili `sc5_crew_standard_v1` e `sc5_crew_party_v1`, regole revisione stringa `"1"`. La capability dichiara codice implementato, non una zona o geometria già qualificata. Campo presente ma malformato è errore di contratto; campo assente non abilita SC5.

## Endpoint e autorità

Sono collegati al router autenticato e al dispatcher game-thread esistenti: GET `/immersive/v1/scintilla/crew/state`; POST `intent`, `bindings-confirm`, `prepare` nello stesso namespace. Nessun nuovo servizio/pump. Body POST massimo16KiB; UUID minuscoli, identità uint64 come stringhe decimali canoniche, campi/liste chiusi e bounded.

Ogni POST richiede `{contract,schemaVersion,requestId,expected}`. `expected` contiene esattamente `producerUUID,raceEpoch,driverTurnId,crewIntentRevision,crewRulesRevision,rosterRevision,inputSessionID,inputEpoch,missionRevision,routeRevision,projectionContextId,layoutProfile,layoutRevision`. Le dimensioni fisiche SC1 e gli hash dei core restano ulteriori gate nativi. Solo intent può negoziare un'autorità ancora incompleta; il client parte sempre dallo stato restituito dal produttore.

Extra delle operazioni: intent ha `required,profileId`; bindings-confirm ha `challengeId,bindings`; prepare non ha extra. BindingProof esatto: `participantId,sourceId,bindingRevision,physicalFingerprint,verification,challengeSequence`. `bindingRevision` identifica l'inputEpoch corrente, `verification` è gesture/retained e sequence è uint64-stringa nonzero. Hash fisico = SHA256 UTF-8 dei cinque token reali `sourceId|deviceBindingRevision|trackingSpaceEpoch|roomCalibrationRevision|gripCalibrationRevision`; i token non possono contenere il separatore. Continuità della posa e storico perdite sono verificati separatamente: recuperare tracking non riabilita da solo un'attestazione precedente.

Cache64 requestId/body/risposta, con risposta esatta cercata prima dell'identità live. UUID riusato con altro body è conflitto409. Il retry di un vecchio prepare non apre il gate live: l'host legge uno stato nuovo e ricontrolla autorità e possesso. Una ricevuta positiva, un nuovo hit o una Task cancellata non annullano un'invalidazione già osservata.

Intent richiesto è atomico per i prerequisiti SC2–5 e lascia il mondo held. Il profilo cambia solo prima del tick di partenza, senza downstream configurato; un semplice preview mai preparato può essere disabilitato, mentre gara preparata/turno già ammesso richiedono una nuova raceEpoch. Reset conserva il grant sano, ricrea la gara nativa prima della risposta e richiede nuovamente la preparazione. Handoff A procede sull'autorità precedente; al commit il gate B si chiude prima della fisica e resta chiuso fino a rebind/possesso/preparazione/Ready esplicito.

## Stato live

Top level chiuso: `contract,schemaVersion,ok,identity,required,prepared,active,profileId,partyEnabled,bindingsVerified,reason,participants,suggestions,party,result`. `ok=true`; active implica prepared. prepared richiede SC1–4 correnti, intro esatta, HUD pronto, bindings vivi e, per Party, zone qualificate.

Partecipanti max6: `participantId,name,isPilot,competence,contributions,opportunityTicks`. Il pilota ha competence esplicitamente null. Competenze supporter: universal/hacker/gunner/defender/engineer/recon. Non sono permessi esclusivi. Contatori stringhe uint64 canoniche, saturazione diagnosticata:

- contributions: `openings,comboContributions,staggers,defenses,shellsOpened,modulesCollected,modulesRecovered,normalBoosts,partyConsents`;
- opportunityTicks: `noOpportunity,waiting,engaged,notReady,paused`.

Suggestions max5, una per supporter e nessuna per pilota: `participantId,taskKind,targetId,instance,threatId,moduleId,expiresAtTick`. Le identità non pertinenti sono null espliciti. TaskKind è offense/defense/capsule_shell/capsule_collect/tether_recover/normal_boost/party_boost; il DTO impone le combinazioni di target appropriate. Le opportunità leggono fatti reali e non autorizzano effetti.

Party chiuso: `state,reason,proposalId,deadlineTick,consentingParticipantIds,requiredCount,cost,usedThisTurn,zoneQualified,zoneAvailable,cooldownUntilTick`. Proposal e deadline sono null senza proposta corrente; consenso max2 persone distinte; costo15, quorum1 se C1 altrimenti2. zoneQualified e zoneAvailable sono distinti. Il gate runtime rilegge anche risorse/riserva, minaccia/impatti, stato fisico, veto e posizione all'ultimo consenso. Nessun POST attiva il turbo: solo input spaziale ammesso su frame v3. La quota è di producer/race/turno, non di inputEpoch o dispositivo.

Gli enum degli errori/reason sono condivisi dai DTO; rifiuti ordinari Party non revocano il grant SC1. Restano 400 schema, 401 autenticazione, 409 autorità/stato e 503 capacità indisponibile, secondo il router esistente.

## Report terminale

`result` è null oppure l'envelope congelato comune a crew/state, report sessione GET e JSON persistito. Campi esatti:

```
schemaVersion,producerUUID,raceEpoch,missionRevision,finishSequence,finishTick,
profileId,crewRulesRevision,partyEnabled,bucket,completedHandoffs,partyActivations,
rotationAnomalies,comparable,physicsFPS,countersSaturated,parameters,participants
```

I parametri congelati sono tredici: i nove tick stringa `suggestionIntervalTicks,suggestionHoldTicks,urgentTetherTicks,maximumWaitTicks,partyHoldTicks,partyWindowTicks,partyCooldownTicks,partyBurstTicks,partyFadeTicks`; più gli interi `partyCost=15,partySpeedDeltaMps=1,partyRequiredCount=1/2` e `partyZoneManifestSHA256`. F=physicsFPS in60…1000. Formule intere: ceil(F/4), ceil(3F/5), ceil(6F/5),30F,ceil(3F/4),3F,6F,floor(F/2),floor(F/4). Hash zona può essere vuoto solo quando la qualifica non è disponibile; non rende confrontabile un risultato Party.

Le tuple race/missionRevision/finishSequence/finishTick devono coincidere con il risultato SC4; producer e roster sono esatti. participants usa la stessa copia congelata dello stato terminale. Il report sessione aggiunge `crew` e `leaderboard_bucket`. I gruppi sono `scintilla_sc5_standard_v1` e `scintilla_sc5_party_v1`, indipendenti da partyActivations; non sostituiscono le altre dimensioni pista/difficoltà/P/regole. Fine artificiale, deviazioni amministrative, test o saturazione non sono confrontabili. Freeze naturale avviene prima degli effetti tardivi. Retry/GET/reset non ricostruiscono il vecchio record.

Persistenza opt-in nella directory report già configurata: nome bounded con sessione sanificata/fingerprint e producer/race/missionRevision/finishSequence, creazione esclusiva senza sovrascrivere un archivio precedente. L'host conserva il rapporto precedente al reset e rifiuta risposte tardive di altro produttore o mutazioni della stessa gara congelata.

## Layout, intro e qualifiche

Context PartyBoost è aggiunto in coda, senza rinumerare SC1–4. Layout v3 riserva il cerchio(0,22;0,85),r0,06 in entrambi i profili, senza fallback world quando disabilitato. Frame storico e validità corrente includono race/turno/intent/proposal e input/roster. WorldOffense non dipende da un nuovo threatId estraneo al proprio contesto.

Intro SC5 usa manifest/2 con profilo/regole/layout3 e sei pannelli per ciascuna variante C1/C2/C3plus. Due manifest e sei video reali distinti standard/Party; receipt vincolata a producer/race/manifest/variant/profilo. Le callback host aggiungono l'autorità effimera crew completa. Fine/skip/errore richiedono il nuovo ciclo di render ACK prima del ritorno al gioco.

SC5 usa `tholus.stk-scintilla-measurement/2` e schema2, mantenendo /1 per SC4. Il fingerprint chiuso aggiunge `crewRulesRevision,zoneManifestHash,partyEnabled,partyCost,partyGain,partyHoldTicks,partyWindowTicks,partyDurationTicks,partyFadeTicks,partyCooldownTicks`; layout valutato3 e actualLayoutRevision3. I sei speedIncreaseTicks restano invariati; stress Party richiede l'inviluppo di velocità effettivo B+1. Disponibilità della prova `[readbackUs,captureUs+50000]`, senza accettare risultati futuri o latenze eccessive. Profilo, binario, asset e maschere devono corrispondere; vecchi report SC4 non qualificano SC5.

Qualifica geometria v3 e zone Party hanno protocolli separati e artefatti hash verificati, descritti nelle due guide tools. Shipping manifest zone: development_unqualified, sei candidati e zero intervalli operativi. Nessuna qualifica nativa è stata generata in questo incarico.
