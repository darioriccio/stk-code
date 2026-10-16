# Strumenti per la settimana di test, 16 ottobre 2026

**Scopo:** arrivare ai test con l'hardware avendo già verificato tutto ciò che
si può verificare via software, e poter cambiare il bilanciamento in pochi
minuti senza ricompilare. **Nessuna prova con puntatori VIVE, ottica del dome o
pubblico:** i numeri qui sotto vengono da gare simulate su un Mac molto carico.

Branch `scintilla/test-week-tooling` del worktree
`/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/test-fixes-2026-10-16/stk-code`,
sopra `scintilla/sc4-test-fixes`. I checkout originali non sono stati toccati.
SC5 non è incluso: è in lavorazione in un'altra sessione.

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
- **Capsule e minacce a zero:** il simulatore non prepara la missione SC4, che
  richiede gli ancoraggi qualificati, e le minacce richiedono un rivale dietro
  tra 8 e 24 m.

## Verifiche

| Verifica | Esito |
|---|---|
| Suite `tools/immersive/tests` (C++, Bullet, Python offline) | **274/274** |
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
- Unire in `main` dopo la consegna di SC5. `scintilla_input.cpp` e
  `scintilla_engine.cpp` sono toccati da entrambi i lavori: conflitti probabili.
- Verificare con persone vere `non_targetable_surface` (rivali dietro zone HUD)
  e `ambiguous_presentation`.
- Qualifica SC4 delle capsule (visibilità misurata a velocità di gara): si può
  fare con l'autopilota, senza VIVE.
- Calibrare `test_week_v1` sui dati reali: prima sessione con la sonda di
  latenza, poi gare con il profilo e il report.
