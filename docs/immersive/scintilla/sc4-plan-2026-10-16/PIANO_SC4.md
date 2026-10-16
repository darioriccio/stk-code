# SC4 — Piano esecutivo: carico, fari, esito della missione e introduzione 2D

Data: **16 ottobre 2026**. Destinatario: agente LLM incaricato della successiva implementazione.

**Questo documento è un piano, non una dichiarazione di SC4 implementata.** Consegna corrente: analisi del codice, calcoli riproducibili e istruzioni esecutive. Nessuna build o modifica al gameplay. SC5 e SC6 restano fuori dall'implementazione richiesta da questo piano.

Procedere nell'ordine della sezione 15. Per ogni passo: leggere il percorso completo, implementare, aggiungere le regressioni indicate, ricontrollare chiamanti e casi limite, registrare le verifiche effettivamente consentite e svolte; solo dopo passare al successivo. Non sostituire una verifica mancante con una dichiarazione di successo. I numeri di bilanciamento sono proposte iniziali, non misure di capacità umane o comfort.

## 1. Risultato richiesto e vincoli

Fonte: [Design Missione Scintilla v2](../../../Tholus_STK_Missione_Scintilla_Design_v2.md), §§2.1, 6, 11, 12, 14, fase SC4 e prove C01–C06/H01–H03/R01–R03. La vittoria è esclusivamente:

```text
naturalRaceFinished && officialTeamRacePosition == 1 && securedSlotCountAtFinish == 3
```

`naturalRaceFinished` precisa il significato di `raceFinished` del design: un comando di fine forzata, una simulazione di avanzamento o una stima di arrivo non costituiscono un arrivo naturale. I fari e il punteggio non sono condizioni aggiuntive di vittoria.

Primo profilo: `sc4_cargo_three_slots_v1`; pista `snowtuxpeak` in direzione normale, Tux umano, tre AI, difficoltà easy, configurazione di gara locale già ammessa da SC2/SC3, P=2–6 e C=P−1. Conservare i giri e la rotazione già previsti dal sistema. Non cambiare dinamicamente C perché una sorgente perde tracking. La versione iniziale resta separata dalle azioni crew legacy e dalla difficoltà adattiva, come i gate attuali.

Consegnare:

- Tre alloggiamenti, inizialmente vuoti; capsule Relay con guscio apribile e modulo recuperabile con fascio, anche in sequenza dalla stessa persona con C=1.
- Al massimo un modulo sganciato per impatto SC3 effettivamente applicato; recupero temporaneo sul cavo del kart; rimpiazzi soltanto da future opportunità del tracciato.
- Tre conferme narrative dei fari, persistenti nella gara, abilitate dall'acquisizione dei rispettivi slot.
- Quattro esiti naturali distinti, congelati al traguardo e presentati coerentemente in STK e host.
- Prima introduzione 2D effettivamente fruibile su TholusOS, con materiali finali locali e passaggio verificato al gioco.

Non aggiungere inventari complessi, salute, peso, valuta, bonus energetici per i fari, trasmissioni obbligatorie, nuove AI di guida, track mesh/collisioni/navmesh, cinematiche 3D, ruoli esclusivi o Party. Nessuna raccolta richiede due persone diverse. Nessun modulo deriva da un normale pickup STK, anche se la grafica riusa una sua mesh. Non rendere più forte l'impatto quando il carico è vuoto.

## 2. Baseline, provenienza e disciplina del lavoro

Radice STK: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code`.

Radice host: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController`.

Asset pista: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-assets/tracks/snowtuxpeak`.

Il [manifest dei sorgenti](source-manifest.json) identifica HEAD e hash della baseline letta, incluso WIP non committato. Non applicare ciecamente il piano a HEAD: gran parte di SC1–SC3 attualmente esiste nel working tree. In un worktree isolato trasferire una baseline coerente e verificata, preservando tutto il lavoro concorrente; non usare reset/stash/clean. Tutti gli output e cache configurabili sul SSD montato e scrivibile. Leggere gli AGENTS dei due repository e delle directory interessate.

Riferimenti storici utili: [piano SC3](../sc3-plan-2026-10-15/PIANO_SC3.md), [qualifica nativa SC0–SC3](../sc0-sc3-qualification-2026-10-15/README.md), [test host aggiornati](../sc0-sc3-host-tests-2026-10-16/README.md), [indagine VSync](../vsync-2026-10-16/README.md). Queste prove non certificano codice SC4 futuro; il caso display inattivo/sleep-wake e la qualifica fisica venue restano distinti. Non riaprire incidentalmente tali lavori.

La richiesta attuale autorizza il piano. Per la successiva implementazione seguire i vincoli che l'utente darà allora: non ereditare il vecchio divieto temporaneo di build dal piano SC3. Se build/test sono autorizzati, eseguirli come descritto qui; se vietati, scrivere/revisionare i test e lasciare esplicita la verifica pendente.

Nel seguito `IR/` significa `src/modes/immersive_relay/`. I nomi di nuovi file, endpoint e campi sono proposte contrattuali; non esistono ancora.

| Integrazione attuale | Intervento necessario |
|---|---|
| `IR/scintilla_combat.hpp/.cpp`, `scintilla_defense.hpp/.cpp` | Conservare SC2/SC3. Aggiungere core puro `scintilla_mission.hpp/.cpp`, senza GL/Bullet/HTTP. |
| `IR/spatial/scintilla_engine.cpp` | Adattatore autoritativo, proprietà delle entità, dispatch unico dei colpi, registrazione render, consumo impatti, preparazione missione e barriera di fine. |
| `IR/spatial/scintilla_input.hpp/.cpp` | Oggi target/hit/prove incorporano snapshot combat. Estendere semanticamente a capsule/cavo; conservare prove storiche, freshness, perdite sticky e controllo istanze vive. |
| `IR/spatial/scintilla_target_lifetime.hpp` | Aggiungere l'oracolo delle entità missione reali: stato del proprietario corrente, non soltanto registro dell'ultimo frame. |
| `IR/scintilla_layout.hpp`, shader `data/shaders/utils/immersive_scintilla.frag`, proiezioni equirettangolare/fisheye | Tre slot e tre fari, feedback raccolta/recupero; render e picking condividono forme, clipping, occlusione e aree HUD. |
| `IR/relay_race_mode.cpp::update/newLap`, `src/modes/linear_world.cpp::newLap`, `src/karts/kart.cpp::finishedRace` | Rilevare il vero arrivo e congelare lo stato prima di `notifyLap`/report. Non equiparare tutte le chiamate a `finishedRace` a un arrivo naturale. |
| `IR/relay_session_controller.cpp::notifyLap/buildReportLocked`, router/dispatcher HTTP | Oggi risultato e rank sono anche basati su crew score. Aggiungere esito missione senza reinterpretare i campi legacy o permettere un report prematuro. |
| `src/tracks/drive_node.cpp`, `drive_graph.cpp`, dati XML pista | Distanze e progressi nativi per gli attraversamenti; dati Relay separati per gli ancoraggi, nessuna modifica al grafo originale. |
| Host `Modules/GameShowControl/ImmersiveSTKSpatialHTTP.swift` | Nuovo modo `missionSC4`, DTO rigorosi, `requiresCombat/Defense/Mission`; l'attuale `requiresDefense == combatSC3` va esteso esplicitamente. |
| Host `ImmersiveSTKSpatialPreparation.swift`, `ImmersiveSTKSpatialInput.swift`, `ImmersiveSuperTuxKartRuntimeController.swift`, API client/models | Intent, preparazione, lifecycle, gate Ready/Start/resume, intro e risultati; guardie async per producer/race/revisione. |
| Host `Workspace/GamesSTKSpatialInputView.swift`, `GamesWorkspaceEntryView.swift` | Stato operatore, percorso avviabile, messaggi di recupero e quattro esiti; verificare anche i chiamanti telefono. |
| Host `Modules/Shared/MediaBus.swift` e playback esistente | Intro tramite proprietà della composizione e trasporto già disponibili; niente secondo player o nuovo scheduler. |
| `tools/immersive/tests/CMakeLists.txt`, suite input/combat/defense, test app host | Collegare le nuove regressioni ai runner reali, non lasciare eseguibili o test non scoperti. |

## 3. Tempo, identità e limiti

### 3.1 Due tempi separati

Leggere la frequenza fisica F e congelarla nella preparazione. Oggi F=120 Hz, non gli FPS del renderer. Gameplay in tick interi `int64_t`; trasporto, deadline GPU, prove ottiche e watchdog in microsecondi monotoni. Pausa/handoff fermano il tempo gameplay, non il watchdog di servizio. Nessun timeout di rete modifica da solo il carico.

Per durata D in microsecondi:

```text
minTicks(D,F) = (D*F + 999999) / 1000000     divisione intera
maxTicks(D,F) = (D*F) / 1000000
```

Validare intervalli e overflow prima della moltiplicazione. Minimi arrotondati per eccesso, massimi per difetto. Una finestra è `[inizio, scadenza)`: alla scadenza è già chiusa. Accumulare intervalli reali tra tick, non entrambi gli estremi dei campioni. Non arrotondare i confronti tramite il valore visualizzato in UI.

| Parametro iniziale | Valore | A 120 Hz |
|---|---:|---:|
| Hold continuo di raccolta | 0,6 s | 72 tick |
| Hold continuo di recupero cavo | 0,6 s | 72 tick |
| Finestra cavo, dopo prima presentazione ammessa | 3,0 s | 360 tick |
| Budget tecnico di prima presentazione | 250 ms monotoni | Non è una finestra gameplay |
| Budget di interazione da qualificare per capsula C=1 | 2,0 s | 240 tick |

Finestra cavo iniziata al tick 0: hold iniziato a 287 completa a 359 e passa; iniziato a 288 completa a 360 ed è respinto. A 60 Hz i valori sono 36/180/143; a 144 Hz sono 87/432/344. A 144 Hz il hold effettivo è 0,6041667 s. Non cambiare la durata minima per ottenere un numero comodo di frame.

### 3.2 Identità e memoria limitata

Ogni transazione contiene `raceEpoch`, `missionRevision`, `entityId`, `instance`, `phaseRevision`, `participantId`, `controlEpoch` ed evento/seriale SC1. Preparazione e prove includono anche producer, sessione fisica, input epoch, roster, route, proiezione, layout e profilo. Seriali/epoch a 64 bit sul wire come stringhe decimali, con decoder rigoroso; non passare da JSON Double/Float. Hash del manifest identifica i dati caricati, non sostituisce autenticazione HTTP.

Risorse massime del primo profilo:

- 3 slot, 1 cavo pendente, 1 capsula attiva con C=1 / 2 con C≥2, 3 fari, massimo 5 feedback sorgente.
- Manifest: massimo 12 gruppi di opportunità per giro, 2 entità per gruppo, massimo 12 giri; fino a 288 ricevute di opportunità e 36 attraversamenti di stazioni, allocate e validate alla preparazione. Rifiutare profili fuori limite; non troncarsi silenziosamente.
- Consumo impatti SC3 in ordine autoritativo con high-water mark per race/human instance; nessun set illimitato di ogni pressione dell'intera gara.
- I 32 target render esistenti rimangono un limite condiviso: riservare capacità per 2 capsule + 1 cavo, poi AI, poi eventuali marker diagnostici. Una capsula non pubblicata nello snapshot/render non può essere colpibile.

Una nuova gara svuota slot, ricevute, fari e stato finale con un nuovo epoch. Pausa, reprepare e cambio pilota della stessa gara non li svuotano.

## 4. Core missione e transazioni

Core proposto C++11: configurazione immutabile validata, stato POD/vettori limitati, eventi di input già verificati dall'adattatore; nessun puntatore nativo, accesso al filesystem, GL, JSON o clock globale. Rendere testabili `advanceTo`, `openCapsule`, `updateBeams`, `consumeImpact`, `crossConfirmation`, `freezeNaturalFinish`, `abort`, `suspendInput`, `resetForRace` (nomi adattabili mantenendo il contratto).

### 4.1 Slot e conservazione

```text
Empty --collect(module M)--> Secured(M)
Secured(M) --confirmed impact--> Tethered(M)
Tethered(M) --valid recovery--> Secured(M)
Tethered(M) --deadline--> Empty + M permanently retired
```

`Tethered` riserva il suo slot ma non conta come consegnato al traguardo. Non può essere riempito da una nuova capsula. `Secured+Tethered ≤ 3`; un module ID ha al massimo una proprietà fra capsula, slot, cavo o stato ritirato. Apertura non crea un modulo; raccolta trasferisce quello della capsula; recupero riporta lo stesso modulo nello stesso slot. Non implementare i moduli come `Attachment` STK, che può essere cancellato da animazioni native.

Al commit raccolta scegliere il primo slot Empty in ordine 0,1,2, atomizzare trasferimento/ritiro capsula/evento feedback. Primo inserimento nello slot imposta `everAcquired[slot]=true`. Refill successivo non genera una nuova tappa. Nessuna energia o crew score nascosta.

### 4.2 Capsule e proprietà del fascio

```text
Scheduled -> Shell -> Open -> Collected
                    \        \
                     -> Expired
```

`primary` valida su Shell apre una sola volta, incrementa phaseRevision e produce feedback `shell_opened`. Primary su Open non raccoglie; ritorna `use_tool`. Duplicati dello stesso evento restituiscono l'esito originale senza incremento di fase. La raccolta richiede un nuovo tool press con prova di Open già presentato; un tool tenuto mentre il guscio era chiuso non si attiva automaticamente. C=1 può sparare, rilasciare e premere tool da solo.

Primo tool valido prende ownership della capsula. Ordinare i candidati per seriale autoritativo SC1, poi participantId come tie-break stabile; non per ordine di callback GPU, thread o hash map. Se una decisione precedente sullo stesso oggetto è ancora pendente, non farla scavalcare da una successiva già pronta: trattenerla entro la deadline SC1, poi chiuderla e proseguire. Altri owner ricevono `target_busy`; non sommare i loro tempi. Un partecipante possiede un solo fascio fra offesa SC2, difesa/boost SC3 e missione.

Perdita accertata di mira/occlusione, campione stale, release, cambio target/istanza/fase, pausa, handoff, roster o epoch incompatibile: annullare progresso, liberare ownership e richiedere release/press. Conservare l'invalidazione sticky: miss seguito da hit nello stesso frame non ripara il vecchio hold. Non trasferire progresso all'altro partecipante, a una nuova capsula o a un nuovo tool press.

Al completamento ricontrollare oggetto vivo, fase, slot disponibile, race attiva, proof fresca e owner. Se due capsule completano con un solo slot libero, ordine deterministico come sopra: una sola raccolta; altra rimane Open finché la finestra lo permette, feedback `cargo_full`, hold disarmato. Una singola pressione non raccoglie automaticamente la capsula successiva.

Quando tutti gli slot sono occupati, non attivare nuovi gruppi di capsule; segnare consumata quell'occorrenza di percorso. Non riproporla facendo retromarcia. Dopo una perdita, usare il successivo gruppo ancora futuro, non ricreare quello già oltrepassato.

## 5. Sgancio e recupero: il ponte SC3→SC4

L'evento corretto è `ScintillaDefense::ImpactEvent` prodotto da `confirmImpact` **dopo** che l'effetto fisico è stato applicato. Oggi è l'ultimo evento latched, non una coda. Consumare immediatamente nello stesso aggiornamento autoritativo dopo la conferma; aggiungere un drain/receipt esplicito o chiamare il core missione nello stesso punto. Non dedurre un impatto da contatti Bullet, fine volo, suono o richiesta di effetto.

Chiave di idempotenza `(raceEpoch, threatId, humanInstance)`. Verificare race e kart proprietario; processare gli eventi in ordine alla fonte, non far dipendere l'ordine da polling HTTP. Segnare consumato anche un impatto senza carico. Duplicati e seriali vecchi sono no-op; identità differente o reset non riutilizza la vecchia ricevuta. Se il produttore in futuro ammette più eventi prima del consumo, sostituire il latch con una coda limitata e consumo nello stesso tick: non perdere un evento silenziosamente.

Policy deterministica: se non c'è già un cavo, staccare lo slot Secured di indice più alto. Se carico vuoto o cavo già attivo, nessun ulteriore sgancio; l'effetto fisico SC3 resta quello normale. Questa policy rispetta «al massimo uno», evita due recuperi obbligatori simultanei con C=1 e non dipende dall'intervallo nominale degli attacchi. Non alterare cooldown, energia o forza dell'impatto SC3.

Il modulo sul cavo è una vera entità Relay non collidente, ancorata al kart, con identità e trasformazione registrate per frame. Usare una posizione frontale/laterale curata per la camera stabilizzata; non modificarne la camera né disegnarlo attraverso il kart. Cavo decorativo, modulo target geometrico: il filo non è un hitbox. Profilo e dimensioni devono superare la verifica di visibilità/occlusione descritta sotto.

Separare `TetherAwaitingPresentation` da `TetherRecoverable`: il carico è già Tethered in entrambi. La finestra di 3 s parte una sola volta quando una prova di presentazione del modulo, relativa a questa istanza/fase e alla route corrente, è validata; non basta averlo disegnato in un framebuffer non presentato. Riutilizzare la catena di presentazione SC1/SC3, senza inventare un booleano host di visibilità. Per cominciare il timer non serve che qualcuno stia mirando.

Se entro 250 ms monotoni non si riesce a dimostrare la prima presentazione, richiedere **hold tecnico dell'intero mondo** mediante il gate esistente, pubblicando `mission_presentation_unavailable`. Conservare slot e modulo, cancellare gli hold; niente perdita o recupero automatico. Un'occlusione permanente dovuta a un cattivo posizionamento è un difetto da correggere, non una ragione per riavviare all'infinito il timer. Dopo ripristino e reprepare coerente, avviare il timer solo se non era mai partito. Se era partito, continuare dal residuo conservato. Non usare il watchdog come nuova logica di pausa autonoma fuori dal controller autoritativo.

Durante il recupero normale, perdere volontariamente la mira o l'occlusione nel gioco azzera il hold ma non ferma la finestra. Una perdita globale della presentazione o un handoff usa invece l'hold del mondo e ferma tutti i timer. Alla deadline il modulo è ritirato una volta, lo slot diventa Empty, prossimo rimpiazzo da un futuro ancoraggio. Al traguardo AwaitingPresentation/Tethered contano incompleti: la prima presentazione tardiva non può cambiare il risultato.

## 6. Geometria, visibilità e tempo utile delle opportunità

### 6.1 Contratto geometrico

Non riutilizzare `ScintillaCombat::Profile::inRange` per le capsule: oggi limita i rivali a 30 m e diametro angolare minimo 1,5°. Aggiungere una policy per tipo di target, mantenendo invariato SC2. Proposta iniziale capsule: sfera r=2 m, distanza ammessa 8–75 m e diametro angolare ≥3°. Sono parametri di authoring da qualificare, non dimensioni già validate in pista; la resa può usare un modello/sagoma dedicata coerente con la superficie selezionabile.

Per centro a distanza d>r, diametro angolare:

```text
alpha = 2 asin(r/d)
d <= r / sin(alpha_min/2)
```

Con r=2 e alpha_min=3°: limite angolare 76,403100 m; domina il limite 75 m. A 75 m diametro 3,056137°: circa 17,386 pixel su una equirettangolare larga 2048. Questo non dimostra leggibilità sul telo: misurare footprint nella proiezione reale e nell'output effettivo. In domemaster non usare W/360 per convertire gli angoli in pixel.

Per picking continuare a usare il resolver storico e il confronto depth conservativo già corretto. Considerare tutti i contatti validi, quindi l'ambiguità rispetto al minimo definitivo. Near/far clip identico per marker e resolver, stessa superficie, stesso frame/camera/trasformazione. Non aggiungere epsilon positivo in depth normalizzata, fallback al centro o scelta del target semanticamente «più plausibile». Se il marker grafico non copre la sfera selezionabile, cambiare la rappresentazione o restringere la superficie condivisa: niente hit invisibili.

La sfera di 2 m è specifica della capsula. Per il cavo fissare come candidati iniziali gli offset locali kart `(±3, 5,5, 6)` metri (coordinate x/y/z), raggio **0,5 m**, distanza camera ammessa **0,75–15 m**, diametro angolare minimo **3°**. Due candidati, uno per lato: scegliere un lato prima della prima presentazione con la stessa procedura di visibilità live/storica già prevista, poi mantenerlo per tutta l'istanza; nessun salto fra i lati durante un hold. Questi offset, molto leggibili ma alti, richiedono specifica verifica di tunnel/soffitti e dimensione del cavo; se non idonei, l'authoring deve sostituirli e rieseguire il calcolo, non disegnarli attraverso la geometria.

Trasformazione: `p_world = T_kart_smoothed * offset`; registrare centro/raggio di quel frame. Origine del cavo su un socket del nucleo con la stessa trasformazione, senza usare un vecchio body transform per uno dei due estremi. L'istanza appartiene al kart reale verificato al commit, anche se il rendering usa la posa smoothed.

Esempio analitico dei default del costruttore camera, non di ogni configurazione operativa: camera locale `(0;0,40;−0,25)`, pitch 35°, target destro `(3;5,5;6)`. Sottrarre la camera, poi proiettare sulla sua base ortonormale: `x'=dx`, `y'=dy*cos(35°)−dz*sin(35°)`, `z'=dy*sin(35°)+dz*cos(35°)`. I numeri calcolati sono in `calcoli.json` alla voce `tether`; distanza circa **8,61 m**, diametro circa **6,66°**. La lunghezza dall'origine locale del kart è circa **8,67 m**; il socket effettivo cambia questa lunghezza. Il raggio non deve dipendere da dove arriva il colpo. Non applicare il minimo 8 m delle capsule al cavo, non cambiare la camera per far tornare questi numeri. Smoothing, pendenze, configurazione reale e proiezione richiedono il controllo del footprint e della depth effettivi; la base camera non si ricava dal solo heading del kart.

### 6.2 Il budget, con controesempio già calcolato

Proposta di budget C=1: 0,45 s per percezione/orientamento + 0,25 s per risoluzione primary + 0,20 s cambio/riarmo + 0,25 s risoluzione tool + 0,60 s hold + 0,25 s margine = **2,00 s**. Le latenze di trasporto/prova ottica vanno misurate: se eccedono le quote, aumentare il budget/qualificare un profilo diverso; non alzare freshness o timeout SC1 per nascondere il problema. Il budget non è un percentile umano dimostrato.

Su traiettoria rettilinea verso un bersaglio fisso, offset trasversale b<Rnear, velocità costante v, tratto utile anteriore:

```text
L = sqrt(Rfar²−b²) − sqrt(Rnear²−b²)
T = L/v
```

Rfar=75, Rnear=8, b=4 → **L=67,965054 m**.

| Velocità | Tempo utile ideale | Budget 2 s |
|---:|---:|---|
| 15 m/s | 4,531004 s | Superato |
| 17 m/s | 3,997944 s | Superato |
| 25 m/s | 2,718602 s | Superato |
| 30 m/s | 2,265502 s | Superato |
| 35 m/s | 1,941859 s | Insufficiente |
| 40 m/s | 1,699126 s | Insufficiente |

Velocità limite di questo esempio: **33,982527 m/s**. Per 40 m/s servirebbe Rfar≥87,020185 m e r≥2,277923 m per mantenere 3°. Non applicare automaticamente questo ingrandimento: tunnel, curva, HUD o pendenze possono rendere nullo il vantaggio. Il Tux easy ha base 25×0,6=15 m/s dai dati attuali, ma boost, zipper, nitro, scia e pendenza impediscono di trattarla come un tetto reale.

Su traiettoria variabile verificare il tempo di permanenza effettivo nell'insieme di visibilità, non solo `d/v`. Se si usa un bound di accelerazione a≥0 su un orizzonte T, il percorso massimo è `v0*T + a*T²/2`; il bound deve includere incrementi istantanei di velocità o spezzare l'integrale in corrispondenza di essi. Senza bound dimostrato usare replay/misure native della velocità ammessa dal profilo, senza chiamarli garanzia universale.

### 6.3 Ancoraggi: lavoro numerico già fornito, qualifica ancora necessaria

[ancoraggi-candidati.json](ancoraggi-candidati.json), prodotto da [analizza_driveline.py](analizza_driveline.py), risolve i riferimenti fra vertici dei quad e ricostruisce la driveline semplice di snowtuxpeak: **83 quad**, ciclo fra i lower center lungo **992,570572 m** in double. `DriveNode::addSuccessor` usa la distanza 3D fra lower center: non usare la sola distanza XZ. Al runtime prevale la lunghezza del DriveGraph nativo, inclusi float, direzione e convenzioni di inizio giro.

Sei candidati iniziali distribuiti lungo il giro, scelti per modesta svolta locale e larghezza: nodi **7,17,26,45,59,71** (frazioni circa 0,08848 / 0,22261 / 0,36821 / 0,63485 / 0,77541 / 0,88251). Il file riporta centri, basi ortonormali, larghezze e due posizioni laterali alternative a quota locale 3,5 m. **Le due posizioni sono alternative, non una coppia già adatta a capsule simultanee**: possono sovrapporsi e non sono qualificate. Una svolta locale ridotta non dimostra 75 m di corridoio visibile. Non copiare il JSON dei candidati fra i dati di runtime.

Creare invece dati Relay separati, proposti in `data/immersive_relay/scintilla/snowtuxpeak_mission_v1.json`, con: schema/profile revision, track/direction/hash, gruppi e istanze univoche, lap allowlist, nodo/frazione nativa di attivazione e scadenza, coordinate finite, raggio, phase policy, fino a due capsule per gruppo, tre stazioni di conferma e riferimenti asset. Caricare/validare prima della gara; niente I/O nel render loop. La qualifica è legata all'esatto hash dei dati, camera, proiezione, velocità, output e raggio; non fidarsi di un semplice campo `qualified:true` inserito manualmente.

Authoring/qualifica obbligatoria per ogni gruppo:

1. Attraversare realmente il tratto con camera stabilizzata e veicolo in movimento; verificare rettilineo/curva, salita/discesa, condizioni native di velocità e boost, occludenti, layout HUD e near/far. Usare la depth reale, non solo il grafo di guida.
2. Misurare la più lunga finestra **continua** in cui la capsula risulta presentata, visibile, selezionabile e dentro il canale autorizzato. Deve superare il budget C=1 alla velocità di progetto del tratto. Misurare anche la sequenza Shell→Open e la visibilità dopo il cambio grafico.
3. Con C≥2 qualificare due opportunità nello stesso tratto, su posizioni/fasi che non si occultino a vicenda e non saturino i 32 target; non sommare le finestre dei due oggetti per dimostrare che uno è recuperabile.
4. Dare almeno tre opportunità distinte ottenibili prima del finale, più opportunità successive di rimpiazzo. Percorrere tutti i giri configurati; il numero teorico di capsule non dimostra la loro ottenibilità. Dopo l'ultimo ancoraggio non promettere un rimpiazzo prima del traguardo.
5. Se il gruppo è fuori dal suo envelope o tecnicamente non visibile all'attivazione, registrarlo skipped e attendere il prossimo gruppo futuro. Il profilo non è pronto se questa policy può scartare sistematicamente tutte le opportunità a velocità normale. Correggere gli ancoraggi o il budget; non frenare automaticamente il pilota, cambiare pista o regalare moduli.

Le necessarie prove locali possono essere eseguite senza controller fisici per geometria, timeline e transazioni. Non simulare un grant ottico positivo per spacciare quelle prove per qualifica puntatori/venue.

## 7. Attraversamenti, riapparizioni e fari

Usare progressione del kart umano derivata dalla pista nativa, con giro ufficiale e segmento attraversato. Le notifiche legacy degli encounter e `test_accelerated_progress` non sono una fonte autoritativa SC4. Non inferire un giro soltanto dal wrap di una frazione normalizzata.

Per un segmento continuo in avanti `[s0,s1]` e punto q, attraversamento iff `s0 < q <= s1`. Ordinare tutti i punti attraversati nello step, non soltanto quello più vicino alla posizione finale. Chiave `(race, lap, anchorId, ordinal)` consumabile una volta. Retromarcia e nuovo passaggio dello stesso giro non riarmano; giro successivo genera una nuova occorrenza solo se ammessa dal manifest. Teleport/rescue/respawn non sono segmenti percorsi: saltare le occorrenze oltrepassate, riallineare l'origine, niente accensione o spawn retroattivo. Non reintrodurre un'occorrenza saltata tornando indietro.

Gli slot hanno `everAcquired[3]`; la prima raccolta abilita la stazione corrispondente. Ciascuna stazione può comparire nuovamente nei giri ammessi. Il successivo attraversamento valido dopo l'abilitazione accende `beaconLit[slot]`; il latch resta fino al reset gara anche se il modulo si perde. Attraversare prima di acquisire non accende il faro; nello stesso tick, l'attraversamento fisico precede le nuove raccolte post-fisica, quindi richiede la successiva occorrenza. Non consumare carico, richiedere hold o trasformare le tre accensioni in un gate di vittoria.

## 8. Ordine autoritativo e barriera di traguardo

Questa sezione è vincolante: implementare e testare prima di completare UI/report. Il codice corrente chiama `StandardRace::update(ticks)`, poi aggiorna Relay e SC2/SC3. `newLap` chiama prima il metodo nativo e poi `notifyLap`, che può pubblicare immediatamente un report. Inserire SC4 soltanto in fondo a `updateCombat` sarebbe errato.

### 8.1 Un tick, un solo avanzamento

Il loop nativo effettua uno step Bullet per tick. Aggiungere una fase SC4 `beginStep(nextTick)` prima dell'update nativo, solo quando il mondo è realmente attivo; mantenere `endStep` dopo, senza incrementare due volte l'orologio. Collegare il tick missione alla stessa origine/epoch del combat; non modificare arbitrariamente `simulationTick` durante una callback di rendering o HTTP. Con `ticks=0` non far avanzare scadenze. Il profilo corrente deve verificare `ticks=1` quando avanza fisica; test aggregati devono chiamare i singoli step, non consegnare N tick con una sola posa. Un valore runtime incompatibile richiede hold tecnico, non perdita o avanzamento parziale.

Ordine per lo step che termina al tick n:

1. Applicare richieste di sospensione/epoch e verificare i gate prima della fisica. Se held: nessuna fisica di kart/AI, nessuna scadenza missione, nessun credito; invalidare i mantenimenti.
2. `beginStep(n)`: scadere finestre la cui deadline ≤n; gli oggetti scaduti non possono essere raccolti nello stesso tick. Conservare lo stato pronto per un'eventuale barriera d'arrivo.
3. Eseguire lo step nativo. Nel percorso di vero arrivo, congelare SC4 come descritto sotto. Un arrivo chiude subito le ammissioni Scintilla di questa gara, prima del successivo commit SC2/SC3/SC4.
4. Se non c'è arrivo: completare i commit SC2/SC3 già previsti; consumare il relativo impatto SC3 una volta. Un impatto precede una raccolta completata nello stesso tick. Poi elaborare attraversamenti missione con lo stato di abilitazione dei fari precedente alle nuove raccolte.
5. Dispatch unico dei colpi/hold missione ammessi; aperture, raccolte e recuperi in ordine deterministico. Le fasi cambiate richiedono nuove prove presentate; non utilizzare il risultato di un colpo appena aperto per un tool già tenuto.
6. Pubblicare uno snapshot coerente. Renderer/HTTP leggono lo snapshot e non eseguono transazioni.

Gli input possono essere ricevuti prima, ma nessun callback GPU tardivo retrodata un commit. Al tick di arrivo non si accredita una raccolta che sarebbe stata risolta soltanto al passo 5. È una regola discreta esplicita e conservativa: risoluzione temporale massima 1/F, oggi **8,333333 ms**, non «precisione sub-tick» del carico. Scadenze prima della barriera e finish prima dei nuovi crediti devono avere test di uguaglianza, non epsilon.

### 8.2 Arrivo naturale e dato congelato

In `RelayRaceMode::newLap`, registrare lo stato precedente del kart umano, chiamare `StandardRace::newLap`, poi rilevare la transizione non-finished→finished dovuta a quel passaggio e al completamento ufficiale dei giri. Verificare kart umano/istanza, gara ammessa e non eliminata, modalità locale non accelerata, nessuna termination artificiale. Il metodo nativo aggiorna la posizione prima della fine. Leggere la posizione ufficiale dal kart/race manager nello stesso percorso; non stimarla da distanze, score o risultato Relay gold/silver. Verificare che coincida con la classifica nativa anche in arrivi ravvicinati.

**Prima di chiamare `notifyLap` sull'ultimo giro**, congelare un record immutabile:

```text
raceEpoch, missionRevision, finishSequence, finishTick,
nativeFinishTime, officialTeamRacePosition,
slotsAtFinish[3], securedCount, cargoCompleteAtFinish,
beaconsAtFinish[3], terminationReason="natural_finish", outcome
```

Il tempo nativo può essere interpolato fra campioni fisici. Con distanza prima della linea a, dopo b e tempi t0/t1:

```text
t_finish = t0*b/(a+b) + t1*a/(a+b)
```

Per a=0,2 m, b=0,1 m, t0=10 s, t1=10+1/120 s: **10,005555556 s**. Riportare il valore nativo senza ricalcolarlo dall'HTTP; il cargo rimane congelato secondo l'ordine discreto precedente. Non ricostruire una storia sub-tick non disponibile.

`notifyLap`, `buildReportLocked`, arcade finish/results e risposta host devono usare quel record. Non pubblicare un risultato provvisorio per poi sostituirlo dopo una query GPU. Separare il latch di finish gameplay dalla serializzazione sotto mutex: niente chiamate ricorsive al session controller mentre il suo mutex è già detenuto. Acquisire il record e consegnarlo al controller con un percorso esplicito; mantenere ordine dei lock e thread affinity attuali.

Force-finish, abort, timeout, eliminazione, producer morto e avanzamento accelerato producono `terminationReason` distinto e `outcome=not_completed`; non uno dei quattro esiti naturali. La politica legacy fuori da SC4 resta invariata. `hasFinishedRace()` da solo non basta: `StandardRace::endRaceEarly` può segnare arrivi stimati.

| Arrivo ufficiale | Slot Secured al traguardo | Outcome | Testo principale |
|---|---:|---|---|
| 1° | 3 | `mission_victory` | Missione compiuta: primi, con tutti i moduli. |
| 1° | 0–2 | `race_won_cargo_incomplete` | Gara vinta. Consegna incompleta. |
| ≥2° | 3 | `cargo_delivered_race_lost` | Carico consegnato. Primo posto mancato. |
| ≥2° | 0–2 | `mission_failed` | Missione non completata. |

Mostrare sempre posizione effettiva e `N/3` moduli; eventuali contributi reali sono secondari. Faro spento non riduce la vittoria; Tethered non vale come Secured. Fetch ripetuti/retry/disconnessione non cambiano il risultato. Dopo la barriera annullare hold, query/eventi pendenti e nuove minacce, senza distruggere il record finché il nuovo raceEpoch non è pronto.

## 9. Estensione di SC1: un percorso per tutte le prove

Aggiungere `TargetKind { Rival, DiagnosticPickup, MissionCapsule, MissionTether }` e snapshot tipizzati per kind. Per missione includere entity/instance/phaseRevision, fase e proprietà necessarie alla prova storica. Non fingere che una capsula Open sia un rivale Breached. L'eligibilità va verificata nei frame plausibili, all'ammissione e al commit corrente; una fase precedente o un'istanza ritirata non può risorgere.

Attualmente `drainCombatShots()` svuota la sola coda dei colpi: rinominare/generalizzare internamente e fare **un unico drain** nell'adattatore. Smistare per kind al proprietario corretto. Due sottosistemi che drenano la stessa coda perdono eventi; un controllo `combatOwners` prima del dispatch respinge tutti i bersagli missione. Aggiornare insieme HitEvent, snapshot, helper di eligibilità, feedback e test.

Gli hit/proof missione usano il contesto geometrico world; il kind determina l'azione. La nascita di una minaccia SC3 non invalida un hold su capsula invariata, come già corretto per WorldOffense. Viceversa passare realmente al nodo difesa chiude il hold missione. Non far arrivare un beam missione a `combat.update` come se fosse una AI, o a entrambe le logiche.

Usare namespace di ID disgiunti e verificati con i range oggi usati (kart `1+i`, pickup diagnostici `100000+i`). Soluzione proposta: allocator missione monotono con range dedicato validato alla preparazione e `TargetKind` esplicito; ID non convertiti in float shader, slot render separato dall'identità. Un nuovo oggetto allo stesso indirizzo deve avere nuova instance. Dopo scadenza/raccolta il live oracle fallisce anche se l'ultimo rendering lo mostrava ancora.

Conservare tutte le correzioni precedenti: roster non inoltrato come aim, un solo rate limit, timestamp di mira mai arretrato da campioni vecchi, confronto clock coerente, history dimensionata, polling fence prima dell'expiry, replay bounded, feedback release_required/target_busy, clipping identico e minimo definitivo dei contatti. Non copiare una versione di questi file da un vecchio commit per semplificare il merge.

## 10. Rendering, HUD e feedback del giocatore

Render non collidente di gusci, moduli, cavo e tre fari, in pass coerente con profondità storica e scena. Non aggiungere i marker alla depth scena che poi viene usata per occultare i marker stessi senza definire il pass: conservare la separazione attuale. Per la capsula usare geometria/maschera condivisa fra visibile e selezionabile; silhouette e dimensioni reali devono essere esplicite. Vietati quad on-top attraverso pareti. Nucleo sul kart decorativo, senza modificare massa, baricentro o collisioni.

Estendere il layout con tre slot readonly e tre piccoli indicatori fari. Nelle coordinate isotrope attuali, s=min(W,H), x=2(uW−W/2)/s, y=2(vH−H/2)/s (y positivo verso il basso), proposta:

- Slot: centri x=−0,18 / 0 / 0,18, y=−0,16, r=0,06.
- Fari: stessi x, y=−0,27, r=0,025.
- Retrovisore attuale termina a y=−0,34; i badge sorgenti iniziano a y=0,025. Gap minimo fra queste nuove circonferenze: 0,025, cioè 3,2 px già a s=256, 19,2 px a s=1536. Verificare anche stroke/glow/testo: i calcoli riguardano solo i cerchi, non tutta la grafica.

Incrementare `Layout::Revision`, aggiornare `classify`, `worldRectFree`, shader di entrambe le proiezioni e gate host. Tutta l'area effettivamente coperta da icone/testo è `ReservedHUD`: non si spara attraverso gli slot né si recupera cliccando un'icona readonly. Il recupero si mira sul modulo world. Il medesimo contorno effettivo deve essere escluso dalla qualifica dei target world.

Stati visivi riconoscibili per forma e testo oltre al colore: Empty, Secured, Tethered con countdown; hold con progresso, ownership, `target_busy`, `release_required`, `cargo_full`, `shell_opened`, `module_collected`, `module_recovered`, `module_lost`. Pubblicare questi esiti allo snapshot e ai feedback delle sorgenti usando il percorso esistente. Feedback finale e timer non devono sopravvivere a un cambio raceEpoch.

Audio: pericolo immediato > perdita carico > cambio pilota > missione > commento. Usare cue con ID stabile e dedup per evento; niente accodamento di vecchie voci dopo una pausa o una nuova gara. Non aggiungere un servizio TTS/LLM live.

## 11. API, preparazione e lifecycle host

### 11.1 Superficie additiva proposta

Seguire router→dispatcher→thread autoritativo esistenti, autenticazione locale e codici/error envelope attuali. Nuovi percorsi sotto `/immersive/v1/scintilla/mission/`:

| Metodo/suffisso | Contratto |
|---|---|
| POST `intent` | Abilita/disabilita il requisito SC4 in stato held/pre-gara. Idempotente per requestId e revisione; SC4 implica SC2+SC3. Non resetta il carico. |
| POST `prepare` | Valida profilo, anchor manifest, race/kart, fisica, SC1/2/3 preparate, roster, output/layout e receipt intro. Stessa revisione/race → no-op; config diversa durante gara → conflitto. |
| GET `state` | Stato coerente con richiesto/preparato/attivo/sospeso, motivi, tre slot, fari, finestra cavo, feedback e risultato eventuale. Lettura senza side effect. |
| POST `intro-complete` | Receipt host autenticata per producer/race/intro hash, `played` o `operator_skipped`; accettata solo nel pre-gara held. Non è una prova ottica né autorizza da sola Start. |

Proporre DTO versionato `schemaVersion=1`, `profileId`, `profileRevision`, `raceEpoch`, `stateVersion`, `required`, `prepared`, `active`, `suspendedReason`, `slots[3]`, `beacons[3]`, `tether`, `intro`, `result`. `result` assente prima della fine; posizione nulla per conclusione non naturale. Campi numerici finiti, enum chiusi o unknown esplicito non operativo. Decoder host che fallisce l'ammissione su conteggi/ID/range incoerenti; non sostituire un carico mancante con `[pieno,pieno,pieno]`.

Non introdurre endpoint di raccolta, di vittoria o di assegnazione moduli invocabili dall'host: le transazioni provengono solo da prove SC1 e eventi gameplay. Fixture/test possono iniettare fatti nel core puro, mai lasciare un bypass in produzione. Limitare body, contatori e messaggi d'errore secondo l'infrastruttura esistente.

### 11.2 Ready e ciclo completo

Aggiungere `missionSC4` nell'host con `requiresCombat=true`, `requiresDefense=true`, `requiresMission=true`. Usare capability esplicite del server; niente downgrade silenzioso a SC3 se un endpoint manca. Intent richiesto persistente fino a conferma coerente, per evitare la corsa «Pronto prima della preparazione». Coprire tutti gli ingressi: operatore, telefono, countdown, ripresa, results→Pronto, reset e avvio da API. Il server impone il gate anche se la UI è aggirata. Impostare atomicamente il requisito SC4 e i prerequisiti SC2/SC3 prima di restituire successo a `intent`: non lasciare una finestra avviabile fra tre chiamate indipendenti. Non ammettere la disattivazione del requisito mentre la gara è attiva.

Ordine normale: selezione SC4 e intent richiesto → mondo held → intro → ritorno alla route STK → prove di presentazione fresche → preparazione SC1, SC2, SC3 e SC4 → abilitazione Ready/Start. Se alcuni prerequisiti sono già preparati e coerenti, riusarli; non resettare moduli o chiudere il grant per comodità.

Pausa/handoff: conservare slot, fari, ricevute, tempo residuo e risultato; cancellare ownership/progresso tool e richiedere release/press. Il nuovo pilota e i supporter sono associati ai participantId corretti, conservando il grant fisico del produttore quando ancora valido. Non ricreare il difetto precedente che chiudeva l'endpoint in confirmHandoff/reset lasciando il produttore occupato. SC4 non cambia ruoli o rotazione SC5.

Reset/nuova gara: disarmare input, nuovo raceEpoch, carico/fari vuoti, nuove ricevute e result nullo; ricomporre prep sulla stessa istanza producer quando supportato. Nessun bisogno normale di chiudere e riaprire STK. L'intro torna richiesta per la nuova gara; operatore può saltarla esplicitamente. Reprepare della stessa gara non riproduce automaticamente l'intro e non ricarica lo stato missione.

Ogni operazione async host deve validare generation, producer UUID, raceEpoch, input session, roster e revisioni prima di pubblicare il risultato. Risposta di preparazione o playback appartenente alla gara precedente è ignorata. Cambiare route/proiezione invalida la preparazione interessata e richiede nuova prova; non invalidare il carico della gara. Completamento intro e callback media non possono far partire una gara già sospesa/annullata.

## 12. Introduzione 2D completa, senza nuovo motore media

Produrre sei pannelli grafici definitivi (vettoriali/2D o immagini con provenienza e licenza), composti per la zona frontale della cupola, più il primo asset video locale e i sottotitoli. Niente placeholder, stock senza licenza o schermate developer. La prima versione può usare grafica vettoriale curata e testi animati, senza obbligo di generazione di immagini o voiceover; deve essere comprensibile anche senza audio. Una voce preregistrata è una traccia opzionale del manifest, mai sintetizzata al runtime. Se presente, montare sul parlato reale e ricalcolare la timeline, non accelerare la voce per farla entrare.

Timeline base: **40 s, 30 fps, 1200 frame**, eventuale audio PCM 48 kHz: **1600 campioni/frame, 1.920.000 campioni**. Cut frame `[0,180,360,570,780,990,1200]`; cut campioni `[0,288000,576000,912000,1248000,1584000,1920000]`. Intervalli semichiusi, nessun frame doppio al taglio. Una traccia AAC può avere priming: usare timestamp/durata media effettivi, non il numero di byte o un timer wall-clock di 40 s per dichiarare playback completato.

| Shot | Tempo | Immagine/azione 2D | Testo leggibile principale |
|---|---|---|---|
| 1 | 0–6 s | Rete con tre fari spenti; Scintilla introduce la missione | «La rete dei fari è fuori uso.» |
| 2 | 6–12 s | Nucleo del kart con tre slot vuoti | «Recuperate tre moduli.» |
| 3 | 12–19 s | Gara e rivale con scudo; apertura prima del colpo | «Apri lo scudo. Coordina i colpi.» |
| 4 | 19–26 s | Guscio→modulo→slot, primary e tool distinti | «Rompi il guscio. Tieni il fascio per recuperare.» |
| 5 | 26–33 s | Allarme posteriore, settore difesa, modulo sul cavo | «Proteggete il carico. Recuperate i moduli sganciati.» |
| 6 | 33–40 s | Kart al traguardo, posizione 1 e tre slot pieni | «Primi al traguardo, con tutti e tre i moduli.» |

Il design fornisce anche il testo voce di Scintilla; adattarlo al montaggio rispettando l'obiettivo. Per C=1 l'esempio SC2 mostra le azioni sequenziali del suo profilo; per C=2 mostra apritore+tiratore; per C≥3 rispetta i contributi distinti richiesti dal combat. Non illustrare come obbligatorie due persone per aprire/raccogliere una capsula. Preparare le varianti di pannello necessarie e un manifest che selezioni la variante per C, senza generare media live.

Host: riusare il trasporto video e ownership `MediaBus` (`begin/apply/clearSurfaceComposition`), rispettando il token della composizione. Una singola clip precomposta evita un nuovo scheduler di sei scene. Se si usano cue/scene separati, passare dal `UnifiedCueMomentEngine`/ShowCueDirector già richiesto dall'architettura host, senza scheduler locale parallelo. Modello di stato puro in package locale coerente con la modularizzazione; il composition root app collega i servizi esistenti. Non aggiungere dipendenze del package verso l'app.

Ciclo presentation host: `Idle → Preparing → Playing → ReturningToSTK → Ready`; Error/Cancelled sono stati espliciti. Il mondo resta held per tutta l'intro. Acquisire la superficie con il proprio owner, non fermare contenuti di un altro owner arbitrariamente. Alla fine/cancel rilasciare solo le risorse possedute, ripristinare la route STK e attendere frame freschi coerenti prima di preparare/ammettere input. Non avviare il countdown su `AVPlayer` ended se nel frattempo il producer/race è cambiato.

Receipt `played` solo dopo fine media osservata; `operator_skipped` solo per azione esplicita dell'operatore, mostrando la sintesi obiettivo. Media mancante/corrotto o output non disponibile produce errore recuperabile e blocca l'avvio; nessuno skip automatico. Non chiudere il grant fisico solo perché cambia temporaneamente la sorgente media: tenere disarmato l'input e rifare le prove necessarie dopo il ritorno.

Consegne: asset finale, manifest con hash/durata/frame rate/dimensioni/varianti C/lingua/licenza, sottotitoli locali, breve README di authoring. Prevedere le chiavi di localizzazione usate dal progetto, prima variante italiana completa. Composizione sulla cupola tramite il mapping esistente; non stirare un video rettangolare su tutta la sfera.

## 13. Matrice di regressione obbligatoria

Le righe sono comportamenti, non una richiesta di produrre un test tautologico per ogni dettaglio. Usare core puro per combinazioni esaustive, consumer reale per prove storiche, integrazione per lifecycle e barriera nativa, GPU per maschere/depth. Agganciare tutti i test ai runner esistenti.

| ID | Caso | Verifica |
|---|---|---|
| M01 | C=1, stessa persona apre e recupera | Una raccolta dopo nuova pressione e hold completo; nessun requisito di identità distinta. |
| M02 | Due primary sul guscio, retry e riordine | Una sola apertura; query pronta dopo non scavalca la precedente pendente. |
| M03 | Due tool sulla stessa capsula | Un owner, target_busy per l'altro; nessuna somma di tempi. |
| M04 | Due capsule, un solo slot libero | Un trasferimento; altra Open/cargo_full, nessun ID in due slot. |
| M05 | Tool già held prima di Open | Nessuna raccolta finché non release/press con prova della nuova fase. |
| M06 | Miss→hit prima del tick gameplay | Negativo sticky, progresso annullato; hit successivo non ripara lo stesso hold. |
| M07 | Cambio target/source/participant/epoch | Nessun trasferimento di ownership/progresso; feedback di riarmo. |
| M08 | GPU/proof della capsula già raccolta/ritirata | Rifiuto anche se registro del rendering è vecchio; nuova instance allo stesso indirizzo non coincide. |
| M09 | Impatto applicato vs richiesto/rifiutato/difeso | Sgancio solo sull'applicato, una volta per chiave. |
| M10 | Impatto con 0/1/2/3 slot Secured | Nessun underflow, al massimo uno; slot deterministico. |
| M11 | Nuovo impatto con cavo già attivo | Nessun secondo cavo; SC3 mantiene il normale effetto fisico. |
| M12 | Recupero a tick deadline−1 / deadline | Primo ammesso, secondo perso; 60/120/144 Hz, conteggio intervalli corretto. |
| M13 | Prima presentazione ritardata/assente/vecchia | Timer parte una sola volta sul frame corretto; hold tecnico se indisponibile, nessun regalo/perdita. |
| M14 | Pausa/handoff durante recovery | Residuo conservato, hold azzerato, mondo e AI fermi; release richiesta alla ripresa. |
| M15 | Perdita tracking mentre il gioco resta attivo | Hold annullato; finestra non si ferma per una sola sorgente. |
| M16 | Ritorno sull'ancoraggio in retromarcia e stesso giro | Nessun farming; nuova occorrenza solo nei giri ammessi. |
| M17 | Rescue/teleport attraverso anchor o faro | Nessuna ricompensa attraversando un salto non fisico. |
| M18 | Più crossing nello step, wrap ufficiale del giro | Ordine coerente e una sola ricevuta per occorrenza; niente wrap da semplice frazione. |
| M19 | Faro prima/dopo acquisizione e successiva perdita | Solo dopo acquisizione; acceso persistente, nessun consumo di modulo. |
| M20 | Raccolta e crossing faro nello stesso tick | Il nuovo componente non retroattiva il crossing precedente. |
| M21 | Quattro arrivi naturali | Outcome e posizione ufficiale/carico congelati esatti, indipendenti da score/fari. |
| M22 | Cavo pendente al traguardo | Cargo incompleto anche se il modulo appare ancora vicino al kart. |
| M23 | GPU pronta/collezione/expiry/impact al tick d'arrivo | Ordine §8 verificato: deadline prima, finish prima dei nuovi commit; nessuna modifica successiva. |
| M24 | Force-finish, early finish, abort, timeout, test accelerato | `not_completed`, mai vittoria naturale. |
| M25 | Finish HTTP retry/reconnect/re-fetch | Stesso record e finishSequence; nessun report provvisorio. |
| M26 | Ready da UI/telefono/API e results | Blocca SC4 non preparata; nuova gara recuperabile senza riavvio producer. |
| M27 | Reprepare stessa gara vs reset | Primo conserva inventario/fari; secondo riparte vuoto con epoch diverso. |
| M28 | Handoff operatore e telefono | Stesso grant valido riusabile, partecipanti corretti, nessun endpoint occupato senza recupero. |
| M29 | Nuova minaccia SC3 mentre si raccoglie | Nessuna invalidazione per solo threatId; cambio reale a difesa chiude il fascio. |
| M30 | Colpi rivale/capsula e hold difesa nello stesso batch | Un solo dispatch, zero eventi persi o consegnati a due core. |
| M31 | Parete davanti al modulo, near clip, bersagli in pareggio | Shader/resolver concordano, niente tolleranza depth che consenta attraversamenti. |
| M32 | 32 target, 2 capsule e cavo | Priorità coerente fra snapshot/render/input, nessun target invisibile colpibile. |
| M33 | HUD a 256, 1536², 2048×1024 e resize | Icone/testi non sovrapposti; area coperta esclusa dal world picking. |
| M34 | Capsule in curva/pendenza e a velocità/boost reali | Finestra continua qualificata; nessuna assunzione che 15 m/s sia massimo. |
| M35 | Intro C=1/C=2/C≥3, played e skip esplicito | Tutorial coerente, nessun avvio prima del ritorno alla route e prep. |
| M36 | Intro cancella/error/end tardivo/producer cambia | Nessun countdown spurio, pulizia solo proprio owner, retry operabile. |
| M37 | DTO malformati, numeri >2^53, array/asset fuori limite | Decoder preciso e fail-closed, nessun trap/allocazione illimitata. |
| M38 | Sessione massima 12 giri, 5 supporter, retry ripetuti | Memoria bounded, niente contatori esauriti durante gara normale. |
| M39 | Vanilla e SC1/SC2/SC3 non SC4 | Nessun cambio di condizioni vittoria, physics, pickup o intro legacy. |
| M40 | Gara naturale→results→nuova gara, output perso e ripristinato | Percorso end-to-end, nessun dato o comando della gara precedente. |

Mappatura design: C01→M01–M08; C02→M09–M11; C03→M12–M15; C04→M16–M18/M34; C05→M21–M25; C06→M19–M20; H01–H03→M14/M26–M29; R01–R03→M27/M36/M39–M40. Le prove matematiche allegate non chiudono queste regressioni dell'implementazione.

## 14. Verifiche consentite senza hardware e qualifica separata

Senza puntatori/venue: core e parser C++ con CTest, consumer SC1 e dispatch, host XCTest/package test appropriati, API autenticata locale, process lifecycle, rendering/depth GPU locale, asset/video decode, flusso di intro e replay, gara nativa naturale e classifiche. Per fixture iniettate documentare l'iniezione; uno stato mock non è una prova ottica fisica. Non indebolire gate per far riuscire uno smoke.

Quando build/test autorizzati, usare nuove directory task sul SSD, ricavando i comandi reali dai runner aggiornati. Per i test C++ il progetto oggi supporta:

```sh
cmake -S tools/immersive/tests -B "$SC4_TEST_BUILD" -DENABLE_RELAY_TEST_TOOLS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build "$SC4_TEST_BUILD" --parallel 6
ctest --test-dir "$SC4_TEST_BUILD" --parallel 4 --output-on-failure
```

`SC4_TEST_BUILD` deve essere impostato dall'agente a una directory nuova sotto `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/`, verificandone il mount. Non riutilizzare stato mutabile di build di altre chat. I target di questi test non sostituiscono le build vanilla/Relay del gioco. Verificare il sorgente CMake dei nuovi file, flag ENABLE_IMMERSIVE_RELAY, configurazione SERVER_ONLY/stub pertinente e packaging dei nuovi asset. Per host usare `scripts/ci/test-app-isolated.sh` e il profilo scintilla esistente, aggiungendo esplicitamente le nuove suite alla discovery/allowlist. Conservare XCTest dove adottato; non migrare framework, Swift language mode o deployment target.

Eseguire source checks e `run_scintilla_depth_gpu.py` esistenti, estesi alle forme missione. Eseguire i regressori SC0–SC3 pertinenti dopo modifiche condivise. Quando passano, non ripetere tutti i test senza nuove modifiche/dubbi. Un programma che compila il core puro non dimostra il percorso `newLap→notifyLap→report` o `intro→Ready`.

Restano prove venue: leggibilità/comfort a proiezione reale, controlli primario/tool fisici, calibrazione e occlusioni della sala, reazioni umane e difficoltà, prestazioni sul Mac target di installazione. SC6 approfondirà il bilanciamento; SC4 deve già essere corretta e giocabile localmente, non rinviare difetti strutturali a SC6. Annotare eventuali limiti reali senza bloccare lavoro software indipendente.

## 15. Sequenza di implementazione con gate e complessità

Scala: **media** = modifica circoscritta; **alta** = più sottosistemi o invarianti sensibili; **molto alta** = timing/render/input/lifecycle con interazioni. Non sono stime di ore. Ogni passo consegna codice, regressioni e nota di verifica; se la verifica richiesta è vietata nella sessione, distinguere revisione completata da esecuzione pendente.

| Passo | Cosa deve fare l'agente | Gate prima del successivo | Complessità |
|---|---|---|---|
| 0 | Rileggere baseline/WIP, AGENTS, manifest e calcoli; delimitare worktree e registrare scostamenti. | SC1–SC3 e correzioni presenti; nessun file concorrente perso. | Media |
| 1 | Definire tipi, profilo, slot, capsule, ricevute e core missione puro; validatore dati limitato. | Conservazione/ownership/reset e M01–M12/M37/M38 nel core. | Alta |
| 2 | Integrare tick missione e barriera nativa di arrivo, record immutabile e report additivo. | M21–M25, traccia reale `newLap` prima di report; fine artificiale distinta. | Molto alta |
| 3 | Estendere target kind/snapshot/live oracle e dispatch unico SC1→core corretto. | M05–M08/M29–M32; nessuna regressione delle correzioni SC1. | Molto alta |
| 4 | Implementare loader degli ancoraggi, crossing/expiry e spawn/ritiro entità Relay. | M16–M18, no farming, budget memoria, nessun pickup STK duplicato. | Alta |
| 5 | Render capsule e raccolta completa, feedback source/HUD provvisorio coerente. | C=1 sequenziale e C≥2 concorrente; depth/picking/phase concordi. | Alta |
| 6 | Consumare impatto SC3, cavo, prima presentazione, recupero/perdita e rimpiazzo futuro. | M09–M15 e finish con cavo; una sola perdita per impatto. | Molto alta |
| 7 | Aggiungere fari, layout definitivo, nuclei, audio feedback e testi risultati. | M19–M20/M31–M33; fari non alterano il gate vittoria. | Alta |
| 8 | API intent/prepare/state/intro receipt e host missionSC4; gate completi, pause/reset/rebind. | M26–M30/M37/M40, niente riavvio obbligatorio producer. | Molto alta |
| 9 | Produrre asset intro definitivi e varianti, collegare playback esistente e ritorno alla gara. | M35–M36: played/skip/error/cancel verificati, ownership route pulita. | Alta |
| 10 | Qualificare gli ancoraggi reali e il cavo: geometria, visibilità continua, velocità, giri e capacità C. | M13/M32/M34; dati runtime curati con evidenze, non JSON dei candidati. | Molto alta |
| 11 | Build/test autorizzati, regressioni condivise, ciclo naturale completo e nuova gara; aggiornare contratti/docs. | Tutti i casi software obbligatori provati o limiti nominati; nessuna dichiarazione di venue qualificata. | Alta |

Il passo 2 si può provare inizialmente con stato missione controllato nel core/test harness; la prova naturale integrata si ripete al passo 11. Il passo 5 usa dati di sviluppo dichiarati non qualificati; non promuovere il profilo pubblico prima dei gate 8–10. Se emerge un difetto, correggere e ricontrollare il passo che lo possiede prima di proseguire. Non lasciare TODO nel percorso necessario a giocare SC4.

## 16. Artefatti e definizione di completamento

Artefatti di questa pianificazione:

- [verifica_calcoli.py](verifica_calcoli.py) e [calcoli.json](calcoli.json): aritmetica dei tick, confini temporali, modello astratto di conservazione, outcome, geometria ideale, HUD e timeline. Esempi/reference, non test C++ o prova umana.
- [analizza_driveline.py](analizza_driveline.py) e [ancoraggi-candidati.json](ancoraggi-candidati.json): ricostruzione offline e candidati non qualificati, con hash degli XML.
- [source-manifest.json](source-manifest.json): provenienza del codice e documenti effettivamente usati.

Riproduzione, dalla directory di questo piano:

```sh
python3 -B verifica_calcoli.py
python3 -B analizza_driveline.py --track '/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-assets/tracks/snowtuxpeak'
```

I comandi senza `--write` confrontano i risultati con i JSON consegnati; `--write` va usato solo dopo revisione di una modifica intenzionale. Il primo script non legge il gioco né certifica il suo codice. Non aggiornare le attese soltanto per far passare un test dell'implementazione.

Al termine dell'implementazione consegnare: core/adattatori, dati pista qualificati localmente, grafica/intro e manifest con licenze, API e DTO aggiornati, test nei runner, report delle verifiche con configurazione/macchina/limiti, istruzioni operatore per preparare/giocare/riprovare. Dichiarare separatamente **codice completato**, **build/test locali superati**, **percorso naturale provato**, **qualifica fisica ancora pendente**. La compilazione o quattro outcome costruiti a mano non rendono completa la missione.

Checklist finale: raccolta ottenibile in corsa; carico conservato; perdita e recupero percepibili; niente dupliche o farming; pausa/cambio pilota/reset recuperabili; quattro risultati naturali corretti; intro reale e ritorno al gioco; vanilla e SC0–SC3 preservati. Nessun lavoro su SC5 è autorizzato da questo piano.
