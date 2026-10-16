# Zone Party SC5: authoring, prove native e installazione

Il manifest distribuito contiene **solo sei candidati e zero intervalli operativi**. Non abilita Party. `validate_party_zones.py` può controllarne lo schema senza compilare o avviare STK:

```sh
python3 -B tools/immersive/scintilla/validate_party_zones.py --manifest data/immersive_relay/scintilla/snowtuxpeak_party_zones_v1.json
```

L'uscita `qualified:false` è intenzionale. Il profilo standard SC5 non richiede zone Party; richiede comunque la propria qualifica geometrica v3. Non trasformare i candidati 6,17,26,45,59,71 in intervalli ammessi sulla base della sola curvatura.

## 1. Congelare gli intervalli prima delle catture

Questa procedura richiede una futura build e prove native autorizzate. Il solo incarico di implementazione senza build non la esegue.

1. Usare `snowtuxpeak` normale, tux, easy, tre AI, adaptive e crew telefonica legacy disabilitati, P2…6. Registrare executable SHA256, fingerprint degli asset del recorder SC5 e FPS fisici effettivi.
2. Leggere estremi, distanze e direzione dal **DriveGraph nativo** della stessa pista. Ispezionare l'intero volume/corridoio reale, non soltanto i centri dei quad. Verificare larghezza, collisioni, rampe, salti e tratto finale.
3. Definire al massimo sei intervalli disgiunti, non avvolti sullo start; larghezza minima6m. La distanza finale esclusa deve contenere l'intero orizzonte di guardia. Il loader confronta gli estremi con i nodi nativi entro1mm.
4. Portare `authoringStatus` a `authored`, con autore e licenza, **prima** di generare le prove. Calcolare SHA256 dei byte finali del manifest. Non modificarlo dopo la cattura: cambierebbero sia questo hash sia il fingerprint degli asset.

`authored` significa soltanto dati congelati. L'ammissione viene dal report esterno. Non esiste uno stato `native_qualified` da scrivere nel manifest: causerebbe dipendenza circolare fra hash e prova.

Il runtime richiede localizzazione nativa continua, marcia in avanti, kart a terra e non in animazione, nessun boost/minimo di velocità attivo, freno non premuto e riserva energetica disponibile. Ricontrolla anche all'ultimo consenso:

`d_guard = speedBound × (0,5 + 0,25 + marginSeconds) + positionErrorMeters + chassisCircumsphereRadius`.

Il raggio deriva dall'AABB della **forma Bullet effettiva**, inclusi gli offset. Il modello non dimostra aderenza, arresto, comfort o sicurezza fisica. Limiti e incertezza del manifest devono essere sostenuti dalle misure native.

## 2. Evidenza e catture da conservare

Preparare un documento UTF-8 con schema chiuso:

- `schemaVersion`:1; `contract`:`tholus.stk-scintilla-party-zones-evidence/1`;
- `sourceKind`:`native_runtime`; `synthetic`:false;
- `producerUUID` e `operatorId` del ciclo realmente osservato;
- `manifestSHA256`, `trackHash`, `nativeBuildSHA256`, `trackAssetsSHA256`, `physicsFPS`;
- `zones`: esattamente una riga per intervallo del manifest.

Ogni riga contiene soltanto `zoneId`, `geometryClear`, `noRampOrJump`, `observedSpeedBoundMetersPerSecond`, `observedPositionErrorMeters`, `checks`. Geometria e assenza di rampe sono attestazioni dell'ispezione nativa. La velocità testata deve coprire il limite; l'errore osservato deve rientrare nel limite spaziale dichiarato. Non convertire una formula di arresto assunta in una misura.

`checks` contiene esattamente cinque chiavi. Ogni prova contiene `passed`, `nativeRunId`, `artifactSHA256`:

| Chiave | Cosa documentare nel percorso nativo reale |
|---|---|
| `brakeBefore` | Freno oltre soglia prima del commit: nessun effetto, costo, cooldown o quota. |
| `brakeDuring` | Freno durante burst e fade tramite `PlayerController::action`: decelerazione, acceleratore azzerato, nessun floor/impulso aggiuntivo. |
| `brakeAccelOrders` | Entrambi gli ordini PA_BRAKE→PA_ACCEL e PA_ACCEL→PA_BRAKE, con lease Party attiva e senza cambiare la priorità globale vanilla. |
| `analogBrake` | Valori sotto, esattamente e sopra16384; veto soltanto sopra soglia, senza perdere la richiesta fisica mantenuta. |
| `laterNativePad` | Un pad ZIPPER successivo resta attivo; scadenza o rilascio del lease Party non lo cancella. Provare anche pad precedente, nitro, skid e SC3boost come esclusioni. |

Conservare nelle catture tick/FPS, identità native, azioni e ordine effettivo, velocità/cap prima e dopo Bullet, normale e velocità angolare, durata burst/fade, energia/quota e stato della categoria ZIPPER. Includere pendenza, velocità quasi zero, tentativi rifiutati, uscita zona durante consenso e rescue/discontinuità. Verificare il bound tangenziale ≤1m/s entro l'epsilon del piano; il caso tangenziale4/normale3 non deve diventare un impulso di2m/s.

Una cattura può sostenere più controlli, ma ogni controllo deve essere esaminato. Salvare i byte originali come `<artifactSHA256>.proof`, massimo1MiB ciascuno. La qualifica accetta al massimo30 riferimenti distinti. Sono artefatti di evidenza del ciclo nativo, non file generati dal verificatore matematico.

Il tool verifica **schema, provenienza dichiarata e integrità**, non può stabilire che una dichiarazione dell'operatore sia vera. Il loader nativo ricontrolla gli stessi dati e i byte delle catture, poi i dati reali di pista/build/asset/FPS. Una dichiarazione `sourceKind:native_runtime` falsa non diventa una prova reale. I test di formato usano fixture sintetiche isolate e non installano qualifiche.

## 3. Validare e installare la qualifica

Fornire al tool il manifest congelato, il documento di evidenza, la cartella delle catture e le identità **attese dalla stessa build nativa**. Leggere gli argomenti con `--help`. `--output-dir` deve essere la cartella dati `immersive_relay/scintilla/qualification_v2` della build da qualificare, su SSD montato e scrivibile.

Il tool:

1. rifiuta manifest vuoti/non authored, source offline/sintetiche, campi ignoti/duplicati, set incompleti, FPS/hash diversi, misure insufficienti o catture modificate;
2. copia i byte delle catture in `party_zone_proofs/` e l'evidenza esatta in `party_zones_evidence_v1.json`;
3. scrive per ultimo `party_zones_v1.json`, che lega manifest, build, asset, evidenza e tutte le zone.

Le tre destinazioni sono nella cartella `qualification_v2`, esclusa dal fingerprint degli asset per evitare autoriferimenti. Tutti gli output controllabili rimangono sul SSD; nessun file di qualifica viene installato dalla modalità di sola analisi del manifest.

Ripetere il preflight held SC5 dal flusso dell'app. Un report di zone non sostituisce il report geometrico v3 di `QUALIFICA_SC5.md`. Un cambio di manifest/build/asset/FPS richiede nuove prove coerenti. Errori lasciano Party indisponibile e la gara held, senza producer restart o consumo di energia/quota.

## 4. Stato della consegna senza build

Sono presenti parser nativo, controllo delle catture, tool, regressioni e dati authoring. Non sono state eseguite ispezioni native/Bullet, catture di freno, qualifica delle zone o prove in sala. Nessun intervallo è stato promosso a operativo e nessuna evidenza nativa è stata inventata. Comfort, leggibilità e identità del portatore richiedono accettazione hardware/umana distinta. SC6 resta fuori da questa procedura di implementazione.
