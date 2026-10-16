# Missione Scintilla — SC0: baseline e profilo

Data: **13 ottobre 2026**, Europe/Rome. Fase autorizzata: **SC0**.

**SC0 completata come ricognizione e definizione del profilo.** Il checkout offre la gara Relay, il cambio pilota, la cattura a sei facce, domemaster ed equirettangolare nativo. Non offre ancora il percorso Spatial Input → targeting storico STK né un profilo Scintilla eseguibile. Le capacità mancanti sono elencate con proprietario e prova richiesta. **SC1 non avviata.**

Consegne: questo rapporto, [profilo documentale](profile.json), [manifesto delle fonti](evidence/source-manifest.json), [ambiente e asset](evidence/environment.json), [verifiche](evidence/verification.json) e [log della suite di proiezione](evidence/projection-source-check.log). Il JSON del profilo non è una configurazione accettata dal gioco: nessun loader, endpoint o flag Scintilla è stato introdotto.

## 1. Baseline effettivamente esaminata

| Materiale | Baseline e portata |
|---|---|
| STK | `main`, HEAD `0827c13dbec8e927ccee08ba52c97c7fa6b74a67`, più WIP locale registrato nel manifesto; SuperTuxKart 1.5 |
| Design | [Missione Scintilla v2](../../../Tholus_STK_Missione_Scintilla_Design_v2.md), conservato integralmente |
| Spatial Input | Piano v2 del 13 ottobre nei Downloads; contratto e codice del checkout SocialTableController a HEAD `a373fb164369ce64f29fed3815345c79b5dfd81d`, con SI1/SI2 non tracciati e altro WIP |
| Mac di questa verifica | Apple M1 Pro arm64, macOS 26.5.2, Xcode 26.6, Apple clang 21, CMake 3.31.1; non il Mac mini M1 base del target venue |
| Asset | `../stk-assets`: presenti `snowtuxpeak`, `lighthouse`, `olivermath`; lettura di XML e hash, nessun caricamento grafico |

Il commit `96d38a4654a3a7e41556b26c564197bff3595932` citato dal design non è presente nell'object database locale. Non è stata dedotta una relazione di antenato/discendente né verificato il remoto: la baseline di questa fase è il checkout richiesto, identificato da HEAD e hash. Il codice effettivo prevale sulle capacità descritte come attuali nel vecchio studio; le regole di gioco del design restano il requisito.

Il WIP STK preesistente comprende tre documenti/istruzioni e tre sorgenti Relay; questi ultimi aggiungono `updateWorldClock()` dopo `StandardRace::updateTime()`. È stato letto e preservato, non attribuito a SC0. La preparazione documentale è avvenuta in un worktree isolato dello stesso HEAD; l'audit del comportamento ha letto il checkout principale con il suo WIP. Il manifesto registra entrambi questi fatti, senza presentare un worktree pulito come copia del codice locale modificato.

### Differenze rispetto al design da portare nelle fasi successive

1. **Equirettangolare presente nel sorgente.** L'estensione del contratto TholusOS del 7 ottobre e l'adapter/GLSL attuale sostituiscono la descrizione «futuro/stub» della Phase 4.6 e delle sezioni 1.2/9/15 del design per questa capacità. `status:production` significa adapter selezionabile; `venue_validation:pending` resta il limite documentato. [S02, S03]
2. **Spatial Input SI1/SI2 già presente come WIP condiviso.** Non crearne un secondo. Il rapporto SI2 registra test isolati riusciti, ma build app completa fallita e nessuna acquisizione fisica. Questi sono risultati precedenti letti, non test rieseguiti da SC0. SDK/trasporto consumer, Accessori e calibrazione persistente sono ancora fasi successive nel materiale esaminato. [H01–H04]
3. **`tool` non è ancora acquisito dal bridge.** Il wire ammette `primary`, `secondary`, `grip`; il bridge e il manifest VIVE attuali producono solo `primary`. Non esiste un controllo wire chiamato `tool`. [H02, H05]
4. **Video pronto non significa frame adatto al tiro.** Il gate host controlla proprietà, geometria, dimensioni, generation e freschezza del video fino a 500 ms; non restituisce l'identità del frame STK visibile all'istante dell'azione. Quel limite non è utilizzabile come budget del puntamento. [S04, H06]
5. **Gli esiti Relay correnti sono basati anche sul punteggio crew.** `buildReportLocked()` produce rank perfect/gold/silver/bronze; non implementa «posizione ufficiale 1 + tre moduli al traguardo». La nuova condizione resta consegna SC4. [S05]

## 2. Mappa dei componenti e confini di riuso

| Area | Sorgente esaminato / punto utile | Conseguenza per Scintilla |
|---|---|---|
| Build e runtime | `CMakeLists.txt`, `sources.cmake`, `relay_main_hooks.cpp`, launcher | C++ con opzione UNIX `-std=gnu++0x`; CMake; renderer STK, OpenGL/Syphon su Mac. Nessuna dipendenza Unity/Swift nel gioco. Non presupporre C++17 per il futuro client |
| Feature flag | `ENABLE_IMMERSIVE_RELAY`, `ENABLE_RELAY_LOCAL_API`, `ENABLE_SYPHON_OUTPUT`, `ENABLE_RELAY_TEST_TOOLS` | I primi tre sono attivati dallo script MVP; test tools OFF. `ENABLE_SPOUT_OUTPUT` non dimostra un percorso Windows qualificato |
| Mondo di gara | `RelayWorldController::load()` | Una sola entità kart umano, `ai_karts + 1` kart totali; normale gara STK, pista non invertita. Partecipanti Relay diversi dal numero di kart locali |
| Lifecycle | `RelaySessionController`, `ArcadeSessionStateMachine`, `RelayPilotRotation`, `RelayPlayerController` | Configurazione, start/countdown, pause, abort, reset, handoff e input pilota riutilizzabili |
| Freeze | `RelayRaceMode::captureHandoffFreezeTransforms()`, `enforceHandoffFreeze()`, `update()`, `updateTime()` | Congela tutti i kart e il tempo gara durante world-held; render vivo. Le nuove attività mantenute vanno cancellate, i nuovi timer di simulazione sospesi |
| Crew attuale | `CrewRoleManager`, `CrewEventDirector`, `CrewActionHandler` | Azioni semantiche con ruoli/cooldown/finestre. Non sono il consumer di raggi né il kit universale Scintilla |
| Trasporto locale | `LocalControlServer`, `RelayCommandRouter`, `RelayGameDispatcher` | API locale autenticata e POST drenati fra frame. Il dispatcher usa una deque senza limite esplicito in `dispatch()`: non usarlo come canale per pose ad alta frequenza senza policy bounded |
| Geometria video | `ImmersiveCameraRig`, `FullSphereCapture`, `CubemapRenderTarget`, adapter di proiezione | Pose della camera e sei render target esistono, ma manca lo storico per il picking correlato all'output |
| Pubblicazione | `ImmersiveRenderPipeline`, `ImmersiveOutputFrame`, `SyphonOutputAdapterMac` | Texture e metadati di formato; manca un frame ID/tempo di simulazione/camera/race generation nel DTO pubblicato |
| Effetti | `ExplosionAnimation`, `Attachment`, `Powerup`, `PowerupManager` | Riutilizzare con policy circoscritta; nessuna patch agli effetti in SC0 |
| Host TholusOS | `ImmersiveSuperTuxKartRuntimeController`, `ImmersiveSTKSurfaceRoutingBridge`, `RoomSpatialInputService` | Host di processo, video e servizio sala già separati; collegamento Spatial Input a STK da realizzare |

Riferimenti navigabili: [S01–S09] e [H01–H06] in fondo. Le ricerche nei sorgenti Relay e nei moduli STK host non hanno trovato un consumer `tholus.spatial-input/1`, un retrovisore di missione o un resolver storico di bersagli Scintilla. Questa assenza riguarda i percorsi ispezionati; non è una dichiarazione sull'intera suite.

## 3. Capacità di output e puntamento

| Capacità | Evidenza SC0 | Limite preciso |
|---|---|---|
| Cattura sferica | Sei facce della scena, FOV delle facce 120° con campionamento delle sovrapposizioni | Non equivale alla copertura fisica del telo né a sei viste giocatore |
| Domemaster | Implementato; preset `fulldome_safe`, 1536×1536, facce 768, pitch 35° | Contenuto front-facing 180°; nessuna mira automatica su un nemico virtualmente dietro |
| Equirettangolare | Adapter GPU nativo 2:1; safe 2048×1024/facce 768/pitch 0°; target 4096×2048/1024; 8192×4096 ammesso manualmente | Suite source/math passata oggi. Shader, pixel Syphon, prestazioni e orientamento fisico non verificati oggi |
| Camera | Segue il kart umano nel flusso a un kart locale; pose smussata e stabilizzazione, yaw/pitch/roll configurabili | `getSmoothedTrans()` influenza forward e posizione: il comportamento durante un sobbalzo umano va provato; non dedurre comfort dal solo flag horizon |
| Readiness | Gate producer più preparazione e ownership host, dimensioni/freschezza, revisioni geometria/presentazione | Consente l'avvio del video; non certifica un colpo su un oggetto mobile |
| Retrovisore Scintilla | Assente nei percorsi esaminati | Vista posteriore ricampionata, layout e mapping sono SC3; nessuna proprietà rear è oggi annunciabile come pronta per Scintilla |
| `rear_alert` | Requisito futuro SC3 | Alert e sorgente dell'attacco devono essere reali; un messaggio laterale non prova una vista targetable |
| `rear_targetable_view` | Non qualificata | Richiede vista realmente proiettata, `viewId`, layout/crop/riflessione, frame e occlusione. L'equirettangolare da solo non chiude il gate |

Decisione SC0: il primo profilo resta **domemaster safe**, per limitare le variabili del test a due puntatori. L'equirettangolare è un'alternativa già implementata da qualificare separatamente, non un prerequisito nascosto e non una funzione da riscrivere. La sessione deve selezionare esplicitamente proiezione, formato e superficie; niente cambio automatico silenzioso durante un tentativo.

### Catena temporale e spaziale richiesta

`evento con raggio sala → superficie fisica ammessa → pixel/vista dell'output → frame STK effettivamente presentato → camera e target storici → occlusione → effetto sullo stato corrente ancora valido`.

Il servizio comune usa metri, sistema destro, +Y alto e −Z davanti; il contenuto STK documenta +Z davanti. La compatibilità di base va applicata una sola volta con le fixture comuni. Una riflessione di Z ha determinante −1 e non va mascherata da quaternion. La camera mobile del gioco si applica dopo il mapping del telo, non al posto di quel mapping. [H02, S02]

Oggi `ImmersiveOutputFrame` contiene texture, target GL, dimensioni, tipo/ID di proiezione e nome server. `publishFrameTexture(... flipped:NO)` e i tempi di profiling non forniscono l'identità dell'immagine vista dal pubblico. `state_version_seen` della crew è una versione dello stato semantico e ha una finestra propria; non è un frame video. Anche la generation della connessione Syphon host è necessaria ma insufficiente. [S04, S05, H06]

Prima di accettare colpi reali, SC1 dovrà qualificare un'associazione producer/host con identità di frame e stream, race generation, tempo di simulazione/cattura, pose camera/capture, revisione proiezione/layout e intervallo di presentazione con incertezza. Deve esistere uno storico bounded di target/istanze e occlusione coerente. Questi sono requisiti da implementare nel contratto pertinente, **non nomi di campi già disponibili**. In caso di associazione assente/ambigua si rifiuta il colpo; nessuna selezione del frame favorevole o della posa più recente.

## 4. Input comune: disponibile e mancante

SI2 offre codec e schemi chiusi, clock bridge↔host, provider WSS, cache di pose, pipeline e runtime comuni; la facade sala è posseduta dalla composizione app. `drain()` è un confine interno host, non un SDK consumer raggiungibile da STK. La pipeline richiede binding e calibrazioni qualificati forniti dal proprietario della sala; non li deduce dalla connessione. [H01–H04]

Per Scintilla sono necessarie `roomRay6dof`, `eventTimePose`, `primaryAction` e un secondo controllo indipendente. Decisione di mapping per il profilo: `primary` → sparo; `secondary` → `tool`. L'host può mappare il pulsante fisico adatto a `secondary`, ma il bridge deve prima acquisirlo e negoziarlo. L'attuale insieme annunciato contiene solo le prime tre capability: il profilo completo deve quindi risultare **non preparabile**. Non simulare `tool` con un long-press che sottrae il comando di fuoco e non aggiungere un driver in STK. [H05]

Il wire comunica press/release, aim, cancel e lifecycle; l'azione mantenuta nasce nel consumer dopo un press valido e vive solo con campioni freschi e contesto valido. Un release può essere senza raggio. Ogni impulso automatico deve avere identità/tempo/campione propri. I limiti comuni attuali sono **60.000 µs di età e 20.000 µs di incertezza**, con verifica dell'intero intervallo: tetti di ammissione, non latenza fisica misurata. L'adapter STK deve aggiungere il clock host↔processo STK e la qualifica del frame, senza riusare offset di altri giochi.

Il conteggio `players` del descriptor comune è oggi verificato contro `sources.count`. Per Scintilla il descriptor deve quindi indicare **1–5 puntatori**, mentre il roster di gioco è **2–6 persone incluso il pilota**. Il wire fornisce `sourceId` e revisioni, non l'identità della persona: il binding autorizzato partecipante↔sorgente appartiene alla sessione host. Non usare indice USB, left/right o device ID per contare i contributori B/C. Al cambio pilota si aggiorna quel binding e si richiede release-before-rearm, senza rifare la calibrazione.

## 5. Pool di powerup: stato attuale e decisione di profilo

**Il pool attuale è vanilla, senza filtro Scintilla.** `Powerup::hitBonusBox()` usa `PowerupManager::getRandomPowerup()` e i pesi di `data/powerup.xml`. I tipi sono dieci: bubblegum, cake, bowling, zipper, plunger, switch, swatter, rubber-ball, parachute, anvil. Nella race-weight-list letta anvil ha peso zero; gli altri nove compaiono con peso positivo. `anchor` è il nome dell'asset XML associato all'anvil, non un undicesimo powerup. [S07]

Decisione per il primo profilo controllato, **da applicare nella fase di implementazione pertinente**, a kart umano e AI allo stesso modo:

| Elemento | Policy scelta | Motivo / verifica successiva |
|---|---|---|
| Zipper | Consentito, dosi native | Agisce sulla guida senza generare attacchi o scudi concorrenti; misurare velocità ed esposizione dei bersagli |
| Cake, bowling, rubber-ball, plunger | Esclusi | Proiettili, effetti offensivi o disturbo visuale fuori dalla policy breccia/difesa |
| Bubblegum | Esclusa | Un comando genera shield attachment o pickup offensivo in funzione del look-back; non equivale allo shield Relay |
| Swatter | Escluso | Attacco tramite attachment, non coordinato dalla policy Relay |
| Switch | Escluso | Cambia la natura dei pickup, reintroducendo combinazioni non controllate |
| Parachute, anvil come powerup | Esclusi, anche se un peso è già zero | Effetti selezionati per posizione, privi di mira/preavviso Relay |
| Nitro piccola/grande, zipper di pista, skid/guida | Conservati | Risorse native del pilota, separate da energia di servizio e carico |
| Banane di pista | Conservate come ostacoli visibili; **solo penalità native anvil/parachute**, senza generare bombe nel profilo | La banana vanilla può creare `ATTACH_BOMB`, trasferibile per collisione. Escludere solo le armi delle scatole non basta |
| Collisioni, cadute, rescue, mesh e grafo AI | Conservati | Rischi della guida. Non generano perdita di modulo o credito per una combo Relay |
| Carico missione | Stato dedicato, fuori da `Attachment` e pickup vanilla | Raccolta unica, nessun premio doppio, nessuna cancellazione per cleanup dell'esplosione |

Su Snow Peak l'XML esaminato contiene **13 elementi `item`, 4 banane e 4 `small-nitro`**. Sono conteggi statici del file, non un censimento di oggetti caricati a runtime. La scelta sulle banane è quindi pertinente alla pista iniziale. Nessun asset o XML è stato modificato.

Il futuro filtro deve intervenire sull'assegnazione/uso per il solo profilo Scintilla, senza riscrivere globalmente `powerup.xml`; deve coprire umano, AI, reset e inventari residui. Le scatole non devono mostrare un'arma esclusa per poi annullarla invisibilmente. La restrizione delle banane deve conservare il pickup visibile e scegliere una delle due penalità ammesse, senza bomba trasferibile. La regressione vanilla è criterio della futura patch, non una verifica dichiarata qui.

Per la prova tecnica del puntamento le azioni crew legacy devono essere disabilitate (`crew_mode: disabled` esiste già), con futura abilitazione separata del consumer Scintilla. Oggi attivare quel valore non crea il nuovo consumer. Il fallback `CrewActionHandler::findRivalKart()` verso un rivale disponibile e `slowRival()` (squash/rallentamento) non sono percorsi validi di tiro Scintilla. [S06]

### Sobbalzo e immunità

`ExplosionAnimation::create()` può rifiutare per invulnerabilità, goal phase o shield; il costruttore assegna invulnerabilità e cancella l'attachment. La durata viene dalle proprietà del kart, e il distruttore azzera le velocità e ripristina le camere ordinarie. [S08]

SC2 dovrà isolare durata/intensità e recupero Relay conservando i default vanilla, verificando animazioni incompatibili e avvenuta creazione prima di assegnare l'esito. Un obiettivo di 1 s non equivale a 1 s perso nella gara. La protezione proposta di 4 s parte dopo il recupero e non deve essere preimpostata in modo da far rifiutare la chiamata nativa. Carico, shield Relay e credito dei contributi restano separati dall'attachment.

## 6. Parametri della baseline di lavoro

Il [profilo JSON](profile.json) registra scelte, valori e stato di implementazione. Le etichette `crew_standard`/`crew_party` sono nomi del design, non valori nuovi da inviare oggi a `/operator/configure`.

| Parametro | Prima baseline | Natura |
|---|---|---|
| Pista / kart / AI | `snowtuxpeak` / `tux` / 3 AI | Scelta SC0, coerente col launcher corrente; un solo kart umano |
| Partecipanti prima prova | P=3, C=2 puntatori | Raccomandazione del design; nessun hardware certificato |
| Gamma del prodotto | P=2…6; C=P−1 | Requisito di questa modalità; il solo pilota vanilla/Relay resta un altro profilo |
| Giri | P2/P3=6; P4=8; P5=10; P6=12 | Confermato in `getRecommendedLapCount()`, che ignora l'override richiesto |
| Handoff | Ogni giro, conferma richiesta; tempi e freeze esistenti | Conservare; nuovi comandi disarmati e timer gara sospesi |
| Difficoltà / assistenza | `easy`, adattiva OFF | Baseline riproducibile; il codice corrente già permette l'assistenza disabilitata |
| Crew telefoni / Party | Legacy OFF nella prova; Party OFF | Isolare l'effetto dei puntatori; nessun controller registrato come telefono |
| Proiezione | Domemaster 1536×1536, cubemap 768, pitch 35° | Preset provvisorio esistente, non benchmark M1 |
| Camera | stabilized_chase; yaw/roll 0°, horizon ON, roll limit 12°, forward offset 0,25, vertical 0,4, smoothing 0,16 | Parametri correnti; non nuova calibrazione della sala |
| P2 | Cinque colpi della stessa persona sullo stesso rivale | Requisito fisso |
| P3 | A mantiene, B colpisce | Requisito fisso |
| P4–P6 | A mantiene, B e C distinti colpiscono nella stessa apertura | Requisito fisso; non ridurre automaticamente alla perdita di un controller |
| Acquisizione / durata apertura | 0,6 s / massimo 3,5 s | Ipotesi iniziali del design, da SC2 |
| Finestra B+C / cadenza | 1,25 s / massimo 4 impulsi al secondo | Ipotesi del design, non frequenza di tracking |
| Catena solo | Massimo 2,5 s fra colpi, 8 s totali | Ipotesi del design |
| Sobbalzo / protezione | Obiettivo animazione 1 s / 4 s dopo recupero | Ipotesi da misurare con ripartenza e camera |
| Carico / recupero | 3 slot; massimo 1 modulo sganciato per impatto; finestra iniziale 3 s | Tre slot proposti dal design; 3 s scelti nell'intervallo 2,5–3 s |
| Fari / vittoria | Conferma automatica persistente; posizione ufficiale 1 e 3 moduli al finish | Requisito SC4; fari e punteggio non sostituiscono la consegna |

Energia di servizio, costo della difesa/boost, cadenza e anticipo degli attacchi AI, range/volumi di lock, posizione degli ancoraggi e composizione del retrovisore non hanno valori provati nel materiale. Restano parametri da determinare rispettivamente in SC2/SC3/SC4 con geometria e velocità reali. Non vengono riempiti con numeri presentati come misurati; nessuna di queste meccaniche è attiva nel profilo eseguibile corrente. Il futuro configuratore deve rifiutare un profilo incompleto per la funzione richiesta, non sostituire default nascosti.

## 7. Gate aperti e prova richiesta

| ID | Mancanza / proprietario | Prova che la chiude |
|---|---|---|
| G01 | Secondo controllo — bridge e profili comuni TholusOS | Capability `secondaryAction`, press/release indipendenti su ciascuno dei due dispositivi; perdita tracking e reconnect senza press fantasma |
| G02 | Accessori, binding e calibrazione — SI3/SI4 | Due dispositivi registrati e qualificati con revisioni sala/grip vere, senza store STK duplicato |
| G03 | Trasporto consumer/SDK e adapter STK — SI5 + incremento STK autorizzato | Contratto distribuito/versionato, client C++ compatibile, autenticazione e direction dei messaggi, lease, contesto e code bounded; fixture comuni valide/invalide |
| G04 | Frame presentato e mapping — host STK + producer, SC1 | Clock distinti, correlazione output/frame/camera/istanze, occlusione, curve, delay/reconnect/resize; rifiuto dei casi ambigui |
| G05 | Backend e set fisico — Spatial Input/venue | Computer/backend esistente identificato, configurazione con/senza HMD dichiarata, due identità/pose/azioni reali; misure temporali e spaziali |
| G06 | Retro — SC3 + mapping host | Alert separato dalla vista; superficie fisica e vista virtuale coerenti, occlusione e difesa testate; tiro nel retrovisore solo dopo qualifica del suo mapping |
| G07 | Powerup/scudi/stagger — SC2 e SC3 | Policy della sezione 5 applicata soltanto al profilo, nessun bypass da AI/pickup/banane/legacy, camera e vanilla verificati |
| G08 | Risultato e carico — SC4 | Tutti e quattro gli esiti al traguardo, posizione ufficiale e inventario congelati una volta; force-finish/abort mai promossi a vittoria competitiva |
| G09 | Prestazioni e comportamento umano — SC6 | Camera mobile, due controller, ombre, shield e retro effettivo; M1 target e playtest. P≥4 richiede almeno tre puntatori reali |

Il rapporto SI2 segnala un helper POSIX/OpenVR da qualificare e un launcher che rifiuta Windows; la wheel macOS x86_64 esaminata non si caricava nel processo arm64. Questo non dimostra l'impossibilità di ogni backend Mac. SC0 non ha interrogato computer remoti, installato runtime/driver, eseguito pairing o aperto USB/SteamVR. L'hardware è **non qualificato in questa consegna**, non dichiarato assente dalla sala.

Per una futura SC1 si possono sviluppare verifiche software con fixture identificate senza attendere la qualifica fisica, ma non chiudere il collaudo reale finché G01–G05 restano aperti. L'approvazione di SC0 non autorizza quelle implementazioni né le fasi SI parallele.

## 8. Verifiche eseguite e criterio di chiusura

- Lettura del design completo, delle sezioni del piano comune pertinenti al consumer STK, dei contratti proprietari e dei percorsi di codice indicati; ricognizione read-only di asset, toolchain e cache di build.
- **PASS: 5 test esistenti** con `python3 -B tools/immersive/CheckRelayProjectionSource.py` contro il checkout principale: cardinali, 4.000 round trip, seam/poli, copertura a sei facce su 7.381 posizioni, wiring del gate/thread. [Log](evidence/projection-source-check.log).
- Controllo dei link locali, parsing dei JSON consegnati, coerenza del profilo con conteggi/parametri, diff della sola consegna e confronto degli hash del WIP preesistente. Esiti nel [verbale](evidence/verification.json).
- Nessuna build del gioco o dell'app host, compilazione shader, esecuzione STK, chiamata API operativa, ricezione Syphon, prova controller o proiezione fisica. Le cache e il binario esistente non sono attribuiti al sorgente corrente senza build receipt.

**Criterio SC0 soddisfatto:** checkout e riuso mappati; capacità input/output distinte dalle qualifiche; pool attuale e pool di profilo espliciti; parametri iniziali e ipotesi identificati; nessuna assunzione critica su frame, retro o hardware descritta come funzione pronta. Nessun sorgente gameplay/host, asset, contratto wire o test è stato modificato da SC0. Il worktree documentale temporaneo è rimosso dopo la copia verificata della consegna; cache e altri worktree sono conservati.

## 9. Fonti locali

Gli hash nel manifesto identificano le versioni lette; i link portano ai file locali, che potranno evolvere. I riferimenti al repository host sono esterni a questo repository e non ne importano codice o istruzioni operative nelle fasi Scintilla.

- **S01:** [CMake](../../../../CMakeLists.txt), [hook runtime](../../../../src/modes/immersive_relay/relay_main_hooks.cpp), [launcher](../../../../tools/immersive/RunRelayTholusOS.command), [build MVP](../../../../tools/immersive/BuildRelayMVP.sh).
- **S02:** [Contratto TholusOS/STK](../../../immersive_relay_tholustos_integration.md), estensione Native full-sphere e Presentation readiness gate; [Phase 4.6 storica](../../../immersive_relay_phase46_projection_generalization.md).
- **S03:** [Config](../../../../src/modes/immersive_relay/render/immersive_render_config.cpp), [camera](../../../../src/modes/immersive_relay/render/camera/immersive_camera_rig.cpp), [cattura](../../../../src/modes/immersive_relay/render/cubemap_render_target.cpp), [equirect adapter](../../../../src/modes/immersive_relay/render/projection/equirectangular_projection.hpp), [shader](../../../../data/shaders/immersive_equirectangular.frag).
- **S04:** [Pipeline](../../../../src/modes/immersive_relay/render/immersive_render_pipeline.cpp), [frame DTO](../../../../src/modes/immersive_relay/output/immersive_output_adapter.hpp), [Syphon](../../../../src/modes/immersive_relay/output/syphon_mac/syphon_output_adapter_mac.mm).
- **S05:** [Sessione e report](../../../../src/modes/immersive_relay/relay_session_controller.cpp), [mondo](../../../../src/modes/immersive_relay/remote/relay_world_controller.cpp), [race loop e freeze](../../../../src/modes/immersive_relay/relay_race_mode.cpp), [dispatcher](../../../../src/modes/immersive_relay/remote/relay_game_dispatcher.cpp).
- **S06:** [Crew effects e fallback](../../../../src/modes/immersive_relay/crew_action_handler.cpp), [ruoli](../../../../src/modes/immersive_relay/crew_role_manager.cpp), [director](../../../../src/modes/immersive_relay/crew_event_director.cpp), [difficoltà adattiva](../../../../src/modes/immersive_relay/relay_difficulty_director.cpp).
- **S07:** [Powerup use/selection](../../../../src/items/powerup.cpp), [tipi](../../../../src/items/powerup_manager.hpp), [manager](../../../../src/items/powerup_manager.cpp), [pesi](../../../../data/powerup.xml), [attachment/banane/bombe](../../../../src/items/attachment.cpp).
- **S08:** [ExplosionAnimation](../../../../src/karts/explosion_animation.cpp).
- **S09:** [Suite source/math](../../../../tools/immersive/CheckRelayProjectionSource.py), [audit storico](../../repo_audit.md).
- **H01:** SocialTableController `Packages/TholusSpatialInput/README.md` e `Package.swift`.
- **H02:** SocialTableController `contracts/TholusSpatialInput/README.md`, `Sources/TholusSpatialInput/Models.swift` e schemi canonici sotto `Packages/TholusSpatialInput/`.
- **H03:** SocialTableController `Packages/TholusSpatialInput/Sources/TholusSpatialInput/SpatialInputPipeline.swift`, `SpatialInputRuntime.swift` e `Modules/Accessories/SpatialInput/RoomSpatialInputService.swift`.
- **H04:** SocialTableController `docs/spatial-input/si2-2026-10-13/REPORT.md`: evidenza pregressa e limiti del backend/build; non rieseguita.
- **H05:** SocialTableController `tools/TholusTrackingBridge/README.md`, `actions/room-actions.json`, `actions/vive-controller.json`, `bridge/acquisition.py` e `bridge/openvr_backend.py`.
- **H06:** SocialTableController `Modules/GameShowControl/ImmersiveSuperTuxKartRuntimeController.swift`, `ImmersiveSTKSurfaceRoutingBridge.swift` e `ImmersiveSuperTuxKartAPIModels.swift`.
