# SC5 — Piano esecutivo: competenze, opportunità, cambio pilota e Party

Data: **16 ottobre 2026**. Destinatario: LLM implementatore.

**Consegna di questa cartella: piano e calcoli, non implementazione SC5.** Procedere nella sequenza della sezione 15, completando e ricontrollando ogni passo prima del successivo. Non avviare SC6. Le verifiche native, build, avvii e prove fisiche dipendono dall'autorizzazione della futura richiesta: questo documento non la concede. Per un incarico «senza build» scrivere e revisionare i test, eseguire solo i controlli consentiti, e lasciare esplicitamente aperte le verifiche native.

Fonti: [design v2](../../../Tholus_STK_Missione_Scintilla_Design_v2.md), §§7, 8, 12, 14–17; [piano SC3](../sc3-plan-2026-10-15/PIANO_SC3.md); [piano SC4](../sc4-plan-2026-10-16/PIANO_SC4.md); [tre correzioni SC4](../sc4-audit-fixes-2026-10-16/README.md). Riferimenti della baseline e hash: [source-manifest.json](source-manifest.json). Matematica: [calcoli.json](calcoli.json), riproducibile con [verifica_calcoli.py](verifica_calcoli.py). Regressioni richieste: [MATRICE_TEST.md](MATRICE_TEST.md).

## 1. Risultato richiesto e decisioni fissate

SC5 aggiunge un livello di coordinamento alle meccaniche SC1–SC4. Restano invariati la gara nativa, la camera stabilizzata, la vittoria `naturalFinish && officialPosition == 1 && securedSlots == 3`, il pairing e la calibrazione fisica.

Consegnare:

1. Competenze consigliate per persona, kit universale, rotazione deterministica; nessun bonus statistico nella prima versione.
2. Opportunità concorrenti reali e suggerimenti proporzionati a C, senza un nuovo minigioco esclusivo o un encounter che blocca tutte le altre attività.
3. Cambio pilota completo, leggibile e recuperabile, con assegnazione dei puntatori, verifica del possesso e riarmo; risorse/stato persistente preservati.
4. Variante Party esplicita, disattivata per default, con turbo deliberato di squadra, quota per turno, costo e consenso distinti dal boost SC3.
5. Feedback e rapporto che riconoscono contributi reali, separando i risultati Party e standard.

Primo profilo: P=2–6, C=P−1, pista normale `snowtuxpeak`, stessi vincoli locali SC2–SC4, difficoltà easy e adaptive off. Identificatori proposti **nuovi**, non già disponibili: `sc5_crew_standard_v1`, `sc5_crew_party_v1`. Entrambi richiedono SC1–SC4; il secondo aggiunge Party. Selezione immutabile durante la gara; cambi consentiti prima della partenza o nella configurazione della gara successiva.

Non abilitare il `crewMode` telefonico legacy. Non introdurre nuovi powerup vanilla, classi con abilità esclusive, navigatori con scorciatoie inventate, nuovo driving AI, tiro nel retrovisore, intercetto dei proiettili o ruoli che suppliscono a identità mancanti. Il ricognitore segnala/raccoglie opportunità reali visibili.

I valori Party e la frequenza dei suggerimenti sono **proposte iniziali di bilanciamento**, non misure scientifiche di capacità umana o comfort. Le formule dimostrano proprietà del modello indicato, non la sicurezza fisica o la piacevolezza della proiezione.

## 2. Baseline e mappa concreta

STK: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code`.

Host: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController`.

`IR/` qui abbrevia `src/modes/immersive_relay/` nella radice STK. Leggere gli AGENTS applicabili. Il checkout contiene molto WIP SC0–SC4 non committato: un worktree da HEAD **non contiene questa baseline**. Confrontare il manifest, importare solo la baseline necessaria con hash verificati e preservare il lavoro concorrente. Non copiare binari/cache come sorgenti. Tutti i nuovi artefatti controllabili vanno sul SSD montato e scrivibile.

| File/simbolo attuale | Uso previsto |
|---|---|
| `IR/relay_pilot_rotation.*`, `finishHandoff`, `getSnapshot` | Unica autorità della rotazione; espone `m_completed_handoffs`, driver e prossimo driver. Non creare un secondo timer di cambio. |
| `IR/relay_session_controller.*`, `getRecommendedLapCount`, `isWorldHeldLocked`, `getSpatialCrewIDs`, report | Congelamento mondo, roster nativo, turni e rapporto; gate SC5 lungo tutti gli ingressi. |
| `IR/crew_role_manager.*`, `crew_event_director.*` | Sistemi legacy dipendenti da telefoni/azioni semantiche: conservarli, non attivarli per SC5. |
| `IR/spatial/scintilla_engine.cpp`, `refreshRoster`, `updateCombat`, `refreshCombatInput` | Adattatore autoritativo unico: snapshot fatti, dispatch input, conferme fisiche e gate. |
| `IR/spatial/scintilla_input.*` | Frame storici, prove, release, epoch, negative sticky, drain unico; aggiungere contesto Party senza aggirarli. |
| `IR/scintilla_combat.*`, `scintilla_defense.*`, `scintilla_mission.*` | Proprietari delle rispettive regole. Il coordinatore legge fatti e riceve commit; non ricrea i tre core. |
| `IR/scintilla_defense_physics.cpp`, `applyBoost`, `eligibleHuman` | Boost SC3 già corretto per applicazione dopo Bullet; estrarre solo i parametri comuni alla primitiva Party. |
| `IR/scintilla_layout.hpp`, `data/shaders/utils/immersive_scintilla.frag` | Unica geometria HUD/picking; introdurre layout v3 preservando v1/v2. |
| `IR/spatial/scintilla_measurement_native.hpp`, `IR/scintilla_mission_measurement.hpp` e validator Python | Estendere qualifica alla maschera v3; non riutilizzare un descriptor v2 come prova v3. |
| Host `ImmersiveSuperTuxKartRuntimeController.swift` | Selezione, intent, Ready, handoff, reset, sequenza prepare; mantiene grant nella sospensione logica. |
| Host `ImmersiveSTKSpatialInput.swift` | `rebindParticipants`, `samePhysicalSources`, pump unico: verificare possesso nel pump, non aprire un secondo consumer SI5. |
| Host `ImmersiveSTKSpatialPreparation.swift`, `Workspace/GamesSTKSpatialInputView.swift`, `GamesWorkspaceStore.swift` | Associazioni persona→sorgente e stato intermedio del cambio; UI esplicita, nessun automatismo da indice display. |
| Host `Packages/TholusScintillaMission`, `ImmersiveSTKMissionHTTP.swift` | DTO/validazione/reducer puri nel package esistente; orchestration nell'app, niente dipendenza package→app. |
| Host `ImmersiveSuperTuxKartAPIModels.swift`, `Localizable.xcstrings` | Rapporto tipizzato e testi EN/IT, inclusi errori/accessibilità. |
| `tools/immersive/tests/CMakeLists.txt`; host `scripts/ci/test_app_isolated.py` | Registrare realmente i casi aggiunti. Non basta creare un file di test. |

App verificata: Swift 5, default isolation MainActor, Approachable Concurrency; deployment macOS 26.2. Package missione: Swift tools 6, macOS 14. Non uniformare globalmente versioni/isolation. Prima delle modifiche Swift applicare le skill disponibili pertinenti a concorrenza, UI macOS e test, rispettando framework e vincoli della richiesta.

Baseline verificata da sorgente, non certificazione runtime: l'ultimo audit SC4 non ha individuato altri difetti concreti; build/test Swift-C++ aggiornati, qualifica geometrica reale e ciclo completo rimangono da verificare. Non chiamare SC5 pronta operativamente sulla sola base di quel risultato.

## 3. Modello dati e responsabilità

Proporre `IR/scintilla_crew.hpp/.cpp`: piccolo core C++11 puro, senza GL/Bullet/HTTP; include coordinamento, assegnazioni e macchina Party. Separare i tre sottostati senza costruire un framework di plugin. Limitare roster a 6 e supporter a 5, opportunità a 7 bundle, suggerimenti a 5.

Identità autoritative:

- `producerUUID`, `raceEpoch`, `crewRulesRevision`, `crewIntentRevision`;
- `driverTurnId`: contatore della **gara**, derivato dai cambi realmente completati, inizia da 0;
- `inputEpoch`, `rosterRevision`, `participantId`, `sourceId` e revisioni fisiche preesistenti;
- `proposalId` Party monotono per gara; target/evento SC2–SC4 conserva id+instance+window/phaseRevision.

Usare uint64 verificati e stringhe decimali canoniche sul wire; niente Double per identità. Rifiutare overflow. La quota è `(producerUUID,raceEpoch,driverTurnId)`: mai sorgente, dispositivo, stato UI, inputEpoch o una richiesta HTTP.

`driverTurnId` avanza una sola volta quando la rotazione nativa completa un vero cambio di driver. `confirmHandoff`, retry, rebind, manutenzione, route change, pausa e source reconnect non lo avanzano. `skipHandoff` oggi può completare il cambio: conta il **risultato nativo**, non il nome del comando. Un salto amministrativo che cambia realmente pilota crea un turno e rende il report non confrontabile con una rotazione normale; non offrire il comando come ricarica Party. Immutabile una volta osservato un turno, salvo nuova raceEpoch.

Stato persistente: competenze/turno, uso Party del turno, contatori di contributi, risorse SC3 e stato SC4 già posseduti dai core. Stato effimero: suggerimenti, press/hold, consenso, prove e verifica del possesso; si cancella ai confini di autorità. Snapshot massimo O(P+7), niente storico illimitato di messaggi.

## 4. Matematica della rotazione delle competenze

Ordine dei partecipanti = roster nativo congelato, con indici 0…P−1. Per il turno t e pilota effettivo d:

`supporter[j] = roster[(d + 1 + j) mod P]`, per `j=0…C−1`.

C≤2: tutti **universali**, nessuna gestione di cinque professioni. C≥3: ruolo consigliato

`r(j,t) = (j + C * floor(t/P)) mod 5`.

Ordine ruoli: 0 hacker, 1 cannoniere, 2 difensore, 3 ingegnere, 4 ricognitore-recuperatore. Per C=3–4 il primo ciclo mostra rispettivamente i primi 3/4, il secondo ruota secondo la formula. Ogni persona mantiene sempre tutte le azioni consentite. Il ruolo è un consiglio e un'etichetta, non un permesso o una priorità esclusiva sul target.

Dimostrazione nel caso di rotazione round-robin regolare: in un ciclo di P turni ogni persona guida una volta e occupa ogni j una volta. In due cicli le sue C+ C esposizioni sono gli interi 0…2C−1 modulo 5. Con C=3,4,5 coprono tutte le cinque competenze, con differenza massima di una esposizione fra ruoli. Ad ogni singolo turno i C valori sono distinti perché C≤5. Un handoff amministrativo può rompere l'equità teorica; registrarlo, non alterare la storia per fingere equità.

| P | C | Giri attuali verificati | Turni guida/persona | Turni supporto/persona | Esposizioni ai ruoli dopo gara regolare |
|---|---|---|---|---|---|
| 2 | 1 | 6 | 3 | 3 | universale |
| 3 | 2 | 6 | 2 | 4 | universale |
| 4 | 3 | 8 | 2 | 6 | 2,1,1,1,1 |
| 5 | 4 | 10 | 2 | 8 | 2,2,2,1,1 |
| 6 | 5 | 12 | 2 | 10 | 2,2,2,2,2 |

La tabella deriva da `getRecommendedLapCount`; non cambiare i giri per far tornare il piano. Pubblicare assegnazioni solo nel confine held del turno, non a ogni aim o perdita tracking. Il decremento del numero di dispositivi disponibili non modifica C o le soglie SC2.

## 5. Opportunità concorrenti e capacità delle mani

Il nuovo coordinatore **non genera falsi bersagli né ritarda automaticamente una minaccia reale** per proteggere una combo. Legge fatti correnti validi: rivale eleggibile, minaccia SC3 presentata, capsule/cavo attivi e disponibili, boost realmente utilizzabile, Party esplicitamente abilitato. Le azioni restano disponibili anche se non suggerite. Riutilizzare le prove ammesse dei core; nessun contatto semantico da classifica o primo rivale.

Bundle massimo: un suggerimento offensivo, una difesa, due capsule, un cavo, un boost di servizio, un Party = **7 bundle**. Ogni bundle descrive task atomici; scegliere uno solo dei rivali come suggerimento, senza cancellare gli altri target. Ordinamento rivali stabile: attività già iniziata valida, poi opportunità disponibile e distanza reale, poi id+instance.

Costo conservativo offensiva: `m(C)=1` con C=1, `2` con C=2, `3` con C≥3. C=1 significa cinque colpi sequenziali SC2, non una breccia inventata. C≥2 significa opener e gli altri contributori previsti, distinti. Capsule/cavo/difesa/boost hanno costo 1; Party costo `min(2,C)`. Anche se colpo e fascio usano pulsanti diversi, non assegnare due bersagli diversi alla stessa persona nello stesso istante.

| C | Offensiva completa + difesa simultanea | Offensiva + difesa + una raccolta |
|---|---|---|
| 1–3 | impossibile nel modello conservativo | impossibile |
| 4 | 3+1=4 | impossibile |
| 5 | 3+1=4 | 3+1+1=5 |

Quando C=3 arriva una minaccia durante una combo, suggerire il passaggio alla difesa; non continuare a presentare entrambi come obblighi fattibili. La cancellazione della combo avviene soltanto per input/regole reali SC2, mai perché il suggerimento è cambiato. Con C=1 una capsula guscio→raccolta è una sequenza, non un bundle da due persone.

Algoritmo deterministico e bounded:

1. Raccogliere fatti al tick autoritativo; invalidazioni, nuove minacce e scadenze aggiornano immediatamente i suggerimenti. Per aggiornamenti ordinari massimo 4 Hz di **tempo simulazione**; nessun nuovo filtro sugli aim.
2. Rispettare gli impegni volontari held realmente validi come occupazione: se l'utente non segue il suggerimento, non assegnargli un secondo compito. Suggerire di interrompere se necessario, senza impersonare un rilascio.
3. Enumerare i 2^7=128 sottoinsiemi; scartare quelli incompatibili o con somma slot>C_disponibile. Espandere task, enumerare assegnazioni iniettive delle persone eleggibili: al massimo `5!/(5−k)! ≤120`, quindi **≤15.360** valutazioni per aggiornamento completo. Non eseguire un solutore generale o assegnamenti esponenziali su target senza limite.
4. Valutare lessicograficamente: difesa realmente imminente coperta; recupero cavo urgente coperto; continuità degli impegni compatibili; numero persone con scelta utile; anzianità di attesa; affinità di ruolo; chiave stabile. Urgente iniziale: cavo con tempo residuo ≤1,2s. Tutti questi parametri vanno esposti nel profilo/report.
5. Per anzianità usare `W_i` in tick, incrementato solo durante gara attiva con persona pronta e almeno un compito eleggibile, senza suggerimento. Saturare a 30F; assegnazione azzera W_i. Confrontare somma `(W_i+1)^2` in int64. A F≤1000 e C≤5, massimo `5*(30000+1)^2=4.500.300.005`, oltre int32 ma entro int64.
6. Conservare un suggerimento ordinario per almeno 0,6s se ancora valido; priorità difesa/scadenza/loss interrompono subito. La stabilità è sul testo/indicatore, mai su un input già invalidato.

Il lemma round-robin del calcolo allegato vale solo con uguale eleggibilità persistente, uno slot e priorità attesa; non promette assenza assoluta di stand-by in una gara. Misurare separatamente tempo senza opportunità, con opportunità ma senza suggerimento, impegnato, non pronto/tracking perso, pausa. Non inventare punteggi per riempire il vuoto.

## 6. Contributi e crediti

Il valore deriva da fatti autoritativi, non trigger, tempo held, suggerimenti seguiti o last hit soltanto. Mantenere contatori per participantId e tipo, distinti dai punteggi legacy:

- apertura effettiva SC2 una volta per target+instance+window; contributo valido per persona alla combo; stagger solo se effetto applicato;
- contrasto difensivo solo per minaccia realmente bloccata, con owner riconosciuto dal core;
- guscio aperto, modulo raccolto, modulo recuperato solo a commit rispettivo; niente raccolte a inventario pieno;
- boost SC3 applicato; Party applicato come evento di squadra, con consenzienti senza colpevolizzazione individuale.

Usare i risultati/receipt dei core proprietari e piccoli hook di evento confermato; non rileggere due snapshot uguali come due eventi. Mantenerne la deduplicazione nel ciclo di vita del target/threat/module o con ultimo seriale autoritativo, non una mappa infinita. Somme uint64 saturanti diagnosticate; bonus ruolo = 0. Quando una persona passa al volante conserva i suoi crediti; un dispositivo nuovo non eredita la persona precedente.

## 7. Handoff completo: due stadi e possesso del puntatore

Il checkout attuale consente rebind/prepare solo dopo che `handoffActive=false` e il nuovo pilota è autoritativo. **Preservare i due stadi**, rendendoli espliciti:

A. preavviso → freeze di tutti i kart → passaggio fisico → conferma nativa/countdown esistente → commit del nuovo driver;

B. mondo ancora held dal gate Scintilla → proposta associazioni → rebind SC1 disarmato → verifica possesso sulla nuova autorità → prepare SC2–SC4 → bindings-confirm e prepare SC5 → nuovo rilascio richiesto → Ready/ripresa esplicita.

**Separare il gate di avanzamento del cambio dal gate gameplay.** Oggi `RelaySessionController::confirmHandoff` e `updateRace` ritornano se `m_scintilla_start_permitted=false`: invalidarlo appena inizia A creerebbe deadlock. A può avanzare solo nel ramo mondo-held con autorità fisica/presentazione ancora valide; al commit del driver, applicare atomicamente il blocco del nuovo turno **prima di qualsiasi step fisico**. B richiede autorità e verifica nuove. Non rendere `prepared(newTurn)` prerequisito del comando che deve creare newTurn.

Non aggiungere un secondo conto alla rovescia se quello nativo è già completato; UI deve distinguere «Cambio pilota» da «Associa i puntatori / Preparazione». Nessun percorso auto-confirm, telefono, operatore, skip, resume o API deve superare il gate finale. La conferma possesso non deve essere requisito per completare lo stadio A: richiederebbe il nuovo inputEpoch che esiste solo dopo A, creando un deadlock.

Trasferimento standard: se d_old esce dal volante e d_new entra, il puntatore di d_new viene proposto a d_old; ogni altra associazione resta invariata. Per roster P, la mappa è una biiezione fra P−1 supporter e lo stesso insieme S di sorgenti concesse: rimuovi `(d_new,s)`, inserisci `(d_old,s)`. Verificare cardinalità, unicità, assenza del pilota, esatta uguaglianza di S e revisioni fisiche. Non applicare la proposta senza mostrare persona, colore e sorgente. Nei cambi non standard ricalcolare e richiedere associazioni esplicite mantenendo gli stessi vincoli.

Verifica di possesso proposta, semplice e locale alla preparazione:

- Dopo il rebind riuscito, nuovo challenge UUID legato a generation/producer/race/driverTurnId/inputEpoch/roster/binding revision; l'operatore ha già identificato la persona. Il gesto prova il possesso della sorgente assegnata, **non l'identità biometrica**.
- L'unico pump SI5 osserva sourceState/azioni mentre il gameplay è disarmato. Nessun secondo drain, consumer o azione di gioco sintetica.
- Per ogni sorgente cambiata (e tutte all'avvio), indicare nome/colore e chiedere tutti i controlli rilasciati, quindi un breve primary press→release. Richiedere stato rilasciato noto per almeno 200ms di clock monotono valido e sorgente viva; silenzio o timeout non provano rilascio. Non riusare una pressione già in coda prima del challenge. La prova è vincolata alla sorgente e al challenge, consumata una volta; non fornisce punti o consensi Party.
- L'adattatore raccoglie la prova con sequenze/timestamp già verificati SI5; l'host autenticato la attesta nel completamento binding. Il native ricontrolla identità, set sorgenti, stato held e binding corrente; la preparazione finale richiede inoltre neutralità e nuove prove ottiche secondo SC1. L'host è qui autorità del roster fisico, non autorità del gameplay.
- Release→press→release e preparazione finale sono due barriere: non inoltrare quel press al gameplay. Non chiamare un altro rebind dopo il challenge: cambierebbe inputEpoch e lo invaliderebbe. Eventi tardivi del vecchio possessore sono scartati per epoch/revision, senza chiudere il grant sano.
- Un errore lascia il mondo held e il pulsante Ripeti disponibile. Timeout challenge proposto 30s wall-clock, rinnovabile; non avvia mai la gara automaticamente.

Persistono energia/calore, cooldown, quota Party del turno, carico, fari, ricevute e tempo residuo del cavo. Si chiudono lock/combo/ownership/consenso e suggerimenti. I timer gameplay restano congelati; lease, timeout e challenge continuano sul clock monotono. Conservare la policy fail-closed SC3 per minacce annullate alla sospensione: non resuscitare proiettili vecchi e non convertirli in difese riuscite. Il design generale parla di sospensione dei timer, mentre SC3 implementa anche annullamento delle minacce: documentare questa distinzione nei test, senza cambiare incidentalmente il modello balistico SC3.

## 8. Party: consenso, identità, parametri

Nodo dedicato, secondary/tool hold deliberato. Primary su quel nodo non spara nel mondo e non vale consenso. Tutti possono usarlo, indipendentemente dalla competenza. Party off: nessun comando efficace nemmeno tramite payload diretto; il nodo non invita all'azione.

| Parametro iniziale | Valore | A 120 Hz |
|---|---|---|
| Hold continuo di ciascun consenziente | almeno 0,75s | 90 tick |
| Finestra consenso dalla prima pressione ammessa | meno di 3s | deadline a +360 tick |
| Consenzienti distinti | 1 se C=1, altrimenti 2 | mai due sorgenti della stessa persona |
| Quota | 1 applicazione per turno pilota | condivisa dalla squadra |
| Costo | 15 energia SC3 | 1.800.000 unità Q |
| Impulso tangenziale | +1m/s massimo prima dei clamp nativi | una sola applicazione |
| Aumento limite massimo | +1m/s, 0,5s; fade 0,25s | 60 + 30 tick |
| Forza motore aggiunta | 0 | nessuna spinta sostenuta Party |
| Cooldown servizio condiviso dopo successo | almeno 6s | 720 tick |

Minimi: `ceil(D_us*F/1e6)`; massimi: `floor(D_us*F/1e6)`; F fisico verificato 60…1000, distinto dai FPS. Semantica temporale: proposta valida su `[start,deadline)`, expiry prima dei commit; alla deadline esatta rifiutare. Hold maturo quando `now−pressTick≥ceil(750000F/1e6)`. Tutti i consenzienti devono ancora essere held/visibili e nella stessa autorità al commit; nessun voto lasciato dopo perdita fascio. Le negative sticky SC1 restano tali anche se arriva subito un nuovo hit.

Stati: Disabled, Unavailable(reason), Idle, Collecting(proposalId/deadline/consents), CommitPending, Used. In pratica possono essere un enum più flag derivati; evitare duplicazioni. Primo press valido apre la proposta. Per C=1 la stessa persona può completare da sola. Per C≥2 vale il primo quorum di due persone distinte ordinato per tick ammesso e participantId; altri non generano un secondo effetto. Tutti i voti appartengono alla stessa proposta. Finestra scaduta, cambio contesto, perdita di un voto necessario, pausa/rebind/nuovo turno o freno invalidano la proposta e richiedono release/press; non tenere un hold latente per avvio automatico dopo il veto.

`prepare` nella stessa gara/turno conserva Used e crediti; nuovo inputEpoch non ricarica. Cambio driver realmente completato apre la nuova quota; non caricare il contatore per la semplice richiesta. Reset gara azzera la quota tramite nuova raceEpoch. Limite teorico delle attivazioni nella gara regolare: L = 6,6,8,10,12 rispettivamente, **non L*C**.

## 9. Energia, arbitraggio e applicazione fisica

Una sola risorsa SC3: Q=1000F. Il design dice «riserva di servizio»: in questo profilo significa energia comune **spendibile sopra la riserva difensiva protetta**, non i moduli e non un secondo conto indipendente.

`R_Q = 5Q + 20000*(warningTicks + marginTicks)`; `costParty_Q=15Q`.

Ammissione: `E_Q ≥ R_Q+15Q`. A F=120: R=70 con C=1, R=60 con C≥2; Party richiede rispettivamente **85/75**. Da pieno lascia E=85; ricaricare 15 senza altri consumi richiede 3s, ma il cooldown comune è 6s. A F non divisibile, usare i tick arrotondati effettivi; ad esempio non hardcodare 70/60. [calcoli.json](calcoli.json) include 60,61,120,144,1000 Hz e verifica l'intero intervallo ammesso.

Proprietario energia resta `ScintillaDefense::Runtime`. Aggiungere una transazione interna tipizzata per servizio `NormalBoost/PartyBoost`, non due setter dell'energia. Il core Party propone un effetto; il commit unico ricontrolla tutto e conferma costo/quota solo su successo. Nessun POST «spendi energia» o «applica turbo» per i partecipanti.

Ordine nello step, conservando SC4: expiry/held e arrivo naturale prima dei nuovi commit; risorse/impatti/difesa; raccolte e attraversamenti nell'ordine attuale; infine servizio opzionale. Una minaccia o un impatto dello stesso tick vieta il turbo. Se normal boost e Party maturano nello stesso tick, **normal boost ha priorità**; Party viene annullato senza costo e richiede rilascio. Nel checkout `updateDefense` applica già il boost SC3: separare richiesta e commit, rinviando solo quel commit al punto unico di arbitraggio; non applicarlo una volta lì e una seconda nel coordinatore. Non spostare la risoluzione degli impatti SC3 dopo la raccolta. Entrambi aggiornano lo stesso limite di servizio a `now+ceil(6F)` su applicazione riuscita. Nessuna quota/costo se nativo rifiuta.

Gate Party al commit: SC5 preparata/current, Party abilitato, quota libera, consenso completo, energia sufficiente, nessuna minaccia (solo Idle/Cooldown difesa), nessun rinforzo attivo, nessun boost nativo attivo, kart reale a terra/in avanti con velocità positiva, nessuna animazione/recovery/invulnerabilità, nessuna pausa/handoff/intro/finale, freno non premuto, zona ammessa e prove fresche. Rileggere freno/stato fisico al commit, non solo all'inizio del hold.

### 9.1 Primitiva del turbo e controllo del pilota

Riutilizzare il **limite di velocità nativo** `increaseMaxSpeed` con categoria ZIPPER e parametri Party separati. Non chiamare ciecamente `applyBoost`/`instantSpeedIncrease`: questa API calcola il floor da `kart.getSpeed()`, poi `btKart::adjustSpeed` applica quel valore alla componente tangenziale. Controesempio: |v_t|=4, |v_n|=3, velocità totale5; floor6 produce |v_t'|=6, ossia **+2**, non +1. Può inoltre prevalere il minimo di una superficie zipper. Il percorso SC3 resta invariato e conserva la propria correzione dopo Bullet.

Per Party aggiungere un piccolo helper game-thread dopo Bullet, con questa matematica e sequenza (nessuna nuova categoria MaxSpeed):

1. Validare kart, body, velocità/normal finite, normal unitaria entro tolleranza numerica, v_t non nullo, tutti i gate. Rifiutare superficie `Material::isZipper()`, qualsiasi minSpeed veicolo positivo e qualsiasi speedIncrease attivo, inclusi fade. Nessuna sovrascrittura di floor altrui.
2. Siano B=`getKartProperties()->getEngineMaxSpeed()`>0 e M0=`getCurrentMaxSpeed()` allo stesso tick. Senza speedIncrease attivi, il codice attuale dà `M0=B*f`, con f fattore di slowdown minimo, 0≤f≤1. Verificare finitezza/intervallo; il limite nativo previsto con add_max_speed=1 è **M1=(B+1)*f**. Rileggere la formula di `MaxSpeed::update` al passo0: se è cambiata non duplicare una previsione obsoleta, esporre un calcolo read-only dal proprietario nativo.
3. `n=normal`, `v_n=n(n·v)`, `v_t=v−v_n`, `t=|v_t|`, `z²=|v_n|²`; `t_cap=sqrt(max(0,M1²−z²))`; **`t_new=min(t+1,t_cap)`**. Se `t_new≤t+ε` rifiutare prima di modificare qualsiasi stato: ε numerico `32*epsilon(btScalar)*max(1,|v|,M1,B)`, massimo0,001m/s, non soglia di aim. Con B=28 e float32 vale circa0,000107m/s. Per il normale controllare `abs(n·n−1)≤32*epsilon(btScalar)`; valori oltre limite sono rifiutati. Dopo conversione a btScalar ricontrollare finitezza, variazione tangenziale e cap entro ε prima di scrivere; il bound ≤1 vale entro questa tolleranza numerica esplicita. Se servirebbe ε maggiore, rifiutare numeri fuori scala. Questo preserva componente normale e rispetta il cap totale, con 0<Δt≤1.
4. Preparare e verificare vettore `v_new=v_n+v_t*(t_new/t)`. Tutto il preflight precede le mutazioni. Installare `increaseMaxSpeed(ZIPPER,1,0,durationTicks,fadeTicks)` e scrivere una sola volta la velocità lineare `v_new`, senza toccare velocità angolare, minSpeed, controlli o clock. La classe nativa aggiorna il cap al successivo normale update prima di Bullet; non chiamare `MaxSpeed::update(0)` come una funzione pura, perché può modificare il veicolo. Il comando vive sul game-thread, senza interleaving/await fra controllo e commit.
5. Confermare addebito/quota solo dopo installazione e scrittura riuscite; validare prima argomenti/categoria/tick nei limiti nativi int16 (qui ≤500 e250 a F1000). Nessun errore operativo atteso dopo il preflight. Se il wrapper può fallire, offrire commit atomico dal proprietario anziché lasciare solo metà effetto o rimborsare soltanto l'energia.

Il normale boost e Party condividono helper numerici innocui dove utile, non la semantica di floor. Qui non viene creato alcun minimo Party persistente. Il cap temporaneo ha engineForce=0 e non è una spinta continuativa; freno prima del commit=veto, dopo il commit=normale controllo del pilota senza ulteriori impulsi.

Non azzerare tutta la categoria ZIPPER quando il pilota frena: un pad vanilla potrebbe essere subentrato. Non tentare di «restituire» la velocità iniziale cancellando un effetto. La proprietà Party riguarda il commit e la quota, non attribuisce a Party qualsiasi zipper successivo. Richiedere prova nativa brake-before e brake-during, attraverso `PlayerController::action`, comprese sequenze analogiche, ordine PA_BRAKE/PA_ACCEL e pad successivo. Nel checkout PA_BRAKE azzera Accel, ma un PA_ACCEL successivo può togliere Brake; il kart privilegia Accel se entrambi sono true. Non inventare una priorità brake universale assente dal controller. Se il controllo fisico reale non dimostra il contrasto previsto, la fase fisica resta aperta: correggere l'adattatore locale con ownership esplicita prima di dichiararla chiusa; non introdurre stop globali o sterzo assistito.

Per normale unitaria n e velocità v: `v_n=n(n·v)`, `v_t=v−v_n`; l'operazione deve conservare `v_n` e velocità angolare. L'incremento tangenziale effettivo del nuovo helper è ≤1 per costruzione; può essere inferiore per il cap. Il normale e l'angolare restano invariati; i passi fisici successivi restano governati da STK. In modello rettilineo senza vincoli, energia cinetica specifica aggiunta `ΔK/m=v*Δv+Δv²/2`: a 10/15/20/25m/s con Δv=1 vale 10,5/15,5/20,5/25,5 J/kg. Nessun valore di massa reale è presunto.

L'integrale ideale del solo aumento del **tetto** è `1*(0,5+0,25/2)=0,625m`. Non è un limite universale della distanza guadagnata: accelerazione, terreno, freni e velocità sotto il tetto rendono invalida quell'interpretazione. Non usarlo per certificare una zona.

## 10. Zone Party: calcoli e ammissione

Dati separati dalla mesh/grafo, vincolati a track hash, direzione e versione regole. Inizialmente il manifest è **authoring/non qualificato**: non promuovere i punti allegati a zone operative. La selezione Party nella UI richiede dati ammessi; il profilo standard resta utilizzabile una volta qualificato SC5. L'assenza di dati deve produrre una ragione visibile e istruzioni operatore, non un «turbo disponibile» che rifiuta sempre.

Il calcolo allegato legge i 83 quad reali e ricostruisce il ciclo semplice, circa 992,571m. Per tre centri a,b,c:

`κ = 2 |(b−a) × (c−b)| / (|b−a| |c−b| |c−a|)`.

È la curvatura del cerchio interpolante 3D (4 area/prodotto lati). `a_lat≈v²κ`; il riferimento `v=sqrt(3/κ)` usa **3m/s² ipotetici**, non aderenza misurata. Raggio infinito per terna collineare. Punti candidati nei sei intervalli di pista: nodi **6,17,26,45,59,71**, con larghezza minima6m; il nodo6 differisce dal candidato capsula7 perché la metrica è diversa. Non allargare automaticamente a interi segmenti.

La curvatura campionata non vincola le curve intermedie della mesh, gli ostacoli, le rampe, il grip o l'ingombro del kart. Per ciascuna zona operativa servono estremi s_start/s_end dal grafo nativo, percorso continuo in avanti, validazione geometrica/collisioni e prova nel gioco con freno e velocità del profilo. Vietare rampe/salti, recupero, handoff pending, tratto finale e zone non descritte; non modificare la pista per renderle valide.

Modello di frenata illustrativo: `d(v)=v*τ+v²/(2a)`, quindi `Δd=Δv*τ+(2vΔv+Δv²)/(2a)`. Con a=6m/s² e τ=0,25s **assunti**, a v=20m/s il +1 implica +3,6667m di arresto (38,3333→42m), molto più di0,625m. Allegato con10/15/20/25m/s. Non ricavare a reale dalla formula: misurare native brake/slope; nessuna prova di comfort umano è deducibile.

Runtime: controllare posizione attuale e distanza residua nella zona anche all'ultimo consenso; l'utente può impiegare quasi3s. Guardare un orizzonte operativo da profilo `T=burst+fade+margin`, velocità massima ammessa e incertezza spaziale misurata: `d_guard=v_bound*T+position_error+kart_forward_extent`. Se i limiti non sono disponibili o si esce dalla zona durante consenso, annullare. È un filtro conservativo con ipotesi dichiarate, non previsione esatta della dinamica. Rescue/discontinuità vietano credito e ammissione fino a nuova localizzazione continua.

## 11. HUD, mira e qualifica SC5

Il dome rimane superficie primaria: nome/colore e competenza breve nei badge; una proposta concreta con target reale; stato cambio/assegnazioni; Party distinguibile dal boost normale e costo15 prima dell'uso. Non coprire il reticolo con una finestra informativa. Mostrare «1/2 pronti», scadenza, «rilascia e ripremi», usato nel turno, riserva protetta, zona non ammessa e veto pilota. Tutti i testi Mac EN/IT nel catalogo; render STK usa pipeline/localizzazione esistente e icone leggibili, nessun testo developer in sala.

Coordinate normalizzate esistenti: `x=2(uW−W/2)/min(W,H)`, `y=2(vH−H/2)/min(W,H)`. Nodo Party proposto **(x,y)=(0,22;0,85), r=0,06**, boost servizio rimane(0;0,85). Separazione bordi=0,10 in queste coordinate. Distanza dal centro cupola+r≈0,9380<1. A1536²: u=0,61, v=0,925, r=46,08pixel. Il cerchio è disgiunto dai nodi difesa e tutti gli elementi HUD v2 (altri elementi hanno y≤0,69 per i cerchi difesa, ≤0,52 per le altre aree). Pittura, stroke, halo e numeri interni non devono eccedere la maschera; se il testo non sta, usare un'area riservata esplicita e ricalcolare la qualifica.

Aggiungere `Context::PartyBoost` in coda senza rinumerare i valori; primary/tool classificati dal frame storico con layout v3 **e** validità corrente. Il contesto Party non dipende da threatId SC3; usa race/turn/proposal corretti, come WorldOffense non deve interrompersi per un threat nuovo. Nessun fallback world se il cerchio è disabilitato. Conservare min/max frame history, controlli depth/clipping e negative sticky SC1.

Layout **v3**: maschera Party riservata in entrambi i profili standard/Party per stabilità; pittura inattiva o assente quando off. Badge ruolo/suggerimento restano nei rettangoli già riservati, senza allargamenti impliciti. Aggiornare insieme CPU classify/worldRectFree, shader, frame snapshot, preparazione host e tutti i validatori del layout. Testare almeno256²,768²,1536²,2048×1024,8192²; viewport minima non prova leggibilità reale.

Qualifica: il fingerprint deve distinguere layout v3 e revisione SC5; native recorder e parser C++/Python devono concordare su schema chiuso. Preferire nuova revisione trace/report esplicita per SC5 mantenendo il lettore v1/v2 per SC4: non far accettare v3 cambiando solo una costante. Parametri Party e zone sono parte dell'identità del profilo operativo. Lo schema dei sei speedIncreaseTicks resta invariato con riuso ZIPPER; la geometria deve essere stressata a velocità native almeno pari all'inviluppo richiesto, non basta rinominare uno scenario «Party».

Nessuna qualifica derivata dai calcoli allegati. Conservare il contratto temporale corretto SC4: disponibilità `[readbackUs, captureUs+50000]`, sovrapposizione prove e prefetch pronto all'attivazione; cavo con il proprio contratto. Reimpiegare manutenzione begin/resume e retry esplicito, ACK di nuovo rendering per ciclo e recupero senza producer restart. V3 richiede nuove prove reali delle maschere; vecchi report devono risultare incompatibili chiaramente.

## 12. API, preparazione e concorrenza host

Proposte additive sotto `/immersive/v1/scintilla/` (namespace host `.scintilla`), usando il trasporto autenticato esistente, coerenti con `mission/intent`, `prepare`, stato e ricevute: `crew/intent`, `crew/prepare`, `crew/state`, `crew/bindings-confirm`. Lo schema proposto è fissato in [CONTRATTO_SC5.md](CONTRATTO_SC5.md); verificarne il raccordo al codice al passo1 prima di consumarlo. Questi endpoint configurano/coordinano, **non attivano il turbo**. Il consenso arriva soltanto dagli input spaziali reali ammessi.

Intent imposta atomicamente tutti i prerequisiti SC2–SC5; cambiare profilo mentre una preparazione è in volo non deve lasciare una finestra Ready. Conflitti/retry seguono receipt bounded64 e requestUUID con fingerprint, come SC4; stesso id+body restituisce risultato precedente, diverso body stessoid è conflitto. Payload ≤16KiB, stringhe/liste bounded, campi ignoti rifiutati, UInt64 canonici. Stato include autorità, profilo, turno, ruoli, bindingsVerified, gate, opportunità e Party; assenza di capability non autorizza downgrade silenzioso.

Ordine di ingresso SC5: chiudere il gate gameplay host → latch atomico `crew/intent` nativo (ammesso anche senza qualifica, lascia mondo held) → preflight/maintenance esplicito sul profilo/layout v3 ora selezionato → intro della variante → nuovo render ACK/prove → acquisizione/preparazione iniziale SC1 se non esiste grant, oppure rebind SC1 held quando necessario se il grant esiste → challenge sulla nuova autorità → SC2/3/4 prepare → bindings-confirm/SC5 prepare. Scindere `prepareScintillaSpatialInput` (oggi monolitico) alla barriera SC1: il challenge non può attendere un consumer non ancora creato e non si possono completare SC2–4 prima della nuova verifica richiesta. Preservare il cleanup differente fra acquisizione iniziale fallita e rebind recuperabile sul grant sano. Adattare la sincronizzazione SC4 per non eseguire in parallelo un secondo intent/preflight. Il fingerprint v3 deve essere derivabile dal profilo richiesto prima di `prepared=true`: altrimenti qualifica e preparazione si attenderebbero a vicenda. Le sessioni solo SC4 conservano il loro ordine attuale.

Estendere i gate di tutti gli ingressi: operatore/telefono/HTTP, Ready iniziale, countdown, resume, cambio, reset, results→Pronto. Aggiungere `crewSC5` o equivalenti espliciti nell'enum host, con prerequisiti SC4. Stato dichiarato «preparato» solo se native stesso profilo/turno/roster/input e binding verification corrente. Tutte le operazioni async catturano generation, producer, race, turn, intent, input e roster; ricontrollano dopo ogni await prima di pubblicare. Cancel di Task non dimostra rollback lato server: usare stato e retry idempotente.

Il pump SI5 resta unico. Il challenge di possesso osserva messaggi già drenati e verificati, non toglie lifecycle al lease manager. Durante maintenance il buffer conserva la politica corretta SC4; un completamento di challenge antecedente a maintenance/rebind non riapre il gate. Route nuova invalida prove/preparazione interessate, conserva crediti, quota, risorse e missione; non ricalibrare arbitrariamente.

Estendere il package TholusScintillaMission con DTO/reducer crew; riusare UInt64 e identità. Nessuna migrazione dell'app a Swift6 o @MainActor indiscriminato. UI/modelli mantengono ObservableObject; publisher ad alta frequenza non diventano @Published per ogni aim. I test package e quelli app devono verificare le operazioni reali tramite transport/clock controllati, non soltanto confrontare stringhe sorgente.

## 13. Report e risultati

Il criterio di vittoria SC4 rimane immutato. Aggiungere envelope congelato SC5 collegato a producer/race/result revision: profileId, crewRulesRevision, Party abilitato fin dall'inizio, turni realmente completati, numero turbo applicati, contributi per persona, anomalie rotazione, tempi opportunità. `partyEnabled` e `partyActivations>0` sono concetti distinti: il bucket resta Party anche se non usato.

Bucket proposto `scintilla_sc5_standard_v1` oppure `scintilla_sc5_party_v1`, oltre alle dimensioni già necessarie di pista/difficoltà/P/regole. Non usare il solo `arcade_fixed` attuale. Niente comparazione fra partite Party e standard nello stesso gruppo; non cambiare le classifiche legacy. Fine artificiale=test/not_completed non classificabile; deviazione amministrativa di rotazione segnalata e non confrontabile.

Congelare insieme al risultato naturale SC4, prima di commit tardivi e report: retry, GET, nuova intro, richiamate GPU e reset non modificano il vecchio record. Aggiornare snapshot nativo, JSON HTTP, report persistito, app DTO, package, UI e fixture. Tempi gioco in tick/F escludono freeze; durata commerciale wall-clock separata. Metriche individuali distinguono attesa, assenza opportunità e tracking perso; nessuna accusa pubblica al partecipante che ha proposto Party.

## 14. Invarianti da conservare e revisione obbligatoria

- Un consumer/pump SI5; drain colpi esattamente una volta, dispatch per contesto/tipo. Eventi Party non diventano colpi SC2/SC4 e viceversa.
- Persona distinta != controller distinto. Identità wide mai arrotondata; quorum/credito non dipendono dal colore o indice del frame.
- InputAuthority valida a evento, ammissione e commit; vecchi frame/phase/instance/turn non producono effetti.
- Negative sticky e release-required non annullati da hit successivo nello stesso batch; nessun secondo throttling aim.
- New native AI threat non invalida WorldOffense/Party per semplice confronto threatId: Party annullata soltanto per la sua regola esplicita di indisponibilità con minaccia, con feedback corretto.
- Fine naturale prima dei nuovi effetti, impatto confermato prima di raccolte, no colpo GPU tardivo dopo finish/handoff.
- Non chiudere il grant sano per rebind/reset/preflight; mantenere la manutenzione recuperabile e il nuovo ciclo render ACK SC4.
- Quota Party/cooldown persistono nello stesso turno; costo e fisica una sola transazione game-thread senza await fra controlli e commit.
- Nessun punteggio per pressione, device reconnect, inventario pieno, contrasto inesistente o animazione rifiutata.
- Target/coordinate suggeriti devono esistere nelle prove correnti. Una curva di tre quad non certifica corridoi o comfort.
- No duplicazione di scheduling media host: SC5 gameplay appartiene a STK; eventuali modifiche narrative usano la pipeline intro esistente. Nel briefing aggiungere un pannello/variante Party prerenderizzato e sottotitoli che dicano turbo crew e freno; aggiornare manifest/durata/hash/frame reali se il montaggio cambia. Standard non viene presentato come Party.

## 15. Sequenza esecutiva per l'LLM implementatore

Ogni passo: leggere chiamanti + consumatori, implementare il perimetro, aggiungere le regressioni indicate, rileggere i casi avversi, eseguire i controlli consentiti, registrare esito/limiti. Non passare oltre una discrepanza nota che invalida il passo seguente.

| Passo | Lavoro concreto | Controllo di uscita | Complessità |
|---|---|---|---|
| 0 | Verificare manifest/WIP, vincoli, toolchain e baseline SC4; fissare stato autorizzazione build | Nessuna baseline da HEAD erroneamente assunta completa; source drift spiegato | media |
| 1 | Schema profilo/turno/receipt/bucket + contratto endpoint e DTO di base | Identità, compatibilità legacy, Party defaultoff, gate start definito | alta |
| 2 | Core crew puro: turni e formula competenze, reset/reprepare | Tutte P2…6, giro finale, cambio reale vs retry, invarianti matematica | media |
| 3 | Hook fatti/crediti, scheduler bounded delle opportunità | Concorrenza C1…5, nessun target inventato/duplicato, nessuna azione esclusiva per ruolo | alta |
| 4 | Ciclo handoff a due stadi, proposta biiettiva, challenge possesso e UI | Operatore e telefono fino a ripresa; no vecchi press, no grant close, recovery da errori | molto alta |
| 5 | Core Party: quorum, scadenze, costo/riserva, quota e arbitraggio servizio | Retry stesso turno, dup persona, sticky loss, veto, ordine stesso tick | alta |
| 6 | Adattatore fisico parametrico boost e gate zone | Impulso tangenziale bounded dopo Bullet, cap nativo, freno contrastante, niente stacked boost | alta |
| 7 | Layout v3, nodo Party, badge/suggerimenti, feedback, intro variante | CPU/shader/prove storiche coerenti, costo visibile, qualifiche vecchie respinte | alta |
| 8 | Wire completo/host intent-preflight-prepare-Ready e recuperi | Tutti gli ingressi e await proteggono autorità; nessun downgrade | molto alta |
| 9 | Report congelato e bucket, dati zone in authoring e strumenti qualifica | Risultato naturale coerente; Party off/usato distinti; nessun dato sintetico qualificato | alta |
| 10 | Registrare/eseguire regressioni software secondo autorizzazione e rileggere diff | Matrice test coperta, sourcecheck distinti da test nativi; regressioni vanilla/SC0–4 | alta |
| 11 | Qualifica nativa geometria/zone e ciclo completo, solo se autorizzato | Evidenza reale P/C, freno, handoff, finish, reset e Party; fallback non maschera fallimenti | molto alta |

I passi5–9 si possono sviluppare con interfacce/mock di test stabiliti al passo1, ma non dichiarare il passo4/6/8 verificato end-to-end fino all'integrazione e prove reali. Se no-build, indicare «codice ricontrollato, verifica nativa pendente»; non confondere una colonna di matrice con un test passato. SC6/playtest umano resta fuori.

## 16. Verifica documentale e comandi successivi

Calcoli senza compilazione, sola lettura per default:

```sh
python3 -B docs/immersive/scintilla/sc5-plan-2026-10-16/verifica_calcoli.py
```

`--write` serve soltanto per una modifica deliberata e revisionata della specifica matematica; non aggiornare le attese per nascondere un errore dell'implementazione. Il confronto attuale contiene **57.925 asserzioni aritmetiche/strutturali**, incluse F60…1000, rotazioni, quorum illustrativo, geometria HUD e candidati della pista; non sono test di SC5 implementata.

Per future build autorizzate: usare i target CTest esistenti in `tools/immersive/tests` e registrarvi crew/Party; per l'app usare `scripts/dev/build.py`, per package `scripts/dev/test-package.py` e runner app isolato già adottato. Leggere `--help`/istruzioni del checkout per opzioni reali; output/cache sul SSD, niente nuovi runner alternativi. Test core senza rendering non dimostra il freno, output GPU o la sincronizzazione dell'app. Eseguire guard localizzazione quando si cambia testo Mac. Non eseguire comandi build copiati da un documento quando la richiesta li vieta.

Accettazione finale separata: **piano**, **codice**, **regressioni software**, **ciclo nativo**, **qualifica fisica/utenti**. Il rapporto deve dire quali livelli sono conclusi e quali pendenti, con configurazione e prove. Nessun placeholder in UI/asset richiesti; nessuna dichiarazione di Party operativa senza zona/profilo ammessi.
