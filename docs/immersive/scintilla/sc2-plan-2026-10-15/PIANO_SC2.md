# SC2 — Piano esecutivo: breccia, combo e stagger

Data: 15 ottobre 2026. Stato: **piano, nessuna implementazione SC2**.

Destinatario: agente incaricato della successiva implementazione. Il documento
fissa le decisioni, svolge i calcoli e indica i test; non richiede all'esecutore
di riprogettare la matematica. Se il checkout contraddice una premessa, fermare
quella parte, riportare l'evidenza e aggiornare il piano prima di adattarla.

## 1. Perimetro e consegna

Implementare soltanto SC2 del [design v2](../../../Tholus_STK_Missione_Scintilla_Design_v2.md):

- P=2: cinque colpi della stessa persona sullo stesso rivale.
- P=3: A mantiene la breccia, B distinto da A colpisce.
- P=4–6: A mantiene la breccia, B e C sono distinti fra loro e da A.
- Scudo, acquisizione, contributi, rimbalzo, stagger e recupero leggibili.
- Effetto sul solo rivale identificato da SC1; nessun fallback semantico.
- Attivazione dal percorso dell'app, lifecycle, policy powerup e regressioni.

Non implementare ritorsione AI, difesa di settore, energia, retrovisore, carico,
fari, vittoria di missione, Party, ruoli nuovi o bilanciamento adattivo: sono
SC3–SC6. I bonus box rimangono bersagli diagnostici SC1, senza raccolta remota.
Il fuoco automatico è opzionale nel design: **SC2 usa una pressione per colpo**,
con rilascio richiesto da SC1. Non aggiungere un generatore di autofire.

Rimane il vincolo **senza build**: niente CMake configure, compilatori, typecheck,
test C++/Swift eseguiti, avvio app/gioco/helper o prove hardware. Scrivere e
revisionare i test. Sono ammessi controlli documentali e calcoli Python puri.
SC1 è una base implementata nel working tree, ancora priva di qualificazione
integrata: non chiamarla verificata in esercizio.

Repository STK: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code`.
Repository host: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController`.
Leggere gli AGENTS applicabili. Nuovi worktree, output e temporanei solo sul SSD
montato `/Volumes/SanDisk 2TB`. Preservare tutto il WIP. **Un worktree da HEAD
non contiene SC1**: trasferire una copia verificata delle dipendenze correnti,
con manifest e hash, prima di implementare. Non ricostruire SC1 dal vecchio commit.

## 2. Evidenza del checkout e responsabilità

Il [manifest](source-manifest.json) identifica i sorgenti effettivamente letti,
inclusi quelli non committati. STK HEAD `0827c13dbec8e927ccee08ba52c97c7fa6b74a67`;
host HEAD `cd162f393207c914cbced79cd1273e7b4f081630`. Gli hash dei file contano più
dei soli HEAD. Questi riferimenti sono una fotografia, non un vincolo a ignorare
successive modifiche autorizzate.

| Sorgente relativo al proprio repository | Evidenza e intervento previsto |
|---|---|
| STK `spatial/scintilla_input.hpp/.cpp` sotto `src/modes/immersive_relay/` | Risolve pressioni storiche, non il mantenimento geometrico degli aim. `HitEvent` non contiene sessione, finestra o seriale di consumo. `events()` è un log limitato a 256: non usarlo come coda gameplay. |
| STK `spatial/scintilla_engine.cpp` | Registra marker kart r=0,35 m e item r=0,25 m; cattura metadati/depth, fa tick anche dal rendering. Non applicare danni in `beforeCapture`, negli oracle o nel router. |
| STK `spatial/scintilla_target_lifetime.hpp` | Controllo vivo del proprietario già presente. L'id del kart non basta: preservare istanza e generazione. |
| STK `relay_race_mode.cpp` | `update()` distingue world-held e normale `StandardRace::update`; `updateTime()` non avanza durante hold. È il punto di commit gameplay. |
| STK `remote/relay_command_router.cpp` | `/spatial/enable` accetta solo `{}` e richiede mondo fermo/crew legacy disabilitata. Non cambiare silenziosamente il suo significato. |
| STK `remote/relay_game_dispatcher.cpp` | Alcune GET sono servite direttamente: una nuova GET gameplay va esplicitamente instradata sul thread gioco. |
| STK `src/karts/explosion_animation.cpp` | Durata nativa, clear attachment, invulnerabilità e reset di tutte le camere nel distruttore. Una chiamata ordinaria non soddisfa SC2. |
| STK `src/karts/abstract_kart_animation.cpp` | Il costruttore rimuove il corpo fisico e può sostituire un'animazione; il distruttore lo reinserisce. `update` può fare `delete this` e confronta scadenza con uguaglianza. |
| STK `src/karts/kart_rewinder.cpp` | Ricrea l'esplosione nativa da un formato esistente: non far passare una policy locale nuova come compatibilità network/rewind. |
| STK `src/items/powerup.cpp`, `src/items/attachment.cpp` | Pool vanilla e banane ancora attivi; il profilo SC0 è una decisione documentale, non codice eseguibile. |
| STK `data/shaders/utils/immersive_scintilla.frag` | Picking, clipping e depth già corretti; feedback `hit` attuale significa contatto geometrico, non combo riuscita. |
| Host `Modules/GameShowControl/ImmersiveSuperTuxKartRuntimeController.swift` | `prepareScintillaSpatialInput` esegue enable, output e grant. Inserire qui la preparazione SC2 esplicita, con controllo delle identità dopo gli await. |
| Host `ImmersiveSTKSpatialInput.swift`, `ImmersiveSTKSpatialPreparation.swift` nella stessa cartella | Trasporto e UI da estendere per selezionare/provare il profilo; riusare le capacità native corrette, non creare un nuovo backend. |

Nomi di nuove API e file nelle sezioni successive sono **proposte da implementare**.
Non sono funzioni già disponibili. Mantenere C++11 per il core senza GL; usare
gli stessi framework di test già presenti. Per modifiche Swift applicare le
skill di concorrenza/test pertinenti e le istruzioni host, senza migrazioni.

## 3. Parametri, unità e arrotondamenti già definiti

Fonte dei conteggi: requisito utente. Fonte delle durate: design v2. Frequenza
fisica letta in `data/stk_config.xml`: F=120 Hz. Non confonderla con render FPS,
frequenza del controller o presentazione ottica.

Usare tempo di simulazione intero a 64 bit per le regole. Leggere F dalla
configurazione, congelarlo alla preparazione. Non sommare `float dt`.
Una durata D in microsecondi diventa:

```text
minimo richiesto / immunità: ceilTicks(D) = (D*F + 999999) / 1000000
massimo consentito:         floorTicks(D) = (D*F) / 1000000
```

Divisione intera non negativa; validare F e gli operandi prima della
moltiplicazione, evitare overflow. Per questo profilo i prodotti sono piccoli;
non usare comunque il `time2Ticks(float)` troncante per i minimi SC2.

| Grandezza | Microsecondi | Tick a 120 Hz | Semantica |
|---|---:|---:|---|
| Acquisizione | 600000 | 72 | Minimo; apre quando il tempo verificato raggiunge 72 |
| Breccia massima | 3500000 | 420 | Finestra `[open, open+420)` |
| B→C | 1250000 | 150 | Finestra `[first, first+150)` |
| Intervallo primarie SC1 | 250000 | 30 equivalenti | Controllo esistente sul tempo evento, NON secondo limiter in SC2 |
| Gap solo massimo | 2500000 | 300 | Nuovo contributo prima di `last+300` |
| Catena solo massima | 8000000 | 960 | Nuovo contributo prima di `first+960` |
| Stagger | 1000000 | 120 | Durata animazione; uscita fisica osservata |
| Protezione dopo stagger | 4000000 | 480 | Minimo dalla conclusione effettiva dell'animazione |

Tutte le finestre massime sono **semiaperte**. A scadenza esatta il contributo
è escluso. Gli input allo stesso tick di una scadenza arrivano dopo la chiusura;
quelli allo stesso tick di apertura possono valere solo con la prova visiva
di una breccia già pubblicata. Non aggiungere epsilon alle scadenze.
Il tick è una risoluzione di 8,333333… ms; alle altre F, ceil allunga un minimo
di meno di un tick, floor accorcia un massimo di meno di un tick.

Il limite freschezza SC1 di 60000 µs è diverso: mantenerlo sul clock monotono,
con confronto esatto `age <= 60000`, senza arrotondarlo a 8 tick (66,67 ms).
I 250000 µs di attesa GPU restano un timeout di servizio, che continua in pausa.

### 3.1 Risultati di conteggio e limiti teorici

```text
C = P - 1
P=2: Nsolo=5, nessun opener
P=3: Nshooters=1
P=4,5,6: Nshooters=2
```

Per P≥3, contare l'insieme dei participantId diversi dall'opener, non le
sorgenti né i messaggi. Una bitmask dei soli slot di roster congelati è
sufficiente internamente (massimo 5 bit); conservare gli ID negli eventi.

Cinque primarie alla cadenza ammessa richiedono almeno `(5−1)/4 = 1 s` fra
prima e quinta: 0; 0,25; 0,50; 0,75; 1,00. Non 1,25 s. In `[0,1)` sono quattro
impulsi, in `[0,1]` possono essere cinque: non introdurre un secondo contatore
in finestre mobili che contraddica questo risultato.

Catena solo: `deadline = min(first+960, last+300)`. Quattro gap da 2,49 s
durano 9,96 s: il limite totale di 8 s è necessario e non ridondante.
Il gap medio per cinque colpi in meno di 8 s deve essere minore di 2 s.

Breccia: `deadline = open+420`. Dopo il primo tiratore nel profilo P≥4:
`deadline = min(open+420, firstShooter+150)`. Se il primo colpisce 2,4 s dopo
l'apertura, restano 1,1 s, non 1,25. B e C possono colpire nello stesso tick;
la cadenza è per persona e non impone un ritardo reciproco.

Limiti ideali di blocco, trascurando ogni latenza e la difficoltà della mira:

- Solo: fra due completamenti almeno `1 stagger + 4 recovery + 1 catena = 6 s`;
  frazione temporale massima in animazione `1/6 ≈ 16,67%`.
- Cooperativo: almeno `1+4+0,6 = 5,6 s`; frazione `1/5,6 ≈ 17,86%`.

Sono limiti del modello con bersaglio sempre disponibile, non previsioni del
vantaggio in gara. Valgono solo vietando acquisizione e contributi durante
stagger/recovery. Non accreditare una preparazione nascosta durante l'immunità.

## 4. Autorità temporale e prova storica

### 4.1 Tre tempi con ruoli diversi

1. Clock host/monotono e relativo intervallo d'incertezza: SC1 verifica posa,
   presentazione, ordine e freschezza. Conservare queste prove.
2. Tick autoritativo di ammissione `admittedTick`: acquisito sul thread gioco
   quando una primaria supera l'ammissione SC1, prima dell'attesa GPU. È il
   tempo SC2 per gap solo e distanza B→C.
3. Tick di commit `commitTick`: quando l'update applica l'esito geometrico.
   La finestra deve essere ancora viva anche in questo istante.

Scelta deliberata: le finestre cooperative misurano la **simulazione STK
all'ammissione**, non millisecondi fisici fra dita. Non sottrarre offset o
inventare una conversione fra clock e tick. La jitter di trasporto può incidere
sui bordi; non viene nascosta. La GPU non cambia `admittedTick` a posteriori.
Accettare soltanto se sia `admittedTick` sia `commitTick` soddisfano il termine
applicabile. È conservativo: un risultato valido arrivato dopo la chiusura
non riapre la finestra e riceve un motivo esplicito.

Metadati nuovi interni a STK, non inviati dal controller:

```text
ShotAdmission:
  inputSessionId, inputEpoch, raceEpoch, rulesRevision
  sourceId, participantId, sourceEventId, actionSequence, admissionSerial
  admittedTick
  snapshot compatto delle finestre SC2 correnti per i target registrati
ResolvedShot:
  ShotAdmission + targetId, spatialTargetInstance, combatInstance
  esito SC1 + prova della fase/finestra in TUTTI i frame candidati
```

Nel Frame/Target storico aggiungere snapshot immutabili di `combatInstance`,
fase, attackWindowId, rulesRevision ed eleggibilità SC2. Il marker mostrato
deve usare **gli stessi snapshot**: non caricare uniformi da uno stato cambiato
dopo la cattura. `frameId` rappresentativo di `HitEvent` non basta, perché SC1
considera fino a otto frame plausibili.

Per una contribuzione cooperativa occorre lo stesso attackWindowId nello
snapshot all'ammissione, in ogni frame plausibile e al commit. Tutti i frame
devono mostrare breccia ed eleggibilità; se divergono, rifiuto esplicito.
Per il solo ogni frame deve mostrare bersaglio attaccabile, non recovery/stagger.
Così una pressione vista su shield chiuso non viene riciclata dopo l'apertura.

### 4.2 Ordine e consumo una volta sola

Non scorrere ripetutamente `events()` cercando nuovi UUID: è un log circolare.
Aggiungere una consegna interna affidabile degli esiti con seriale monotono,
o una coda drain-once limitata con slot riservato all'ammissione. Riutilizzare
il limite 64 delle azioni pendenti, senza una seconda coda illimitata.

Le primarie si applicano in ordine di `admissionSerial`, non nell'ordine di
completamento delle fence. Un esito successivo aspetta il precedente ancora
pending; miss/rifiuto/cancel producono terminali che liberano l'ordine. Attesa
comunque limitata dal timeout SC1. Nessun busy-wait o GL wait bloccante.
Release, cancel, perdita tracking e invalidazioni **bypassano** questa attesa:
chiudono il lock prima del successivo commit gameplay ed invalidano i risultati
tardivi con un nuovo epoch. La barriera di invalidazione è registrata subito
all'ingresso; la mutazione dello scudo avviene nell'update autoritativo, non HTTP.

Un retry di rete già scartato da SC1 non crea un nuovo seriale. Al livello SC2
ogni seriale viene consumato una sola volta. Non conservare tutti gli eventi
della partita: bastano watermark per epoch, slot pendenti e contributi correnti
(5 solo o 2 cooperativi). Una riconnessione non azzera recovery o policy gara.

## 5. Fascio continuo: completamento necessario del consumer SC1

`Reticle.valid && toolHeld` non dimostra la visibilità del target. Estendere
il resolver esistente a osservazioni aim senza trasformarle in colpi e senza
scrivere un secondo algoritmo di picking/depth.

Ogni prova mantiene source/participant, continuità, sequenza aim, tempo originale,
intervallo temporale convertito, frame candidati, target/istanza e stato finale.
Deve passare gli stessi mapping, clipping, consenso fra frame, depth e controllo
live di SC1. Per il mantenimento non serve che il frame mostrasse già breccia:
serve la stessa istanza geometricamente visibile. Non usare `secondary pressed`
come prova di mantenimento per tutta la durata.

Per ogni sorgente: massimo una query aim in lavorazione e un campione più nuovo
in attesa, sostituibile. Una query non pronta non va ricreata a ogni frame.
Un risultato superato non deve regredire il cursore né prevalere su un miss
più nuovo. Richiedere seriali/epoch e confronti sul tempo originale già adottato
da SC1. Eventi affidabili e cancel non si coalescono con gli aim.

Definire al tick k la validità operativa V(k):

```text
V = grant attivo AND gameplay attivo AND tool mantenuto/rearm valido
    AND prova aim risolta Visible sul target e sull'istanza del lock
    AND nessuna invalidazione/miss/target diverso successivi
    AND maxAge(now, intervallo del campione) <= 60000 microsecondi
    AND target vivo, non animato, non finito, non eliminato, eleggibile SC2
```

`maxAge = now − sampleLow` usa l'estremo prudente e la conversione SC1 corrente;
non usare l'istante in cui la fence è stata letta. Incertezza, futuro o epoch
non validi seguono il rifiuto SC1. Il termine dei 60 ms non viene allungato
per adattarsi alla GPU: se il percorso non lo sostiene, diagnostica e lock chiuso.

Acquisizione: alla prima prova V vera iniziare da zero. Sommare solo intervalli
di simulazione verificati; aprire al raggiungimento di 72 tick. Verificare V
prima di accreditare ogni intervallo: entrambi gli estremi devono appartenere
allo stesso lock valido, senza una invalidazione intermedia. Una prova divenuta
valida adesso non accredita l'intervallo precedente, iniziato non valido.
Una nuova prova tardiva non accredita
tempo passato. V falsa chiude e azzera acquisizione/contributi, senza retrocredito.
Per update con `ticks>1`, non considerare automaticamente provato tutto il salto:
usare i confini delle prove disponibili o, conservativamente, zero credito.

Mantenimento campionato: fra due prove positive si usa l'ultima prova fino a
60 ms, se non è arrivata una negazione. Non è una dimostrazione di visibilità
continua fra campioni; è la policy esplicita di campionamento. A 30 Hz il periodo
è 33,333 ms: 0,6 s comprende 18 intervalli / 19 campioni ideali. Rimangono
26,667 ms sul periodo nominale prima della soglia: non sono un budget GPU
garantito, perché vi concorrono trasporto, incertezza e scheduling.

Un miss, rilascio o cancel noto chiude subito, anche dentro quei 60 ms.
Dopo perdita lock, scadenza o completamento occorre un nuovo ciclo release/press
tool: tenere premuto non crea finestre successive automaticamente. Visualizzare
la richiesta di rilascio. Non abbassare la soglia di partecipanti.

Un dispositivo ha un fascio. Un target ha un opener. Contesa: vince la prima
acquisizione ammessa dal thread autoritativo; gli altri ricevono `target_busy`,
senza sottrarre il lock. Un nuovo opener riparte da zero e con nuova finestra.

## 6. Macchina a stati e transazioni

Nuovo core puro proposto: `src/modes/immersive_relay/scintilla_combat.hpp/.cpp`.
Niente GL, Bullet, HTTP, Swift o singletons in questo core. Input espliciti,
tick interi, output di comandi e snapshot. Un adattatore engine risolve i kart.
Non aggiungere microservizi o un framework generico di eventi.

Chiave di finestra: `(raceEpoch, inputEpoch, combatInstance, attackWindowId)`.
Il participantId e sourceEventId accompagnano ogni contributo. ID monotoni a
64 bit; a overflow rifiutare/chiudere, non riutilizzare. Il roster autorizzato
completo (1–5 crew) determina P alla prepare, indipendentemente dal tracking.

| Stato | Evento e guardia | Transizione |
|---|---|---|
| Shielded, P=2 | Primaria con prova storica valida | Apre catena personale, segmento 1 |
| SoloChain | Primaria stessa persona/istanza, entro entrambi i termini | Incrementa; a 5 emette una richiesta stagger |
| SoloChain | Miss o colpo su altro rivale | Non trasferisce né cancella immediatamente il progresso; scadenza continua |
| SoloChain | Termine raggiunto | Shielded, azzera; un colpo nuovo ammesso dopo il termine può iniziare nuova catena |
| Shielded, P≥3 | Tool press e prima prova valida | Acquiring; snapshot opener e nuovo lock epoch |
| Acquiring | 72 tick di mantenimento ammesso | Breached; nuovo attackWindowId e termine +420 |
| Acquiring/Breached | Release, miss, perdita tracking, cambio target, incompatibilità | Shielded, elimina contributi e richiede nuovo press |
| Breached | Primaria dell'opener | Feedback non-contributo; lock invariato |
| Breached | B distinto, P=3 | Una richiesta stagger |
| Breached | Primo B distinto, P≥4 | Registra B, termine min(apertura+420, B+150) |
| Breached | Ancora B | Nessun altro contributo né proroga del termine |
| Breached | C diverso da A e B, in termine | Una richiesta stagger |
| Breached | Scadenza apertura/B→C | Shielded, contributi persi, release/press richiesto |
| StaggerPending | Effetto nativo confermato una volta | Stagger |
| StaggerPending | Applicazione impossibile | Shielded senza successo, finestra consumata e motivo esplicito |
| Stagger | Animazione effettivamente terminata | Recovery con termine fineEffettiva+480 |
| Recovery | Prima del termine | Nessuna acquisizione/contribuzione, nessuna estensione |
| Recovery | Termine raggiunto e kart compatibile | Shielded |

La catena solo è per persona e istanza di rivale, quindi può essere conservata
su più rivali entro il limite dei kart registrati; non si copia passando A→B.
Alla scadenza, un vecchio risultato con `admittedTick` precedente al termine
non può avviare una catena nuova. Per un contributo già in catena, anche il
commit deve precedere il vecchio termine prima che venga calcolato il nuovo.

`StaggerPending` è una prenotazione interna, non un frame obbligatorio. Cambiare
stato e consumare la finestra **prima** della chiamata all'adattatore fisico.
Idempotency key dell'effetto = chiave finestra. Se due eventi completano nello
stesso tick, il secondo trova già la prenotazione/stagger. Non rimuovere questa
guardia contando sul numero di thread. Nessun punto/ricompensa SC4 qui.

### 6.1 Identità persistenti e reset

`ScintillaEngine::invalidate` oggi incrementa la generazione e svuota il registro
marker anche per invalidazioni input/output. Non usarlo per azzerare tutte le
immunità di gara: scollegare un controller diventerebbe un bypass del recovery.

Distinguere:

- **raceEpoch + combatInstance:** vita effettiva del rivale nel mondo; scudo e
  immunità persistono durante stop/reprepare input e variazioni del rendering.
- **inputEpoch + spatialTargetInstance:** prova della sessione/cattura corrente;
  tutte le vecchie query vengono scartate al cambio.

Tenere una mappa esplicita fra le due istanze e verificarla con l'owner vivo.
Il solo indirizzo del puntatore non identifica un respawn. Reset mondo/gara
azzera entrambi; invalidazione input azzera lock, catene, contributi e query,
ma non uno stagger in corso né il suo recovery. Roster modificato solo durante
riconfigurazione autorizzata in hold: nuove soglie/epoch, vecchie finestre chiuse.

Pausa/handoff: annullare held/lock/contributi e richiedere release al rientro;
simulazione, animazione e recovery congelati per tutti i kart. Un cancel non
equivale a cancellare una conseguenza fisica già applicata. Nessun colpo
pre-pausa viene applicato dopo la ripresa. SC2 non completa la rotazione SC5.

### 6.2 Ordine nel race loop

L'adattatore trasforma gli esiti del resolver in dati immutabili e terminali;
non chiama effetti fisici. Schema da integrare nell'ordine reale di `RelayRaceMode`:

```text
ingresso update:
    acquisisci invalidazioni urgenti e stato di hold/sessione
    se held: cancella azioni mantenute, mantieni freeze esistente, non avanza SC2
normale:
    esegui il normale update fisico/race e aggiorna la sessione esistenti
    avanza il contatore SC2 una sola volta per i tick effettivamente simulati
    ricontrolla hold/handoff, fine gara e roster (possono cambiare nell'update)
    applica invalidazioni e scadenze prima di qualsiasi contributo
    riconcilia owner/istanze e conclusione delle animazioni
    aggiorna mantenimento/acquisizione sulla base delle prove ammissibili
    consuma esiti primari in ordine, prenota e applica gli effetti validi
    pubblica lo snapshot immutabile per la prossima cattura
```

Non duplicare `StandardRace::update`, `updateRace`, avanzamento del clock o
freeze. Se il gioco conclude/entra in handoff nel tick, nessun nuovo attacco
va applicato dopo quel passaggio. Legare il tempo dell'animazione al clock
mondo previsto dalla classe nativa e verificare il confine con `updateTime`;
il contatore del core non è un secondo motore fisico.

Il core gara continua a gestire animazioni/recovery già applicati anche quando
`ScintillaEngine::enabled()` diventa falso. Il grant abilita nuovi input, mentre
il profilo agganciato alla gara mantiene gli effetti e la policy fino al reset.

## 7. Geometria e leggibilità: formule e valori iniziali

Riutilizzare il marker sferico visibile SC1, r=0,35 m per i kart. Nessuna hitbox
allargata invisibile e nessuna correzione della camera. Per distanza dal centro
camera d>r, semidiametro angolare della sfera:

```text
alpha = asin(r/d)
diametro angolare = 2*alpha
```

Scelta iniziale SC2, da verificare in sala: semidiametro minimo 0,75° e distanza
massima 30 m. Con r=0,35 m il vincolo angolare è più restrittivo:
`d <= r/sin(0,75°) ≈ 26,738794 m`. Per d≤r il target non è ammesso.
Questi sono metri del mondo STK rispetto alla camera, non metri mano/cupola.

Nel domemaster equidistante 1536, al centro, il diametro corrisponde a
`2*alpha*1536/pi = 12,8 pixel`. Nell'equirettangolare 2048×1024, il riferimento
verticale è `2*alpha*1024/pi = 8,533333 pixel`. Sono riferimenti locali di
proiezione, non garanzie di numero di pixel a ogni latitudine/bordo/canale.
Niente doppia soglia pixel nascosta: l'ammissione usa angolo e distanza.

Implementazione senza asin per l'ammissione: per parametri positivi e finiti,
`d2 > r2 && d2 <= min(Dmax², r² / sin(alphaMin)²)`. Precalcolare la costante
dal profilo validato. Renderer e resolver devono usare la stessa ammissibilità
storica. Il marker fuori range rimane leggibile come non attaccabile; non
eliminarlo prima della ricerca del contatto più vicino, svelando target dietro.

Picking rimane quello SC1: primo contatto positivo con la sfera più vicina,
ambiguità rispetto al minimo definitivo, clipping nella faccia cubemap,
occlusione conservativa D24. SC2 non modifica epsilon depth, proiezioni,
seam o politica dei pareggi. Ogni frame candidato deve soddisfare il filtro.

## 8. Stagger: policy locale e matematica dell'animazione

Scelta: estendere `ExplosionAnimation` con una **factory/policy Relay esplicita**
sotto `ENABLE_IMMERSIVE_RELAY`, mantenendo intatto il percorso vanilla per
default. Non chiamare prima la factory vanilla per poi correggere gli effetti:
attachment e invulnerabilità sarebbero già stati modificati.

Prima di costruire: stesso mondo/istanza, kart AI rivale, non eliminato/finito,
nessuna animazione, nessun recovery, mondo giocabile, nessuna invulnerabilità
o shield nativo incompatibile, gara locale compatibile. Il costruttore base può
cancellare un'animazione preesistente: verificare **prima** di invocarlo.

La policy Relay deve controllare separatamente durata, moto, cleanup camera,
clear attachment, invulnerabilità nativa ed effetto stars. Per SC2:

- Durata 120 tick, arresto durante l'animazione, velocità lineare/angolare
  azzerate alla conclusione; ripartenza normale AI. Nessun cambio di posizione
  lungo la pista, respawn, eliminazione o manipolazione del risultato.
- Non cancellare attachment (banane anvil/parachute restano possibili), non
  applicare bubblegum, non impostare invulnerabilità globale STK. Recovery della
  classe Scintilla vive nel core e non protegge da collisioni/cadute native.
- Nessun reset o cambio mode delle camere, neppure nel distruttore.
- Niente rotazioni native casuali multiple, né durata nativa delle stars.
  Riutilizzare SFX appropriati con un solo trigger dopo successo effettivo.
- Rifiutare preparazione SC2 in network/rewind/replay non supportati. Non
  cambiare il wire format vanilla né dichiarare serializzabile la policy
  locale come `KAT_EXPLOSION` standard. Audit dei guard effettivi prima di usare
  l'API: i loro nomi vanno ricavati dal checkout, non inventati.

### 8.1 Traiettoria scelta

Un sobbalzo piccolo e deterministico, non una simulazione balistica:

```text
u = clamp(elapsedTicks / durationTicks, 0, 1)  // divisione reale
w(u) = 16*u²*(1-u)²
y(u) = H*w(u)
p(u) = p0 + normal0*y(u)
theta(u) = thetaMax*w(u)
q(u) = normalize(q0 * quaternion(localRight, theta(u)))
H = 0,25 m massimo; thetaMax = 8° = 0,13962634015954636 rad
```

`normal0` e `q0` congelati all'avvio; normal normalizzata, asse di pitch locale
X. Non accumulare quaternioni frame dopo frame. Nessuna modifica yaw/roll
intenzionale. Verificare le convenzioni reali di Bullet/STK nell'adattatore.

Proprietà già derivate:

```text
w(0)=w(1)=0; w(1/2)=1; 0<=w<=1
dy/dt = (32H/T)*u*(1-u)*(1-2u)
d²y/dt² = (32H/T²)*(1-6u+6u²)
max |dy/dt| = (16*sqrt(3)/9)*H/T
max |d²y/dt²| = 32H/T²
```

Per H=0,25 m e T=1 s: velocità massima ≈0,769800359 m/s, accelerazione massima
8 m/s². Posizioni a u=0; 0,25; 0,5; 0,75; 1:
`0; 0,140625; 0,25; 0,140625; 0 m`.
Pitch corrispondenti: `0; 4,5; 8; 4,5; 0 gradi`.
Posizione/orientamento finali coincidono con quelli iniziali, velocità della
curva nulla ai due estremi. La curva è C1 rispetto alla posa ferma esterna;
l'accelerazione ha un salto agli estremi: non presentarla come C2.

Confronto calcolato, non scelta da implementare: un salto balistico di 1 s
a g=9,81 m/s² richiede v0=gT/2=4,905 m/s e raggiunge h=gT²/8=1,22625 m.
Quindi dimezzare solo la durata nativa non definisce un sobbalzo contenuto.
La curva scelta disaccoppia altezza e durata.

Non promettere assenza di attraversamento di soffitti dalla sola curva. Prima
dell'avvio limitare H con una query di clearance conservativa del volume del
kart lungo normal0 (sweep/bounding volume, non un solo raggio dal centro).
Considerare anche l'inviluppo della rotazione. Se la clearance non è provabile,
usare H=0 e thetaMax=0 per quel colpo, conservando arresto e feedback; registrare
il caso. Questo fallback geometrico non cambia bersaglio né dichiara un salto
visibile. L'implementazione del controllo va verificata sulle API fisiche reali.

Gestire fine superata (`elapsed>=duration`), non solo uguaglianza; fissare posa
finale e timer coerente prima della distruzione. La base può fare `delete this`:
nessun accesso a membri dopo la chiamata finale. In teardown/reset rispettare
`handleResetRace()` e non reinserire corpi in un mondo in distruzione.
Usare conferma di fine animazione/identità dell'effetto per entrare in recovery,
non soltanto il timer stimato del core. In hold non aggiornare l'animazione
né avanzare il suo tempo tramite `updateGraphics`.

### 8.2 Costo in gara: cosa si può calcolare

L'arresto di T secondi non implica una penalità di T secondi. Modello illustrativo
con velocità precedente v, ripartenza da zero e accelerazione costante a>0:

```text
tempo per tornare a v = v/a
distanza persa = v*T + v²/(2a)
ritardo equivalente a velocità v = T + v/(2a)
```

Esempio ipotetico v=15 m/s, a=5 m/s², T=1 s: 3 s di riaccelerazione, 37,5 m
persi, ritardo equivalente 2,5 s. Non sono misure STK: curve, collisioni,
nitro e AI cambiano il risultato. Registrare in un futuro playtest velocità
prima/dopo, ritorno al 90% della velocità e perdita di posizione. Non correggere
l'AI di nascosto per garantire un sorpasso o la vittoria.

## 9. Policy powerup e attivazione operativa

Applicare la decisione SC0 soltanto al profilo Scintilla, per umano e AI:
bonus box → un zipper; mantenere nitro e zipper di pista; escludere bubblegum,
cake, bowling, plunger, switch, swatter, rubber-ball e oggetti offensivi
anvil/parachute. Le banane mantengono le penalità locali anvil/parachute,
**senza generare o trasferire bombe**. Questa distinzione è voluta da SC0.

Hook minimi in `Powerup::hitBonusBox`, ammissione/uso inventario e
`Attachment::hitBanana`, con guard centralizzato del profilo. Niente modifiche
globali agli XML o alla distribuzione vanilla. Non fare sampling casuale fino a
ottenere zipper: scegliere direttamente il pool singleton ammesso. Audit anche
del percorso `Powerup::set`, inventario iniziale e reset. Azzerare inventario
incompatibile una volta al confine del profilo, non ogni frame. I cartelli/icone
non devono promettere armi disabilitate. Non cambiare la geometria della pista.

Il profilo rimane agganciato alla **gara**, non al grant di tracking. Una perdita
del controller non riabilita armi native. Vietare il cambio di profilo in gara;
reset/nuova gara è il confine sicuro. Non disattivare collisioni, rescue o cadute.

Proposta di API additiva, da implementare e documentare senza alterare `/spatial/*`:

```text
POST /immersive/v1/scintilla/prepare
  contract: "tholus.stk-scintilla-combat/1"
  profile: "crew_standard_sc2_v1"
  expectedRaceEpoch, expectedInputSessionId, expectedRosterRevision
GET /immersive/v1/scintilla/state
```

Gli identificatori expected sono rilasciati da STK, mai scelti dall'host per
creare un roster. Se non esposti, aggiungere un descrittore di stato/prepare
versionato e testato; non parsare log o indovinare una generazione.
Prepare valida mondo held, modalità locale, roster P2–6 completo, crew legacy
off, output/grant SC1 pronto e identità attuali. Chiamata ripetuta con stessi
parametri restituisce la stessa preparazione senza azzerare recovery. Identità
divergenti → errore, non mutazione parziale. Il client non invia durata, count,
bersaglio o potenza arbitrarî. Prima della gara si può scegliere SC1 diagnostica
oppure SC2; scelta esplicita in UI, nessun effetto fisico da solo `/spatial/enable`.

Prepare iniziale configura e arma il profilo in hold; nessun colpo è applicato
fino al gameplay attivo. Fallimento iniziale impedisce l'avvio dichiarato SC2
e ripulisce solo la preparazione corrente; non danneggia la sessione di un altro
task. Reprepare durante gara conserva policy e recovery, convalida il nuovo
input epoch e cancella i vecchi contributi. Anche GET state deve passare dal
dispatcher gioco o leggere uno snapshot immutabile pubblicato in modo sicuro.

Host: aggiungere il passo dopo `prepareScintillaSpatialInput`, mantenere
ownership e CAS di sessione/configurazione attraverso ogni await. La UI distingue
«mira diagnostica», «SC2 preparata», «input non disponibile», «richiesto reset
gara per cambiare profilo». Usare localization en/it e il checker del progetto.
Nessun nuovo driver, binding fisico o contratto comune Spatial Input.

## 10. Rendering, feedback e budget

Snapshot per target: fase, avanzamento acquisizione, count/required, opener e
contributor mask, termine, eleggibilità. Snapshot per sorgente: intento tool,
lock, motivo eventuale e ultimo esito gameplay. Non passare UUID agli shader:
slot congelati e indici numerici bastano; conservare ID nei DTO/log.

- Shielded: scudo/marker riconoscibile; P2 cinque segmenti, P≥3 icona breccia.
- Acquiring: anello `q=clamp(verifiedTicks/72,0,1)`, identità dell'opener.
- Breached: apertura chiaramente distinta, 1/2 contributi secondo il profilo.
- Stagger e recovery: segnali distinti; recovery non appare attaccabile.
- Closed-shield shot: rimbalzo; non check verde di combo riuscita.
- Miss geometrico, rifiuto di prova e rifiuto della regola restano distinti.
- Prima del press mostrare l'intento «breccia» sul target idoneo. SC2 non mostra
  icone operative di calamita/difesa ancora inesistenti. Per P2 mostrare i 5 colpi,
  senza promettere un opener obbligatorio.

Per una primaria SC2, il risultato geometrico mantiene il feedback in attesa
finché il core gameplay non lo classifica. Non mostrare per un frame il check
verde SC1 per poi sostituirlo con un rimbalzo. Un contributo accettato e uno
stagger effettivamente applicato richiedono glyph diversi: il primo non promette
da solo un effetto fisico. Il percorso diagnostico SC1 conserva il suo feedback.

Segmento j di N: intervallo angolare `[2πj/N, 2π(j+1)/N)`, con margine grafico
interno; riempito se j<count. Non ricavare count dal colore o dal tempo GPU.
Ring e glyph sono dentro il marker visibile; se sporgono, la parte sporgente è
solo feedback e non amplia il volume colpibile. Riutilizzare le stesse funzioni
depth e clipping. Non basta testare l'occlusione del centro per disegnare tutto
un anello attraverso un muro. Nessun retarget della camera o seconda scena.

Limiti presenti: 5 sorgenti, 32 marker, 8 frame candidati/azione, 64 query GPU,
64 metadati frame, al massimo 12 depth frame, budget depth 192 MiB.
Per cubemap 768 e contabilizzazione 4 byte/depth:
`Bframe=6*768²*4=14155776 byte=13,5 MiB`; 12 frame =162 MiB. Il costo reale
driver può differire. Da prima a ultima cattura, 12 frame a 120 render FPS
coprono 11/120≈91,667 ms, a 60 FPS 183,333 ms: non usare 120 Hz fisici come
se fossero FPS misurati, né moltiplicare semplicemente N/F per la copertura.

Aim: 5*30=150 osservazioni/s; ogni osservazione al massimo 8 query =1200 nuove
query/s nel caso senza riuso. Little: a 10 ms di residenza media circa 12 query,
a 50 ms circa 60. È un modello di carico, non un benchmark né una garanzia
di picco. 5 aim simultanei *8=40 slot, più 5 primarie *8=40 supera 64: occorre
backpressure esplicita e priorità alle primarie, non alzare ciecamente il limite.
Conservare al massimo un aim in volo/source e uno coalescibile; non monopolizzare
le pin depth con il mantenimento. Prima di aggiungere query verificare il budget
residuo; rilasciare i lease dopo terminale/cancel senza eliminare fence in uso.

La priorità vale per nuove submission: una query aim GPU già inviata non può
essere annullata liberando immediatamente le sue risorse. Se non c'è posto per
una primaria, attendere entro il suo timeout o dare backpressure esplicita;
non eliminare una fence ancora in uso e non saltare i frame candidati.

La sostenibilità dell'acquisizione entro 60 ms resta una verifica runtime:
registrare età della prova, attesa fence, query/pin attive, chiusure per stale
e saturazioni. Non abbassare le garanzie di SC1 per ottenere una demo verde.

## 11. Sequenza per l'agente esecutore

Completare e revisionare ciascun passo prima del successivo. Non chiedere
nuove autorizzazioni per scelte già fissate nel perimetro; segnalare invece una
premessa del piano smentita dal codice. Le stime sono ordinali, non promesse orarie.

| Passo | Lavoro concreto | Complessità | Gate software prima di proseguire |
|---|---|---|---|
| 0 | Snapshot WIP, worktree SSD, copia dipendenze SC1, confronto manifest | Media | Sorgenti SC1 reali presenti; nessun WIP perso |
| 1 | Profilo validato, tick, stato core, 3 regole e idempotenza | Alta | Revisione invarianti e fixture matematiche, inclusi bordi |
| 2 | Esiti affidabili, admission serial/tick, snapshot storici delle finestre | Molto alta | Non si può riciclare uno shield chiuso, invertire effetti GPU o duplicare un evento |
| 3 | Query aim continue condivise con resolver e gestione held/cancel | Molto alta | Ogni mantenimento ha una prova fresca; niente credito retroattivo |
| 4 | Commit nel race loop, identità persistenti, pausa/reset/retirement | Alta | Nessuna mutazione da router/render/HTTP; recovery resiste a reprepare |
| 5 | Factory/policy stagger locale e clearance, cleanup, guard network | Molto alta | Una conseguenza, camera invariata, vanilla sul ramo originario |
| 6 | Pool powerup circoscritto, confini di gara, banane senza bombe | Alta | Umano/AI simmetrici; vanilla e phone mode preservati |
| 7 | API additiva e UI host, feedback shader/stato | Alta | Percorso utente completo, glyph coerenti con snapshot e risultato fisico |
| 8 | Regressioni, review incrociata dei percorsi, documentazione evidenze | Alta | Nessuna omissione nei casi obbligatori; compilazione/runtime esplicitamente pendenti |

File nuovi sufficienti: core `scintilla_combat.hpp/.cpp`, un test core e registro
casi secondo le convenzioni esistenti, un checkpoint SC2. Policy e adattatore
possono stare nei moduli esistenti se mantengono la separazione; evitare una
classe per ogni stato. Registrare le sorgenti in `sources.cmake` e test nel
runner opt-in `tools/immersive/tests/CMakeLists.txt`, senza configurarlo.

## 12. Fixture e risultati attesi obbligatori

Tempi sotto in tick a F=120; T è il tick di apertura quando indicato. Presupporre
prove geometriche/di presentazione coerenti, salvo il caso che testa il contrario.
I test core non devono pretendere che un mock costituisca prova GPU o hardware.

| ID | Stimolo | Risultato atteso |
|---|---|---|
| M01 | P=2, hit a 0,30,60,90 | 4 segmenti, 0 effetti |
| M02 | Segue hit a 120 | Un effetto, catena consumata |
| M03 | Hit a 0 e 299 | Due segmenti; a 300 senza il secondo la vecchia catena scade |
| M04 | Hit a 0,240,480,720,960 | La quinta non completa la vecchia catena: termine totale esatto |
| M05 | Hit a 0,239,478,717,956 | Una combo valida, tutti i gap <300 e totale <960 |
| M06 | Hit rivale X, miss, hit X | Progressione X continua entro termine |
| M07 | Hit X, hit Y | Due catene isolate, nessun trasferimento |
| M08 | P=3, B prima dell'apertura | Rimbalzo; il successivo open non eredita B |
| M09 | Acquisizione 71 tick verificati, poi 72 | Chiusa a 71, aperta a 72 |
| M10 | P=3, A mantiene, B valido | Un effetto; A non può sostituire B |
| M11 | P≥4, B e ancora B | Un solo contributo, deadline invariata |
| M12 | P≥4, B e C stesso tick, distinti | Un solo effetto |
| M13 | B a T+100, C a T+249 / T+250 | Primo caso valido, secondo escluso |
| M14 | B a T+288, C a T+419 / T+420 | Primo valido, secondo escluso per termine apertura |
| M15 | Due sorgenti associate alla stessa persona | Prepare rifiutata o una sola identità conteggiata secondo contratto; mai due tiratori |
| M16 | Retry identico e stessa pressione su nuova connessione | Nessun contributo duplicato |
| M17 | Frame candidati su finestre W1 e W2, stessa geometria | Nessun contributo; non basta il target comune |
| M18 | Ammissione su W1, release, fence risolta | Nessun effetto; W1 non riaperta |
| M19 | Fence della seconda primaria prima della prima | Commit nell'ordine admission, stessi conteggi della sequenza ordinata |
| M20 | Evento geometricamente valido dopo timeout della catena | Non resuscita la catena; motivo diagnostico coerente |
| M21 | Ultima prova aim vecchia 60000 / 60001 µs | Prima ancora entro budget, seconda chiude; commit tardivo non ringiovanisce la posa |
| M22 | Miss nuovo seguito da Visible vecchio | Lock rimane chiuso |
| M23 | Posa valida ma query depth pending/unavailable | Nessuna acquisizione inventata; ultima prova solo entro budget dichiarato |
| M24 | Tool sempre held oltre 420 tick di apertura | Chiusura e release richiesto; niente riapertura automatica |
| M25 | Perdita di 1 controller con P=4 | Soglia resta A+B+C |
| M26 | Pausa lunga 30 s | Nessun avanzamento stagger/recovery; lock e vecchi comandi cancellati |
| M27 | Input reprepare/render stream reset durante recovery | Recovery conservata; vecchie query rigettate |
| M28 | Target eliminato/raccolto/finito o slot riutilizzato prima del commit | Nessun effetto sulla nuova istanza |
| M29 | Rivale già animato o factory rifiuta | Nessuna seconda animazione, nessuna conferma falsa |
| M30 | Due completamenti stesso tick | Una sola chiamata fisica con la stessa chiave |
| M31 | Stagger finisce a 120 | Recovery fino a 600 escluso; eleggibile a 600 se compatibile |
| M32 | Aggiornamento salta il tick finale dell'animazione | Finalizzazione una sola volta, nessun corpo lasciato fuori fisica |
| M33 | Reset/teardown durante animazione | Nessun accesso a mondo distrutto, né reinserimento indebito |
| M34 | Policy vanilla / Relay off | Parametri, attachment, camera, SFX, network originali preservati |
| M35 | SC2 attivo, banana e bonus box per AI e umano | Solo penalità banana ammesse; zipper dalla box; nessuna bomba |
| M36 | Disable input durante gara SC2 | Nessun ritorno del pool offensivo vanilla |
| M37 | Distanza appena sotto/sopra r/sin(0,75°) | Eligibility identica CPU/overlay; nessuna hitbox aggiunta |
| M38 | Nearest marker fuori range davanti a marker valido | Nessun tiro attraverso il primo per saltare al secondo |
| M39 | Shader hit geometrico ma shield chiuso | Rimbalzo, non conferma di stagger |
| M40 | 5 sorgenti, 8 candidati ciascuna, cache satura | Backpressure bounded; release immediata; nessun fallback al depth corrente |
| M41 | Vecchio epoch termina dopo nuova prepare | Terminale eliminato; non muta roster/stato nuovo |
| M42 | Failure/cancel host durante await preparazione | Nessuna sessione SC2 falsamente pronta o cleanup di un'altra sessione |
| M43 | GET state simultanea a reset | Snapshot coerente sul thread previsto, nessuna data race |
| M44 | Sessione prolungata con almeno 100000 azioni sintetiche nel futuro test core | Memoria limitata e nessun esaurimento del registro dopo pochi minuti |

M15 va concretizzato con il contratto corrente: SC1 già richiede binding coerenti;
il test SC2 deve comunque dimostrare il conteggio per participantId. Niente UUID
fittizi alternativi per fare passare una seconda persona.

Integrare i test SC1 esistenti (ordine aim/azione, clock, presentazione, pause,
replay, nearest tie, clipping, target lifetime, readback e occlusione); non
sostituirli con test che chiamano solo il core nuovo. Aggiungere fixture con
camera in moto, più sorgenti e frame incoerenti. Non è necessario avviare il
motore per scrivere questi casi, ma senza esecuzione restano non verificati.

## 13. Verifica e passaggio di consegne

In questo pacchetto i calcoli sono riproducibili con
`python3 -B verifica_calcoli.py`; il relativo [risultato](calcoli.json) è evidenza
matematica, non esecuzione del futuro C++/GLSL/Swift. Il manifest è un riferimento
da confrontare prima dell'implementazione. Il design SC0 contiene campi storici
ora superati: non copiarli come capacità correnti.

Alla consegna software dell'agente richiedere:

1. File modificati e percorso utente effettivamente collegato.
2. Parametri implementati, semantica temporale e motivi di rifiuto.
3. Test aggiunti/revisionati e controlli davvero eseguiti, separati da quelli
   scritti ma non eseguiti. Diff senza whitespace error e WIP altrui preservato.
4. Limiti aperti: build C++/Swift, shader, vanilla/Relay, GPU in movimento,
   due controller P3 e almeno tre P≥4, proiezione fisica, gameplay prolungato.
5. Nessuna dichiarazione «SC2 completa/qualificata» prima delle verifiche vietate
   o hardware mancanti. Stato ammesso: implementata a livello codice con
   verifiche statiche indicate e verifiche integrate pendenti.

### Prompt pronto per l'agente esecutore

> Implementa SC2 seguendo questo PIANO_SC2.md e il design v2, usando il working
> tree SC1 corrente identificato dal manifest e preservando gli altri lavori.
> Rispetta le formule, le finestre semiaperte e le decisioni esplicite; non
> ricalcolare soglie con arrotondamenti diversi. Procedi nei passi 0–8 e
> revisiona ciascuno prima di continuare. Non avviare SC3. Non fare build,
> configure, compilazioni, typecheck, test nativi, app o hardware. Scrivi i test
> e dichiara quelli non eseguiti. Se un'API reale smentisce una premessa, mostra
> la divergenza e adatta la soluzione mantenendo gli invarianti; non inventare
> chiamate, non rimuovere test e non indebolire SC1 per aggirare il problema.

L'invio a un altro agente e la scelta del suo modello costituiscono il passo
successivo: questo incarico produce il pacchetto, senza avviare l'implementazione.
