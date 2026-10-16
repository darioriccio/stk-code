> **Verifica VSync del 16 ottobre:** build vanilla/Relay, 60 casi SDL e smoke
> SC0–SC3 con VSync effettivamente attivo passati. Il blocco originale con
> display inattivo resta non riprodotto e aperto: [risultati e limiti](../vsync-2026-10-16/README.md).

# SC0–SC3 — completamento dei test host, 16 ottobre 2026

**Esito: 182/182 metodi di test passati in 11 suite, zero fallimenti e zero skip.**
Le varianti parametrizzate sono incluse e controllate dal runner. Il test host
TholusOS arm64 è stato ricompilato e avviato realmente: il precedente blocco
macOS sull’accesso al volume rimovibile non impedisce più questa esecuzione.
SC4 non è stata avviata.

Questo checkpoint completa la parte host rimasta aperta nel
[rapporto del 15 ottobre](../sc0-sc3-qualification-2026-10-15/README.md).
I test C++/Bullet, package Swift, GPU e Syphon precedenti restano evidenze
separate: i sorgenti STK verificati allora sono invariati (hash ricontrollati),
quindi non sono stati ricompilati o rieseguiti per queste sole correzioni host.

## Fallimenti riprodotti, correzioni e ricontrollo

1. **Frazioni scambiate per ID interi.** Il confronto fra `NSDecimalNumber` e
   `NSNumber` poteva arrotondare `1.000000000000000001` a `1`. Il decoder dello
   storico mantiene entrambi gli operandi decimali per il confronto esatto;
   resta il limite degli interi sicuri e l’ordinamento positivo. Test esteso a
   entrambi gli array, frazioni vicine al massimo, interi esatti e JSON realmente
   decodificato da Foundation. Il caso inizialmente fallisce nel run 04 e passa
   già nel run 05, prima delle altre correzioni; ripassa nella suite finale.
2. **Fixture della proiezione non esplicita.** Il test delle dimensioni presumeva
   domemaster mentre la scelta automatica può richiedere una sorgente
   equirettangolare in assenza di una superficie calibrata. Il caso seleziona
   esplicitamente domemaster e verifica anche il passaggio equirettangolare
   e ritorno: target 2048×2048 e 4096×2048. Le aspettative originali restano;
   il comportamento di produzione della proiezione non è stato cambiato.
3. **Avviso video che nasconde l’errore API.** L’aggiornamento diagnostico ora
   valorizza l’errore solo se non esiste già una causa API/gameplay. Stato,
   dimensioni ed età del video continuano ad aggiornarsi. Il test usa un
   ricevitore inerte con avviso deterministico: errori offline/HTTP conservati,
   video ancora aggiornato, senza publisher, Metal o acquisizione dell’output.
4. **Suite SC2 assegnata al nome del file anziché al proprietario compilato.**
   Lo stesso file dichiara due suite: i 19 casi `ImmersiveSTKCombatHTTPTests`
   erano attribuiti a `ImmersiveSTKSpatialHTTPTests` e non venivano selezionati.
   Ora entrambi i proprietari sono espliciti. Recuperati anche due metodi con
   attributi Test su più righe e controllate le tre varianti di ciascun profilo
   di proiezione. Tutti i 182 metodi obbligatori devono esistere nell’elenco
   abilitato di Xcode prima dell’esecuzione; dopo, ogni caso deve risultare
   passato. I test Python distinguono le suite nello stesso file e gli attributi
   multilinea. **29/29** passati; il confronto con l’enumerazione reale
   rifiuta la vecchia mappa e ammette quella corretta.

Il run 04 esegueva 163 metodi nelle dieci suite selezionate: 160 passati e tre
falliti. Il run 05 verifica la prima correzione: 161 passati e due falliti.
Solo il run 06 costituisce il pass completo del profilo corretto.

## Copertura finale

Le colonne distinguono i metodi obbligatori dai casi effettivi conteggiati
nei risultati, comprese le varianti parametrizzate.

| Suite | Metodi obbligatori | Casi passati verificati dal runner |
|---|---:|---:|
| `ImmersiveSTKSpatialHTTPTests` | 10 | 12 |
| `ImmersiveSTKCombatHTTPTests` | 19 | 77 |
| `ImmersiveSTKSpatialActivationTests` | 18 | 69 |
| `ImmersiveSTKSpatialInputTests` | 17 | 37 |
| `ImmersiveSTKOpticalPresentationTests` | 13 | 22 |
| `ImmersiveSTKSpatialHistoryTests` | 11 | 11 |
| `ImmersiveSTKDefenseHTTPTests` | 16 | 53 |
| `ImmersiveSTKPresentationTests` | 9 | 9 |
| `ImmersiveSTKSurfaceRoutingTests` | 10 | 10 |
| `ImmersiveSTKSurfaceRuntimeTests` | 6 | 10 |
| `ImmersiveSuperTuxKartIntegrationTests` | 53 | 70 |

**Build per i test riuscita**, identità locale isolata e firma di sviluppo
conservate. Il runner conferma che i sorgenti non sono cambiati durante build
ed esecuzione e controlla prerequisito Chroma, bundle e varianti obbligatorie.
Nessun nuovo build dell’artefatto applicativo ordinario o avvio della produzione:
questa esecuzione usa il test host compilato dal target app, con dati isolati.

## Evidenze e riproduzione

- [Esiti strutturati](verification.json) e [hash delle sole modifiche host](source-manifest.json).
- Manifest nativo con comandi completi, firma, hash, selettori e conteggi:
  `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence/host-scintilla-06/manifest.json`.
- Risultati Xcode: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence/host-scintilla-06/test-results.xcresult`.
- Log e summary: nella stessa directory; tutti i tentativi precedenti conservati.
- Verifica della mappa compilata e test Python:
  `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence/host-test-resume-2026-10-16`.

Il runner resta quello del progetto, con profilo `scintilla`, cache host e
DerivedData persistenti sul SSD. Il manifest registra gli argomenti effettivi.
Per ripeterlo, riusare l’ambiente documentato nel checkpoint precedente e
scegliere una nuova directory di evidenza; non sovrascrivere i risultati.

## Limiti di evidenza

Queste sono prove native di codice/contratti e integrazione sintetica ospitate
nell’app. Non sono prove di UI visuale, sorgenti fisiche calibrate, latenza ottica,
proiezione, audio, comfort o bilanciamento. Restano anche i limiti VSync=1 e venue
registrati nel checkpoint precedente. Nessuna prova fisica è stata inventata.

Le correzioni sono consegnate con controllo degli hash iniziali e finali per
preservare il WIP esistente. Nessun commit/push. Processi del task chiusi al termine;
cache, build ed evidenze conservate per la riproduzione.
