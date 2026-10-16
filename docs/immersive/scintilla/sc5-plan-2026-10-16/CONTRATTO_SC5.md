# Contratto proposto SC5 — versione 1

Questo allegato definisce **nuove API da implementare**, non endpoint disponibili. Prevale il [piano](PIANO_SC5.md) per semantica e ordine. Nessun campo client autorizza direttamente un effetto fisico.

## Trasporto e identità

Namespace `/immersive/v1/scintilla/crew/`, stesso trasporto/token/coda game-thread delle API Scintilla. GET `state`; POST `intent`, `bindings-confirm`, `prepare`. Body massimo16KiB. Contratto `tholus.stk-scintilla-crew/1`, schemaVersion intero1, requestId UUID minuscolo. Nessun nuovo server.

Ogni POST ha campi comuni esatti `{contract,schemaVersion,requestId,expected}` più quelli specifici sotto. `expected`:

```
producerUUID, raceEpoch, driverTurnId, crewIntentRevision, crewRulesRevision,
rosterRevision, inputSessionID, inputEpoch, missionRevision,
routeRevision, projectionContextId, layoutProfile, layoutRevision
```

Tutte le revisioni, race, turn e inputEpoch sono stringhe uint64 canoniche; layoutRevision segue la rappresentazione canonica delle identità missione. producerUUID è UUID, inputSessionID/route/projection sono token limitati come SC4; campi non preparati possono essere vuoti/zero solo in `intent`. Valori non esistenti si ricavano da GET, non dal numero di callback. Il native confronta anche gli hash qualificati correnti nel gate finale; un client non ne sceglie una versione alternativa. Le dimensioni di autorità esistenti SC1/SC4 omesse dall'envelope di comodità restano imposte dai core proprietari.

Retry: conservare lo stesso requestId/body se esito incerto; ricevuta esatta prima del confronto con stato già avanzato; stesso UUID con altro body=409. Cache64, non registro crescente. Una ricevuta di prepare passata non riapre il gate dopo una sospensione: il client legge stato corrente e il server ammette la gara solo tramite gate live.

## Operazioni

| Operazione | Extra body | Regole |
|---|---|---|
| `intent` | `required: Bool`, `profileId: String` | Solo pre-gara/held; latche SC2/3/4/5 atomicamente. `required=false` ammesso solo quando nessuno stato configurato è attivo secondo policy esistente; nessun downgrade in gara. Profilo standard/Party esatto del piano. |
| `bindings-confirm` | `challengeId: UUID`, `bindings: [BindingProof]` | Solo dopo rebind con nuova autorità, SC1 vivo e mondo held; stesso set sorgenti concesso e stessa mappa corrente. Questa ricevuta attesta solo possesso/roster, non successo ottico o preparazione. |
| `prepare` | nessuno | Held, SC1–4 correnti, intro della variante completata, layout v3 qualificato, input neutro, bindings verificati sulla stessa autorità e Party zones ammesse se profilo Party. Se stessa gara/turno preserva quota/crediti. |

`BindingProof` esatto:

```
participantId: token
sourceId: token
bindingRevision: uint64-string
physicalFingerprint: sha256-lowercase
verification: "gesture" | "retained"
challengeSequence: uint64-string
```

È un'attestazione dell'host autenticato proprietario del roster, non una prova di identità biologica. Native confronta physicalFingerprint ricavato dalle dimensioni di sorgente/calibrazione già nel grant; non accetta un fingerprint inventato dal payload. Per `gesture`, l'host conserva il challenge corrente e solo un evento post-neutralità fresco, completo press-release. `retained` ammesso esclusivamente per la stessa mappa persona→sorgente già verificata nella gara e senza perdita dell'autorità fisica; all'avvio tutte gesture, al cambio almeno sorgente trasferita gesture. Le altre sorgenti devono comunque essere neutre/vive; se l'host non può provare continuità richiede gesture. ChallengeSequence è seriale dell'attestazione per quel challenge, non sostituisce la sequenza SI5; non inoltrare prove raw arbitrarie come input gameplay.

Non duplicare il calcolo del physicalFingerprint: serializzazione canonica delle revisioni reali già usate da `samePhysicalSources`, stessa funzione condivisa in test; native la ricava dai campi del consumer. Se oggi non sono tutti esposti nel binding native, aggiungere il minimo necessario al rebind e alle prove del grant, non fidarsi del solo hash passato dall'host.

## Snapshot chiuso

`state` e risposte riuscite POST restituiscono stesso modello tipizzato:

- `contract,schemaVersion,ok=true`;
- `identity`: tuple expected corrente;
- `required,prepared,active`, `profileId`, `partyEnabled`, `bindingsVerified`, `reason` enum;
- `participants`: massimo6, id/name/isPilot, competence nullable enum, contatori utili per tipo;
- `suggestions`: massimo5, participantId, taskKind, targetId/instance nullable, threatId/moduleId nullable, expiresAtTick stringa. Schemi discriminati: niente combinazioni incoerenti;
- `party`: `state`, `reason`, `proposalId`, `deadlineTick`, `consentingParticipantIds` max2, `requiredCount`1/2, `cost`15, `usedThisTurn`, `zoneQualified`, `zoneAvailable`, `cooldownUntilTick`;
- `result`: null oppure envelope SC5 congelato associato all'esatto risultato SC4.

Nessun campo `prepared=true` con authority incoerente, result terminale o gate mancante. `active` implica prepared+gara attiva. `partyEnabled` è proprietà del profilo immutabile, `zoneAvailable` è corrente. Non mettere token autenticazione nel log/manifest/report.

Errori dominio: `crew_not_required`, `crew_preflight_required`, `crew_identity_changed`, `crew_bindings_unverified`, `crew_profile_locked`, `party_zones_unqualified`, `party_disabled`, `party_used_this_turn`, `party_reserve_protected`, `party_consensus_expired`, `party_release_required`, `party_driver_veto`, `party_zone_unavailable`, `service_busy`. Quelli di input rimangono nel feedback sorgente; non revocare l'intero grant per un normale rifiuto Party. Errori HTTP:400schema,401autenticazione secondo server esistente,409stato/identità,503capability assente. Non introdurre altri codici senza aggiornare DTO/test/copy.

## Intro, resa e compatibilità

Aggiungere capability esplicite `crewRoles`, `crewHandoffVerification`, `partyBoost` e versione layout riconosciuta; non dedurre disponibilità dalla versione app. Le vecchie richieste SC1–4 mantengono schema e comportamento. I nuovi profili hanno le loro varianti intro/manifest qualificate; standard/Party non possono condividere una ricevuta intro se l'informazione sul turbo cambia.

La fotografia SC4 identifica l'intro tramite producer/race/manifest/variant. Per SC5 aggiungere profilo/regole alla selezione e al receipt o usare un manifest/variant distinto, senza accettare il completamento di una variante standard per Party. Non riusare `sameAuthority` vecchia senza includere turno/crew intent nei nuovi DTO.

Il report SC5 è un envelope nuovo aggiunto al report sessione, non una modifica retroattiva al significato del risultato SC4. Caso old host/new native: nessuna attivazione SC5 se le capability non sono negoziate. Caso new host/old native: selezione SC5 non pronta con motivazione, SC4 rimane un'opzione esplicita separata.
