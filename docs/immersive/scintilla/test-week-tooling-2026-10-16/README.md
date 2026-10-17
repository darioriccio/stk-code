# Strumenti per la settimana di test, 16 ottobre 2026

**Scopo:** arrivare ai test con l'hardware avendo già verificato tutto ciò che
si può verificare via software, e poter cambiare il bilanciamento in pochi
minuti senza ricompilare. **Nessuna prova con puntatori VIVE, ottica del dome o
pubblico:** i numeri qui sotto vengono da gare simulate su un Mac molto carico.

Branch **`scintilla/test-week-sc5`** del worktree
`/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/test-fixes-2026-10-16/stk-code`,
sopra `main` (SC0–SC5, `b8ea7a88c`). Il lavoro precedente, nato sopra lo
snapshot SC0–SC4, è riportato commit per commit; il branch
`scintilla/test-week-tooling` resta come storico. I checkout originali non
sono stati toccati.

### Integrazione con SC5

`main` non compilava e la sua suite non era mai stata eseguita:

- due errori di compilazione: le funzioni del rapporto accettavano solo
  `std::ofstream`, e il testo equipaggio usava `video::IVideoDriver` senza
  includerne l'header;
- 9 casi su 355 e 3 controlli sorgente fallivano. Cinque casi missione sono
  risolti dalla correzione «mire all'arrivo» di questo lavoro; gli altri
  erano test SC5 scritti male (parser JSON usato su valori scalari, tempi di
  ricezione nel futuro, turbo fatto maturare senza commit) e controlli non
  aggiornati al codice SC5.

Nell'integrazione: il lampo della sonda di latenza è ora disegnato anche in
modalità equipaggio (prima non sarebbe comparso proprio con SC5), e il reset
del freno Party è stato conservato accanto a quello dell'autopilota. Suite
finale: **378/378**, tutti i controlli sorgente e i test degli strumenti.

### SC5 faceva scendere il dome da 60 a 20 fps

Con SC5 la stessa gara simulata (fisheye 1536, cubemap 768) girava a 16–21
fps invece di 52–68, anche senza misura e a GPU libera. A quella cadenza la
qualifica SC4 non può riuscire, perché le catture devono distare al massimo
50 ms, e in sala il gioco sarebbe stato a scatti.

Il campionamento mostrava il thread principale fermo per il 66% del tempo ad
aspettare la GPU. Gli A/B si fanno senza ricompilare, perché gli shader si
leggono da `data/` a ogni avvio:

| Shader di proiezione | fps |
|---|---|
| overlay SC5 com'era | 19–21 |
| senza overlay | 70,8 |
| overlay pre-SC5 | 69,0 |
| SC5 senza il ramo equipaggio nel percorso spaziale | 61,6 |
| **SC5 corretto (ogni passata HUD inlinata una volta)** | **66,2** |

SC5 aveva aggiunto una seconda copia di feedback, pannelli, HUD missione e HUD
equipaggio dentro il percorso spaziale. Su GL Apple lo shader più grande
costava circa 35 ms di GPU a fotogramma su tutti i pixel, anche dove quel ramo
non veniva mai eseguito.

Ora il corpo spaziale sta in `scintillaSpatialOverlay`, che restituisce i
puntatori come un unico strato premoltiplicato, matematicamente uguale ai
`mix` in sequenza. `scintillaOverlay` compone ogni passata HUD una volta sola
e mette i puntatori sotto l'HUD classico o sopra quello dell'equipaggio, come
prima. I checker verificano lo stesso ordine e che ogni passata compaia una
volta sola.

Durante la diagnosi anche Safari aveva occupato circa il 29% della GPU a STK
fermo. **In sala sul Mac del dome non deve girare altro che usi la GPU.**

## 1. Profili di bilanciamento

Un profilo `tholus.stk-scintilla-tuning/1` sostituisce i valori scelti; quelli
assenti restano quelli qualificati SC0–SC4 (`builtin_sc4`). STK lo legge
dall'ambiente `STK_RELAY_SCINTILLA_TUNING` a ogni nuova gara e lo tiene fisso
fino alla fine della gara. Un file non valido non cambia nulla: la gara usa i
valori incorporati e l'errore compare nello stato.

| Endpoint | Uso |
|---|---|
| `GET /immersive/v1/scintilla/tuning` | profilo attivo, sorgente, SHA-256, errore, elenco completo dei parametri con unità, intervallo ammesso e descrizione |
| `POST /immersive/v1/scintilla/tuning/reload` | rilegge il file; vale dalla gara successiva |

Il parser è rigido: chiavi sconosciute, tipi sbagliati, valori fuori
intervallo o combinazioni incoerenti producono `tuning:<percorso>:<motivo>`.

Profili consegnati, in `data/immersive_relay/scintilla/tuning/`:

| Profilo | Cosa cambia rispetto a `builtin_sc4` |
|---|---|
| `test_week_v1` | marker 0,35→0,8 m e portata 30→35 m; colpo valutato sul **reticolo disegnato** nel fotogramma presentato; tolleranza fascio 120 ms; fuoco automatico ogni 0,25 s tenendo premuto, senza avviso di cadenza; difesa con proiettile relativo all'attaccante, volo minimo 0,2 s, nessuna soluzione di lancio richiesta durante l'annuncio, turbo che **mette in pausa** la minaccia (max 1,5 s), riprova tecnica dopo 2 s; capsule non ritirate sopra la velocità qualificata; gruppi `snow_supply_2/3/5/6` spenti |
| `latency_probe_v1` | solo diagnostica: ogni pressione del grilletto fa lampeggiare di bianco il punto del reticolo per 0,1 s |

`test_week_v1` è un'ipotesi di partenza, da correggere con la telemetria.

## 2. Telemetria di sessione

Con `STK_RELAY_TELEMETRY_DIR` impostata, STK scrive un file NDJSON per gara
(`scintilla-<UTC>-race<N>-<produttore>.ndjson`, schema
`tholus.stk-scintilla-telemetry/1`). La scrittura avviene su un thread separato
con coda limitata a 8 MB; un file esistente non viene mai sovrascritto. Il piè
di pagina riporta righe accettate e scartate. Solo osservazione: la telemetria
non influisce sul gioco.

L'intestazione contiene pista, proiezione, profilo con hash e valori
effettivi. Le righe successive registrano mire e colpi (con l'angolo dal bordo
del bersaglio più vicino), note dell'input (rifiuti, disarmi, mire scartate),
fasi di combattimento, difesa, capsule e carico, giri, esito, e ogni 0,5 s un
campione con velocità, distanze, bersagli disponibili, energia e il **frame
più lungo** dall'ultimo campione.

```bash
python3 tools/immersive/scintilla/telemetry_report.py sessione1.ndjson sessione2.ndjson --json report.json
```

Il report, in italiano, dà per ogni sessione: precisione e quasi-colpi per
fascia di distanza, brecce e stordimenti al minuto, esiti della difesa,
energia, capsule e moduli, quota di tempo senza nulla da fare, latenze e
blocchi del main loop di STK oltre 250 e 600 ms.

## 3. Misura della latenza

1. Avviare STK con `STK_RELAY_SCINTILLA_TUNING=…/latency_probe_v1.json` e la
   telemetria attiva.
2. Filmare a 240 fps il controller e la cupola nella stessa inquadratura,
   premendo il grilletto con un movimento netto, una ventina di volte.
3. Dal video: fotogrammi tra l'inizio della pressione e il lampo bianco al
   reticolo, ×4,17 ms.
4. Dalla telemetria (`latency_flash`, `latency_presented`): posa→ricezione STK,
   ricezione→cattura, cattura→presentazione. La differenza con il video è il
   tratto non misurabile in software: controller, proiettori, ottica.

## 4. Equipaggio simulato

`tools/immersive/scintilla/crew_simulator.py` gioca una gara intera sul Mac:
avvia STK con l'autopilota (`STK_RELAY_AUTOPILOT=1`, SkiddingAI senza limite di
velocità rubber-band) e la vista di simulazione
(`STK_RELAY_SIMULATION=1`, `GET /immersive/v1/scintilla/simulation/view`), poi
pilota l'equipaggio attraverso le stesse API `/spatial/*` di TholusOS.

- Controller sintetici con tremore, inseguimento esponenziale, tempo di
  reazione e ritardo motorio del clic.
- Prove di presentazione credibili: un fotogramma è dichiarato sullo schermo
  solo dopo cattura + latenza di visualizzazione dichiarata (45 ms), con
  finestra di 65 ms, frontiere monotone e senza riscrivere intervalli già
  dichiarati. I test offline verificano queste regole.
- Rinnovo dell'orologio in banda ogni 0,25 s, come il nuovo TholusOS, con i
  tre campioni più stretti entro il limite di STK (50 ms), e ripiego in linea
  dopo 0,5 s. Il taglio a 20 ms di TholusOS qui lascerebbe senza campioni,
  perché in Python il thread delle sonde contende il GIL col ciclo
  dell'equipaggio. Cambio pilota a fine giro (`lap_based_handoff`).
- Ogni membro resta sul bersaglio scelto finché è valido e, se il fascio non
  prende entro 0,8 s, rilascia e riprova.
- Una prova non inizia mai sul confine di un fotogramma il cui predecessore
  non è elencato: in quell'istante erano plausibili entrambi, e STK rifiuta
  (`presentation_rewrite`) una prova che cancelli quell'ambiguità.
- Conta le risposte che TholusOS prima della correzione avrebbe rifiutato
  (`updatesOver50Ms`, `clockProbesOver20Ms`, `firstUpdateOver50MsS`).
- L'installazione si dichiara `software-crew-simulator-no-physical-evidence`:
  nessun risultato del simulatore vale come evidenza fisica.

```bash
python3 tools/immersive/scintilla/crew_simulator.py --launch --build-dir <build/relay> \
  --assets-dir <stk-assets> --runtime-dir <dir> --telemetry-dir <dir>/telemetry \
  --crew 2 --seconds 330 --tuning data/immersive_relay/scintilla/tuning/test_week_v1.json
```

Alla fine stampa un JSON con esito, frame al secondo, tempi delle chiamate API
per percorso, chiamate lente, rinnovi, input rifiutati per motivo e il file di
telemetria. Una richiesta rifiutata viene salvata in `rejected-update.json`.

## 4b. Qualifica SC4 delle capsule con l'autopilota

Senza qualifica degli ancoraggi la missione SC4 non diventa mai pronta. La
qualifica misura nel rendering nativo che ogni capsula, in fase guscio e
aperta, e ogni lato del cavo restino visibili abbastanza a lungo (più di 2 s,
3 s per il cavo) a velocità di gara. Non servono puntatori né persone, ma
servono gare guidate fino al traguardo.

`tools/immersive/scintilla/qualify_mission_autopilot.py` esegue da solo la
procedura di [OPERATORE.md](../sc4-implementation-2026-10-16/OPERATORE.md):
1. gare complete guidate dall'autopilota, due per fase (`--races-per-phase`);
2. esportazione e verifica delle tracce grezze, e validazione offline;
3. installazione di report e tracce in `data/immersive_relay/scintilla/qualification_v1/`
   (cartella ignorata da git; un'installazione precedente viene spostata da parte);
4. preflight nativo in una gara nuova.

```bash
python3 tools/immersive/scintilla/qualify_mission_autopilot.py --build-dir <build/relay> \
  --assets-dir <stk-assets> --runtime-dir <nuova cartella sul SSD> --players 3
```

**L'impronta della qualifica include binario STK, asset, shader, camera,
proiezione, risoluzioni, P/C e giri.** Va rifatta dopo ogni ricompilazione
(compreso il merge con SC5) e per ogni configurazione di sala diversa, per
esempio un altro numero di giocatori. Con 3 giocatori sono 6 giri per gara,
circa 30 minuti in tutto per quattro gare.

Mettere in pratica la procedura ha fatto emergere quattro difetti, tutti
corretti:

- **Crash a ogni gara finita.** A fine gara STK guida il kart umano con un
  `EndController`, che continua a dichiararsi controller del giocatore. Quando
  il relay ferma il mondo per i risultati lo convertiva con `static_cast` in
  `PlayerController`, e STK crashava. Nessuna gara era mai arrivata al
  traguardo prima di questa prova.
- **Angolo delle facce del cubo.** Misura e validatore esigevano facce da 90°,
  la produzione ne usa da 120° sovrapposte: la misura non poteva partire.
- **Collegamento simbolico dei dati.** Ogni build macOS collega `data/` dentro
  il bundle, e il lettore della qualifica rifiutava qualsiasi collegamento
  nel percorso: la qualifica non sarebbe mai stata letta.
- **Percorso delle tracce.** Il lettore univa cartella e nome della traccia
  senza separatore (`…/qualification_v1shell.ndjson`).

### Esito della prima qualifica: SC4 non si qualifica

Due gare complete (408 e 412 s, arrivo naturale, tracce da 25 MB ciascuna) e
validazione offline: **non qualificato** (`activation_not_proven`,
`capsule_window_below_base_speed`, `tether_window_below_base_speed`). Nessuna
finestra valida per nessuna capsula e per nessun lato del cavo. Le cause sono
geometriche e di regola, non di misura:

- **Capsule dietro l'HUD.** Gli ancoraggi stanno 4 m sopra la carreggiata. Con
  la camera inclinata di 35° la pista davanti cade nella metà bassa della
  cupola (v≈0,6–0,75), occupata dalla griglia delle icone dell'equipaggio,
  dalle barre e dai nodi di difesa. Nei tratti utili, su circa 7.800 casi la
  sagoma è libera e intera solo 30 volte. È libera quando la capsula passa ai
  lati (u≈0,2 o 0,8).
- **Cavo sempre dietro l'HUD.** Il modulo agganciato (offset fisso nel codice
  ±3 m, 5,5 m in alto, 6 m avanti) cade a u≈0,40/0,60, v≈0,50 e tocca il
  bordo superiore della griglia di icone nel 99,6% dei fotogrammi. Anche in
  gioco sarebbe quasi impossibile da recuperare. Con offset (±8, 8,5, 4) m
  sarebbe libero dall'HUD per 10–30 s per giro.
- **Velocità piena.** Ogni finestra deve restare tutta alla velocità massima
  nativa (15 m/s su neve) per 2 s (capsule) o 3 s (cavo). Anche col cavo
  spostato, in alcuni giri l'autopilota tiene la velocità piena al massimo
  2,4 s di fila.

Così com'era, **la missione SC4 non poteva essere abilitata in sala**.

### Correzione scelta

Valutate tre strade (spostare capsule e cavo, spostare l'HUD, bypass solo
per la simulazione), con la tolleranza di velocità al 95% decisa:

- **Velocità al 95%.** Una finestra vale se il kart resta almeno al 95% della
  velocità base nativa (`QualifiedSpeedShare`); in gioco una capsula è
  ritirata solo sopra limite/0,95. Questa sola regola porta il cavo da 2,4 s a
  9,7 s nel giro peggiore: era la richiesta della velocità esatta a strozzare.
- **Cavo:** 6 m di lato, 7 m in alto, 4 m avanti al kart, costanti condivise.
- **Capsule:** `place_mission_anchors.py` usa le traiettorie della camera
  misurate e cerca, per ogni gruppo, nodo, scostamento e altezza in cui la
  sagoma resta intera, a distanza utile e fuori dall'HUD v3 per più di 2,5 s
  dall'attivazione nel giro peggiore (migliore delle gare registrate per
  giro, come fa il validatore). Con l'HUD attuale le capsule finiscono
  **16–40 m ai lati e 6–12 m in alto**, con 2,7–5,9 s di finestra geometrica.
  *Superata:* la geometria non vede il terreno, e quasi tutte queste capsule
  finivano dietro pendii e alberi (vedi la sezione seguente).
- **Due gare per fase.** Un rallentamento casuale in un giro di una gara non
  deve far fallire tutto: il validatore tiene per ogni giro la gara migliore.

### Seconda qualifica: prova viva troppo corta e capsule dietro il terreno

Dopo la correzione delle prestazioni (sopra), la qualifica con questi
ancoraggi non dava ancora **nessuna** finestra per le capsule. Le cause erano
due, e nessuna geometrica.

**1. La prova di visibilità scadeva prima di essere sostituita.** Le query di
occlusione si campionano a 30 Hz e sul Mac si leggono circa 28 ms dopo la
cattura. Una prova restava valida 50 ms, ma quella in mano aveva fino a
33 + 28 = 61 ms prima che arrivasse la successiva. La catena continua più
lunga durava 0,3 s, mentre ne servono 2. Lo stesso limite valeva in gioco:
all'attraversamento d'attivazione circa **una capsula su tre** sarebbe stata
saltata anche se perfettamente visibile.

Inoltre il validatore provava l'attivazione solo con il campione precedente,
che a quel punto è quasi sempre ancora in volo: provata nel 15% dei casi.

Correzione (validatore Python, lettore nativo e gioco insieme):

- `LiveProofMaxAgeUs` = 100 ms, cioè un gap di cattura (50 ms) più la
  lettura delle query (fino a 50 ms);
- l'attivazione si prova con la prova che il gioco ha davvero
  all'attraversamento, cioè l'ultima già letta, che deve mostrare la capsula
  ed essere ancora viva. Lettore e validatore tengono una breve storia delle
  prove per capsula.

**2. Le capsule finivano dietro il terreno.** La geometria (portata,
proiezione, HUD) non vede pendii, pareti e alberi. Nelle tracce: g0, g1, g2 e
una capsula di g4 erano sempre coperte (visibilità 0%).

Per vedere il terreno serve il rendering vero. Con
`STK_RELAY_SCINTILLA_PROBES` una registrazione misura **posizioni candidate**
invece degli ancoraggi: quattro per campione, a rotazione, con le stesse
query, maschere e profondità della qualifica. Le sonde sono scritte come
`group:-2`, che lettore e validatore rifiutano, quindi una traccia di
esplorazione non può mai qualificare.

`tools/immersive/scintilla/probe_mission_anchors.py` ha tre comandi:

1. `candidates`: genera i centri che la geometria ammette;
2. `record`: registra gare con l'autopilota che li misurano;
3. `place`: sceglie le finestre dall'attraversamento, se la capsula era
   vista appena prima, finché tengono geometria, velocità e ogni sonda
   vicina. Poi tiene il maggior numero di gruppi non sovrapposti, con la
   migliore finestra peggiore.

Risultato su snowtuxpeak, tre gare di sonde (circa 145.000 misure):

- **la prima metà del giro corre fra pareti e bosco:** dal 70 all'84% delle
  posizioni laterali basse è coperto. Anche le posizioni alte (20–36 m) lo
  sono per più della metà;
- **posizioni valide** esistono solo da circa 0,37 del giro in poi;
- **ne escono 4 gruppi** non sovrapposti, il minimo per la qualifica.

Ogni proposta è stata poi verificata con una qualifica nativa. Tre
correzioni sono venute da lì:

- le finestre non devono superare la scadenza del gruppo (una coppia durava
  21 m, cioè 1,4 s);
- la sagoma deve stare lontana dai bordi dell'HUD (raggio gonfiato di 1,5);
- all'attivazione la capsula deve essere già a portata.

Il gruppo 1 è stato rifinito sulle tracce native: attivazione spostata dove
entrambe le capsule sono provate in ogni giro, scadenza subito prima del
gruppo 2.

| Gruppo | Attivazione | Scadenza | Capsule (lato × altezza, m) |
|---|---|---|---|
| 1 | 0,384 | 0,511 | −30 × 30, −18 × 25 |
| 2 | 0,512 | 0,594 | +24 × 3, +24 × 9 |
| 3 | 0,635 | 0,738 | +30 × 12, +24 × 16 |
| 4 | 0,855 | 0,955 | +30 × 25, +30 × 30 |

**Esito: SC4 qualificata** su questa build. Qualifica con l'autopilota, P=3,
6 giri, 3 gare per fase:

- `qualified_geometry` senza motivi residui (73.780 fotogrammi);
- preflight nativo `qualified=True`;
- prove in `qualification_v1/`.

Le gare prima di questa hanno insegnato due cose per la sala:

- **non far girare altro di pesante sul Mac durante una qualifica:** gli
  scatti oltre 50 ms spezzano le finestre di un giro intero;
- **non lasciare che i display si spengano:** con i display in stop STK si
  blocca alla schermata di caricamento.

**Gara con equipaggio simulato e missione** (`crew_simulator.py --mission`,
che ora esegue il preflight come l'host prima di preparare SC1–SC3).
Snowtuxpeak, 408 s, arrivo naturale:

| Metrica | Valore |
|---|---|
| Capsule offerte | 34 su 48 possibili |
| Capsule aperte | 17 (17 colpi su 17, tutti oltre 20 m, raggio angolare 3,0°) |
| Capsule prese con l'attrezzo e raccolte | 2 |
| Esito | gara vinta, 2 slot di carico su 3 |
| Tempo dell'equipaggio sulle capsule | 22% |

Le capsule mancanti sono saltate per regola: all'attraversamento il kart era
in salto o sopra la velocità qualificata (in boost il 24% del tempo).

Per il bilanciamento in sala:

- le capsule vivono 2,5–8 s e sono piccole, 3° di raggio angolare;
- aprirle è alla portata, raccoglierle con l'attrezzo nel tempo che resta è
  difficile;
- le leve sono nel profilo di tuning: ritiro sopra la velocità qualificata,
  marker più grandi, durata delle prese.

Spostare l'HUD darebbe un gioco migliore: senza griglia di icone e barre
sopra la strada le capsule possono stare **4–9 m accanto alla carreggiata, 3–7 m
in alto**, con 3,0–5,5 s di finestra, e i rivali davanti smetterebbero di
finire dietro le icone (`non_targetable_surface`). È però un cambio del layout
di SC2–SC5 a una settimana dai test: proposto come passo successivo.

**SC5 Party** chiede finestre a velocità base + 1 m/s. Su neve il kart è
limitato a 15 m/s, quindi Party non può qualificarsi su questa pista senza
un cambio di regola (per esempio misurare la durata a parità di distanza).

## 5. Modifiche al gioco (punti 5–9)

Tutte attivabili da profilo, con i valori qualificati come default:

- **Colpo sul reticolo disegnato** (`input.hitTest=drawn_reticle`): il colpo si
  valuta dove il reticolo era disegnato nel fotogramma che il giocatore vedeva,
  non sulla posa al momento del grilletto.
- **Tolleranza del fascio** (`input.beamGraceS`): una posa tardiva non disarma
  subito; nella tolleranza la mira viene scartata e annotata (`aim_dropped`).
- **Fuoco automatico** e **cadenza silenziosa** (`autoFireIntervalS`,
  `fireRateFeedback`).
- **Difesa:** proiettile relativo, volo minimo, politica del turbo
  (annulla/pausa/ignora), riprova tecnica.
- **Marker più grandi e capsule:** raggio e portata del marker; ritiro delle
  capsule sopra la velocità qualificata e gruppi spenti.

## 6. Robustezza trovata con il simulatore

Le prime gare simulate morivano dopo 140–150 s con `lease_or_clock_expired`.
STK continuava a girare regolarmente; una richiesta `/spatial/update` era
rimasta ~0,8 s nella coda del main loop, e nei 0,6 s precedenti nessun rinnovo
era partito per mancanza di campioni d'orologio validi.

- **`GET /spatial/clock` risposto dal thread di rete.** Prima passava dalla coda
  del main loop: il tempo di andata e ritorno includeva un frame di STK, e
  sotto carico superava i 50 ms ammessi per campione. Ne derivavano rinnovi
  saltati e intervalli d'offset larghi fino a 40 ms, che a loro volta facevano
  rifiutare pose come `stale_or_uncertain_pose`. Ora l'orologio è lo stesso
  `steady_clock`, letto subito. Il contratto non cambia: l'intervallo resta un
  limite garantito, solo più stretto. Se Scintilla o il runtime non sono attivi
  la richiesta torna in coda e il router spiega il rifiuto come prima. Vale
  anche per TholusOS, senza modifiche lato host.
- **Niente App Nap per il produttore Syphon.** Con l'uscita Syphon attiva STK
  tiene un'attività `NSActivityUserInitiated | NSActivityLatencyCritical`. La
  finestra di STK di solito è nascosta dietro TholusOS e può girare senza suono:
  macOS ne rallenterebbe i timer e la priorità. Verificato con
  `pmset -g assertions` durante una gara.
- **Telemetria dei blocchi:** `frameMaxMs` in ogni campione e una riga `stall`
  per ogni frame oltre 250 ms, scritta anche dopo la scadenza del grant; il
  report li mostra nella riga «Fluidità STK».
- **Telemetria del pilota:** `trackM` (metri lungo la pista) nei campioni e una
  riga `rescue` per ogni salvataggio, con giro e posizione in pista.

### Disarmo dell'equipaggio mentre la GPU copia la profondità

STK rifiutava ogni prova che nominasse un fotogramma la cui copia di profondità
era ancora in corso sulla GPU (`depth_history_unavailable`). Ogni rifiuto
toglieva la prontezza dello storico e disarmava tutti i controller. Alla
prova successiva l'attivazione ripartiva, e le pressioni con istante
precedente venivano rifiutate (`event_precedes_gameplay_activation`). In gara
simulata è successo circa ogni 4 s, senza alcuna copia saltata per budget.

Ora una copia in corso produce `depth_history_pending`: si rifiuta solo quella
prova, mentre prova precedente, prontezza e controlli armati restano validi e
la prova successiva rinomina il fotogramma. TholusOS gestisce già una prova
rifiutata con motivo, e la ritenta. Una copia davvero mancante continua a
ritirare la prontezza come prima.

Il budget di profondità (192 MB) dà 12 fotogrammi a cubemap 768, 7 a 1024 e
3 a 1536. Una prova copre i fotogrammi mostrati negli ultimi 65 ms, quindi
catturati circa 110–130 ms prima (7–8 fotogrammi a 60 fps). **A cubemap 1536
le prove falliranno sempre; a 1024 si è al limite.** In sala conviene restare a
768, o alzare il budget dopo averlo misurato. La telemetria conta i fotogrammi
senza profondità (`depthSkipped*`).

### Il fascio cadeva per errori GL altrui

Ogni lettura di profondità per la visibilità controllava `glGetError()` dopo
`glReadPixels`, senza prima smaltire gli errori lasciati in sospeso da altro
codice di rendering. Poiché gli errori GL restano finché qualcuno non li legge,
ogni errore altrui faceva fallire la query (`depth_unavailable`). Così ogni
fascio tenuto cadeva entro 30 ms senza tolleranza, o allo scadere della
tolleranza di 0,12 s, e nessuna breccia poteva aprirsi. In 200 s simulati: 243
perdite del fascio per questo motivo, 0 brecce.

Ora la query smaltisce prima gli errori pendenti, contandoli in telemetria
(`visibilityLoss`, sesto valore). Gara successiva: 194 errori altrui smaltiti,
zero perdite per profondità, blocchi del fascio con durata mediana 0,59 s,
**prima breccia aperta e primo stordimento applicato**. Un checker statico
(`check_scintilla_combat.py`) impedisce di reintrodurre il difetto. Lo stesso
controllo serve anche a difesa e missione, che usano la stessa visibilità.

### Autopilota

Il passaggio del controller giocatore riportava lo sterzo verso il centro
prima dell'IA, e l'IA limita la propria variazione per tick alla stessa
velocità: le due correzioni si annullavano e il kart quasi non curvava. Il
risultato erano 14 salvataggi in 150 s e l'ultima posizione. Ora l'IA ritrova lo
sterzo scelto al tick precedente: giro in 60 s, primo posto, nessun salvataggio.

### TholusOS: la sessione cadeva a ogni frame lento di STK

Il pump SI5 di TholusOS (`ImmersiveSTKSpatialInput.swift`,
`ImmersiveSTKSpatialHTTP.swift`) trattava come **risposta non valida** ogni
`/spatial/update` con andata e ritorno oltre 50 ms, e come errore di trasporto
ogni richiesta oltre 0,2 s. In entrambi i casi chiudeva l'intera sessione dei
controller. Una risposta di STK aspetta il frame successivo, quindi basta un
frame lento. In una gara simulata **34 risposte su 3824 hanno superato i 50 ms,
la prima dopo 0,3 s di gara**. Sul Mac di test era carico, ma frame da 40–65 ms
compaiono in tutta la telemetria.

Correzioni nel branch `scintilla/test-week-tooling` del worktree host:

- una risposta lenta resta valida, ma non fornisce il campione d'orologio
  (limiti di 20 ms per le sonde e 50 ms per gli update, come quelli di STK);
- `clockSamples` salta le sonde ritardate, al massimo otto tentativi per tre
  campioni;
- rinnovo ogni 250 ms invece di 500 ms, con tre sonde fresche invece dei
  campioni presi dagli update; se l'host è troppo occupato rinnova al passaggio
  successivo;
- timeout delle richieste spaziali 0,6 s, dentro il grant di 1 s di STK.

**Non compilato né eseguito:** solo `swiftc -parse` e il checker
`scripts/ci/test_scintilla_input_maintenance.py`. Prima dei test serve una
build con `ImmersiveSTKSpatialHTTPTests` e `ImmersiveSTKSpatialInputTests`.

Un blocco dell'intero processo STK oltre circa 0,7 s fa comunque scadere il
grant (1 s dall'ultimo rinnovo, sia in STK sia in SI5): è il limite di
sicurezza voluto. Su questo Mac, con carico 23 e 16 GB di swap, è successo una
volta (1,16 s senza frame, anche l'orologio servito dal thread di rete ha
risposto in 611 ms).

## 7. Risultati delle gare simulate

Snowtuxpeak, 3 IA facili, equipaggio di 2 (apritore e tiratore), 330 s,
fisheye 1536 e cubemap 768, sullo stesso Mac con altre build in corso (carico
14–24, 16 GB di swap). Log: `logs/sim-NN*.log`; telemetria e dump:
`runtime/sim-NN-*/`.

| Gara | Correzioni presenti | Esito | Note |
|---|---|---|---|
| sim-14 | nessuna | grant scaduto a ~150 s | rinnovi senza campioni validi, richiesta ferma 0,8 s in coda |
| sim-15 | clock dal thread di rete, App Nap | grant scaduto a ~185 s | blocco dell'intero processo STK di 1,16 s (sistema) |
| sim-16 | + rinnovo 250 ms | prova rifiutata a ~95 s | difetto del simulatore (`presentation_rewrite`), corretto |
| sim-17 | + simulatore corretto | **330 s completi**, 1 cambio pilota | autopilota lento: 28 salvataggi |
| sim-18 | + autopilota corretto | **330 s completi**, 4 cambi pilota | 0 salvataggi; 74 disarmi per profondità |
| sim-19 | + `depth_history_pending` | **330 s completi**, 4 cambi pilota | disarmi per profondità 24; superato un blocco di 622 ms |
| sim-20 | + riprova del fascio | **330 s completi**, 4 cambi pilota | disarmi per profondità 8; rinnovo massimo 0,45 s |
| sim-21 | `builtin_sc4` | grant scaduto a ~290 s | processo STK fermo 1,4–2,2 s (anche l'orologio dal thread di rete) |
| sim-23 | + bersaglio stabile, motivo di perdita del fascio | **200 s completi**, 2 cambi pilota | 243 perdite del fascio per `depth_unavailable` |
| sim-24 | + errori GL smaltiti | interrotta a ~22 s dal simulatore sospeso 1,1 s | blocchi 0,59 s, **1 breccia, 1 stordimento** |

Cosa dicono, con i limiti di un Mac sovraccarico:

- **Stabilità della sessione:** con le correzioni le gare arrivano in fondo,
  cambi pilota compresi. Le uniche cadute residue vengono da blocchi
  dell'intero processo oltre 1 s, che il limite di sicurezza deve fermare.
- **TholusOS prima della correzione** avrebbe chiuso la sessione in ogni gara:
  la prima risposta oltre 50 ms è arrivata tra 0,1 e 28 s dalla partenza, con
  26–113 risposte lente per gara.
- **Tolleranza del fascio:** con `builtin_sc4` le mire tardive hanno causato
  466 disarmi in una gara; con `test_week_v1` 4 disarmi e 36 mire scartate
  senza disarmo.
- **Combattimento:** fino a sim-23 nessuna breccia, per il difetto GL sopra.
  Dopo la correzione i blocchi reggono e la breccia si apre. Restano esiti da
  osservare con persone vere: `non_targetable_surface` (la pressione cade su una
  zona HUD dove in quel momento si vede il rivale) e `ambiguous_presentation`
  (bersaglio diverso tra due fotogrammi plausibili). L'apritore simulato ora
  resta sul bersaglio scelto e, se il fascio non prende, rilascia e riprova.
- **Movimento dei bersagli:** visti dall'apritore, i rivali attraversano la
  cupola a 44–54°/s di mediana e 130–200°/s al p90, con raggio angolare mediano
  10–18° a marker 0,8 m. Sono i numeri da confrontare con la capacità di
  inseguimento delle persone nella settimana di test.
- **Il modello di equipaggio resta un modello:** questi dati verificano
  sessione, protocollo e regole; non sono una previsione della bravura dei
  giocatori.
- **Capsule:** fino a sim-24 erano a zero, perché la missione SC4 non era
  qualificabile. Ora sono qualificate e giocate dall'equipaggio simulato:
  vedi «Seconda qualifica».
- **Minacce a zero:** richiedono un rivale dietro, tra 8 e 24 m.

## Verifiche

| Verifica | Esito |
|---|---|
| Suite `tools/immersive/tests` (C++, Bullet, Python offline) | **379/379** (sopra SC5, con il caso nuovo della prova viva) |
| Test Python del validatore (`test_mission_qualification.py`) | 40/40 |
| Checker dei sorgenti `check_*.py` e test Python degli strumenti | tutti superati |
| Build Relay incrementale | riuscita |
| TholusOS (`scintilla/test-week-tooling` del worktree host) | **non compilato**: `swiftc -parse` e checker `test_scintilla_input_maintenance.py` superati |

Il report ora mostra anche i blocchi del fascio (avviati, durata, persi entro
0,2 s), le pressioni del fascio/attrezzo, i salvataggi del pilota, i blocchi
del main loop e i fotogrammi senza profondità. Il simulatore riporta inoltre
velocità e raggio angolare dei bersagli, percentuale di tick col reticolo sul
bersaglio e risposte che il vecchio TholusOS avrebbe rifiutato.

## Aperto

- **Una build e un test di TholusOS** quando la macchina è libera:
  `ImmersiveSTKSpatialHTTPTests` e `ImmersiveSTKSpatialInputTests`.
- Unire `scintilla/test-week-sc5` in `main` (fast-forward: il branch parte da
  `main`).
- Spostare griglia di icone e barre dalla fascia sopra la strada (vedi
  «Correzione scelta»), poi rifare l'esplorazione con le sonde: le capsule
  potrebbero tornare vicino alla carreggiata e anche nella prima metà del giro.
- **Sul Mac della sala disattivare lo spegnimento dei display** (o tenere
  attivo `caffeinate -d` durante lo spettacolo): con i display spenti STK si
  blocca alla schermata di caricamento, durante la compilazione degli shader.
  Visto alle 3 di notte, a qualifica avviata con lo schermo in stop.
- Rifare la qualifica SC4 sul Mac della sala, con la build e la configurazione
  definitive (l'impronta comprende binario, shader, ancoraggi e risoluzione).
- Decidere la regola di velocità di SC5 Party su neve.
- Verificare con persone vere `non_targetable_surface` (rivali dietro zone HUD)
  e `ambiguous_presentation`.
- Calibrare `test_week_v1` sui dati reali: prima sessione con la sonda di
  latenza, poi gare con il profilo e il report.
