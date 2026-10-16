# SC3 — Contratto additivo, schema 1

Namespace autenticato `/immersive/v1/scintilla/defense/`; GET `state`, POST
`intent` e `prepare`, tutti sul thread gioco. Contract `tholus.stk-scintilla-defense/1`.

`intent`: oggetto chiuso `{contract,raceEpoch,intentRevision,required}`. ID64bit
come stringhe decimali. L'intent SC3 richiede SC2; non può essere rimosso da gara
configurata. Reset mantiene la scelta fino a cambio profilo esplicito.

`prepare`: `{contract,profile,inputSessionID,inputEpoch,raceEpoch,rosterRevision,
combatProfile,layoutProfile,layoutRevision,routeRevision,projectionContextId,
visibilityEvidenceID,visibleRegions}`. Profile `sc3_sector_rear_readonly_v1`,
combatProfile `crew_standard_sc2_v1`, layoutProfile `front_rear_readonly_v1`,
layoutRevision `1`. `visibleRegions`: 1–8 rettangoli UV chiusi
`{left,top,right,bottom}`, finiti in [0,1], con area positiva. La prova di
commissioning identifica il mapping e le misure di commissioning; il producer controlla
copertura di retro/nodi/barre/badge e binding al route/context corrente.
Questo controllo di dichiarazioni software non certifica visibilità fisica.

Preparazione soltanto a mondo locale fermo, SC2 e input correnti. Retry identico
non ricarica risorse. Cambi di input/roster/layout invalidano la preparazione;
la policy richiesta resta attiva e blocca start/resume finché ripristinata.

State include required/prepared/policyActive, intentRevision, raceEpoch,
rosterRevision, inputSessionID/inputEpoch, profile/layoutProfile/layoutRevision,
routeRevision/projectionContextId, layout{width,height,rearRect:[u0,v0,u1,v1]},
capabilities{rear_alert,rear_visible_view,rear_targetable_view:false,
projectile_interception:false}, blockedReason, energy,reserve,heat, phase,
threatId,sector,attackerId,deadlineTick,simulationTick,cooldownUntilTick.
I valori phase/status seguono gli enum C++ stabili; feedback sorgente separati
per tool SC3 preservano il canale SC2 e i feedback primari transitori.

Le prove di presentazione continuano a transitare tramite SC1. Frame e layout
producer sono l'autorità su threat/fase/contact e sul contesto del pixel;
il client non può dichiarare un allarme visto o scegliere un target semantico.

## Stato e recupero

Campi additivi: `channelOwner`, `boostOwner` (indice della crew ordinata, −1
quando assente), `boostStatus`, `boostCost` (numero intero, 25 nel profilo v1),
`statistics{cancelled,blocked,impacts,missed}`
(contatori saturanti, stringhe decimali64bit), `lastReason` e
`impactEvent{raceEpoch,threatId,humanInstance,tick}`. Quest'ultimo descrive l'ultimo
impatto applicato, resta idempotente sulla coppia race/threat e non modifica il
carico. Energia/calore/cooldown/statistiche appartengono alla gara, non al grant.
`boostCost` deriva dalla stessa costante di profilo usata per ammissione e addebito;
il producer la invia anche allo shader per il costo nel nodo e la quota sulla barra.
L'aggiunta è compatibile con i client schema 1 che ignorano campi di risposta nuovi.

Errori400: schema/valori/profilo non validi;409: autorità cambiata, gara non locale
ferma, preparazione precedente mancante, layout non coperto o audio assente.
Nessun errore cancella l'intent richiesto. Riutilizzare lo stesso evidenceID con
regioni diverse su una preparazione corrente produce
`defense_visibility_evidence_changed`. Le regioni devono contenere anche l'intera
impronta visuale della sorgente e dell'impulso, altrimenti non possono sostenere
la minaccia nel main. Il retro è già interamente coperto durante prepare.

`POST /immersive/v1/spatial/rebind`, autenticato/sul game thread e ammesso soltanto
con SC3 richiesta a mondo fermo, usa il contratto spatial esistente:

```json
{
  "contract": "tholus.stk-spatial/1",
  "inputSessionID": "same-si5-session",
  "routingGeneration": "same-si5-generation",
  "expectedInputEpoch": "1",
  "expectedRosterRevision": "2",
  "bindings": [{"sourceId": "same-granted-source", "participantId": "current-crew-person"}]
}
```

Schema chiuso, stesse sorgenti concesse e biiezione sulla crew corrente.
Risposta200: `{ok:true,accepted:true,inputSessionID,inputEpoch:"2"}`. Retry identico
con epoch precedente e stessa tupla restituisce lo stesso ack senza incrementare.
La lease e il clock devono essere ancora validi. Un altro epoch/roster dà409;
schema/sorgente estranea dà400. Il controllo del gate held è dell'adattatore server,
non del consumer puro. Non cambia le identità SI5 né rinnova la lease. Mantiene
sequenze, UUID anti-replay e storico, ma cancella pending/held e aumenta inputEpoch.
Servono SC2→SC3 prepare correnti e una nuova coppia rilascio/pressione. Stati
canonici, cancel, renew e presentation continuano mentre aim/action danno
`participant_rebind_required`. Cambiare il set fisico o le calibrazioni richiede
il lifecycle SI5 già esistente, non questo comando.

### Recupero dopo reset della stessa gara configurata

Il reset in-place (`RelayRaceMode::reset`, da operatore o Pronto nei risultati)
con SC3 richiesta conserva il grant fisico SI5 finché valido. Revoca tutte le
ammissioni gameplay: pending/held, risultati GPU, target/istanze e storia ottica
precedenti non passano alla nuova raceEpoch. Anche con crew invariata serve
`spatial/rebind` → SC2 prepare → SC3 prepare, con inputEpoch incrementato al rebind.
Il nuovo core parte con E100/H0; pausa e handoff mantengono invece il budget.

Stream e frameId monotoni appartengono allo stesso produttore e rimangono stabili
nel reset in-place. Il consumer scarta la vecchia storia: un update già in volo
con quei frame riceve `presentationAccepted:false` e `frame_history_unavailable`,
senza revocare il grant. Prove nuove devono riferirsi ai nuovi frame. Sequenze,
registro anti-replay, continuità fisica, lease e clock restano autoritativi;
release/revoca/scadenza/cambio stream o route non vengono recuperati dal reset.
Distruzione/ricreazione del mondo e cambio della configurazione fisica mantengono
l'invalidazione completa e il lifecycle SI5 esistente.

Pronto nei risultati può eseguire il solo reset con video held; non avvia né
accoda il countdown. Ogni Ready/start/resume successivo mantiene i gate SC2/SC3
e presentazione. L'host sospende le interazioni prima della richiesta e conserva
il blocco anche su rifiuto o risposta incerta, fino a nuova preparazione esplicita.
