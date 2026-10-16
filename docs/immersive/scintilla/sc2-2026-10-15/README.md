# SC2 — Implementazione sorgente, 15 ottobre 2026

Stato: implementazione a livello codice secondo il
[piano SC2](../sc2-plan-2026-10-15/PIANO_SC2.md).
**Build, test C++/Swift, shader, gioco, Syphon e hardware non eseguiti.**
SC3 non avviata. I controlli Python sorgente/matematici non certificano il binario.

## Consegna e ordine dei passaggi

| Passo | Consegna | Ricontrollo effettuato |
|---|---|---|
| 0 | Worktree STK e host su SSD con copia della base SC1 e manifest del WIP | Hash prima della copia e controllo di conflitti prima della restituzione |
| 1 | Core C++11 puro `scintilla_combat`: 5 / A+B / A+B+C, tick, transazioni e recovery | Revisione confini semiaperti, persone distinte, duplicati, attesa scaduta; test scritti |
| 2 | Coda affidabile drain-once con seriale, tempo di ammissione, race/input epoch e snapshot storici | Consenso di tutti i frame anche su fase/finestra, FIFO indipendente dalle fence, cancel prioritari |
| 3 | Prove aim continue nello stesso resolver SC1 | Una query in corso e un campione coalescibile per sorgente, freschezza 60 ms, nessun regresso da pressione più vecchia |
| 4 | Commit esclusivamente in `RelayRaceMode::update` | Freeze, istanze vive, recovery persistente ai guasti input, completamento fisico osservato |
| 5 | Factory Relay di `ExplosionAnimation`, policy separata da vanilla | Nessun clear attachment/invulnerabilità globale/reset camera; volume di clearance, fine superata, teardown |
| 6 | Powerup confinati al profilo gara | Umano/AI: zipper da box, nitro conservato, banane anvil/parachute senza bombe, guard inventario/uso/trasferimento |
| 7 | API additiva e scelta diagnostica/SC2 nell'app; feedback e anelli | CAS dell'host, stato autorevole prima dell'avvio, compatibilità legacy esplicita, glyph distinti e depth condivisa |
| 8 | Regressioni scritte, controlli Python e checkpoint | Vedere [verification.json](verification.json); nessuna esecuzione nativa |

## Regole e scelte precise

I tempi provengono dal piano: acquisizione 0,6 s (72 tick a 120 Hz), breccia
3,5 s (420), B→C 1,25 s (150), gap solo 2,5 s (300), catena solo 8 s (960),
stagger obiettivo 1 s (120), recovery 4 s (480) dalla fine osservata dell'effetto.
La frequenza è letta dalla configurazione e congelata; i minimi usano ceil,
i massimi floor. Le finestre massime sono semiaperte. Cadenza primaria: il
controllo esistente SC1 da 250 ms, senza secondo limiter. Niente autofire.

Le scadenze dei contributi usano il tick di ammissione sul thread gioco, prima
della GPU; anche il commit deve avvenire in una finestra ancora aperta. Una posa
storica non riapre una breccia. Per la cooperazione, ammissione, tutti i frame
plausibili e commit devono identificare la stessa finestra. Le primarie non si
riordinano secondo le fence. Release/cancel interrompono il mantenimento prima
del commit; il log diagnostico circolare non è una coda gameplay.

Il fascio usa visibilità storica verificata e l'estremo prudente del tempo della
posa; dopo 60 ms non mantiene nulla. La GPU non rinnova l'età del campione.
Una perdita reale chiude la finestra senza ridurre la crew; serve rilascio e nuova
pressione. I salti di update non accreditano acquisizione non dimostrata.

Il core conserva immunità e policy per la vita della gara. Invalidare il grant
cancella le finestre ma non azzera recovery, non interrompe una conseguenza già
applicata e non riattiva le armi vanilla. Reset mondo è il confine del profilo.
La prima attivazione SC2 richiede tempo gara ancora zero; evita armi native già
in volo. Una gara SC2 già attiva può riconvalidare l'input in hold senza perdere
le immunità, se il sistema Spatial Input permette il nuovo grant.

La sfera visibile del kart resta r=0,35 m. Scelta di bilanciamento iniziale:
semidiametro minimo 0,75°, massimo 30 m, limite effettivo circa 26,738794 m.
Il resolver e lo snapshot del marker condividono il filtro. Non si ignora un
marker più vicino non attaccabile per colpirne uno dietro; depth/clipping SC1
rimangono conservativi. I box bonus conservano solo la diagnostica geometrica.

Stagger: `w(u)=16u²(1−u)²`, altezza massima 0,25 m, pitch massimo 8°, posa iniziale
ripristinata e velocità azzerate alla fine. La geometria è controllata con un
volume conservativo, overlap iniziale e sweep; si ignorano il kart stesso e la
superficie di supporto. Il pitch è ulteriormente limitato da `height/radius`
per contenere lo spostamento dei vertici nella risalita. Se la clearance non è
provabile, restano arresto e feedback, con altezza/pitch nulli; il contatore
`suppressedStaggerMotion` espone il caso. La factory rifiuta kart incompatibili,
finito/eliminato, network, rewind e replay/registrazione non supportati.

## Contratto API additivo

Tutte le route sotto `/immersive/v1/scintilla/` sono autenticate e passano dal
thread gioco, anche GET. La superficie `/spatial/*` e il contratto comune
`tholus.spatial-input/1` restano distinti.

- `GET /immersive/v1/scintilla/state`: contract `tholus.stk-scintilla-combat/1`,
  profile `crew_standard_sc2_v1`, `raceEpoch` e `rosterRevision` stringhe decimali,
  `inputSessionID` stringa, `prepared`, `policyActive`, `simulationTick`,
  `suppressedStaggerMotion`, `targets` (massimo 32).
- `POST /immersive/v1/scintilla/prepare`: esattamente `contract`, `profile`,
  `expectedRaceEpoch`, `expectedInputSessionId`, `expectedRosterRevision`.
  Le identità expected provengono dal GET; niente bersaglio, durata o potenza
  arbitrari dal client. Reply: stato SC2, `prepared=true` quando convalidata.
- Identità cambiate: 409 `combat_identity_changed`; prima attivazione dopo
  l'avvio: 409 `combat_requires_race_reset`; contesto incompatibile: 409
  `combat_requires_prepared_local_world`; payload/profilo invalido: 400.
- Le capabilities espongono `scintilla_combat_sc2` come implementata nel sorgente,
  con `physical_qualification: pending`.

L'app prepara prima il grant SC1, poi legge/riconvalida queste identità e prepara
SC2. Verifica proprietà del produttore, sessione, driver, roster, input, route,
configurazione e scelta attraverso gli await. Start/countdown/resume rileggono
lo stato; la scoperta di una policy già attiva aggiorna la UI e richiede un nuovo
avvio consapevole. Il server conserva un proprio gate per la policy agganciata.
Vecchi producer diagnostici sono riconosciuti solo da specifiche risposte legacy
negative autenticate, mai da timeout o risposte ambigue.

## Risorse e limite d'integrazione

Le query restano limitate a 64, depth a 192 MiB e al massimo 12 catture, metadati
a 64 frame. Ogni candidato conserva il risultato depth già letto. Lease del
consumer impediscono lo sfratto per pressione di una query ancora necessaria;
solo un risultato completato e non posseduto può liberare capacità. Una fence
non pronta non si cancella per fare posto. Le primarie si risolvono prima delle
nuove mire; saturazione genera attesa limitata/rifiuto, senza fallback.

**Limite preesistente SI5 mantenuto:** dopo la chiusura di un endpoint già
concesso, l'host richiede ancora l'uscita del produttore prima di creare un nuovo
canale. Pausa e tracking recuperabile mantengono il grant esistente. SC2 non
aggira questa policy di isolamento. Una rotazione completa di driver con nuova
assegnazione dei controller rimane da chiudere nell'integrazione SC5; il freeze
e la cancellazione durante handoff sono già rispettati. Non dichiarare quel
percorso dimostrato da questi sorgenti o dalle fixture.

## Copertura delle fixture del piano

Questa tabella indica i sorgenti che verificano i casi, non test eseguiti.

| Casi piano | Sorgenti/prove disponibili |
|---|---|
| M01–M07, M20 | Test core soloFiveAndRecovery, soloDeadlineAndMissIsolation, sustained load; ordine/deadline nelle regole |
| M08–M18 | Test core noPrebankAndNoMixedWindows, twoCrewNeedsOneNonOpener, distinctShootersAndBoundaries, repeatedEventAndSameTickCompleteOnce; test input historical windows/cancel |
| M19, M21–M24 | Test input FIFO GPU, beam freshness, older reliable press, cache ownership; test core release latch/acquisition gap |
| M25–M28, M30–M31, M41, M44 | Test core loss/target isolation, incompatible instance, pause/reprepare/recovery, bounded reset, 100000 azioni; regressioni SC1 lifecycle/retirement |
| M29, M32–M36, M38–M40, M43 | Test core effect confirmation; regressioni SC1 clipping/depth/readback; dispatcher GET; controlli sorgente della factory, scope e gating. L'effetto fisico e il pool reali richiedono ancora test nativi/integrati |
| M37 | Matematica range e ammissione condivisa nel controllo Python, fixture core geometryAndCurve |
| M42 | Regressioni Swift HTTP/CAS/reset-required aggiunte al test esistente |

Resta necessaria la compilazione dei test scritti e l'esecuzione integrata di
factory/physics, pool oggetti, rendering/shader, world reset, pausa/ripresa e
pressione GPU. Le fixture pure non eseguono il distruttore fisico del kart né
certificano una camera immobile o l'assenza di regressioni del binario vanilla.

Hardware/venue: P=3 con due puntatori e P≥4 con almeno tre, camera in movimento,
output proiettato reale, latenza, usabilità e costo di gara del sobbalzo. Nessuna
pretesa di bilanciamento da «1 secondo»: la riaccelerazione può costare di più.
