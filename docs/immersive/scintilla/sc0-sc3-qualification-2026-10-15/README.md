> **Verifica VSync del 16 ottobre:** build vanilla/Relay, 60 casi SDL e smoke
> SC0–SC3 con VSync effettivamente attivo passati. Il blocco originale con
> display inattivo resta non riprodotto e aperto: [risultati e limiti](../vsync-2026-10-16/README.md).

> **Aggiornamento del 16 ottobre:** il blocco macOS è superato per i test host.
> Il [checkpoint successivo](../sc0-sc3-host-tests-2026-10-16/README.md) registra
> quattro correzioni, il profilo corretto e 182/182 metodi passati in 11 suite.
> Gli esiti sotto restano il resoconto storico del 15 ottobre.

# SC0–SC3 — build e qualifica software locale

**Esito:** build STK Relay, STK vanilla e TholusOS riuscite; controlli nativi,
package Swift, matematici e GPU disponibili superati dopo le correzioni.
**La suite ospitata nell’app TholusOS è compilata ma non eseguita:** macOS
attende il consenso per accedere al volume rimovibile. Nessuna asserzione di
quella suite può essere contata come superata. SC4 non è stata avviata.

Questo checkpoint aggiorna l’evidenza dei documenti precedenti, che restano
storici e descrivevano consegne senza build. Non certifica la qualifica fisica
o la completa accettazione runtime del piano SC3.

## Ambiente e riproducibilità

Mac locale Apple M1 Pro, 16 GB, arm64; macOS 26.5.2, Xcode 26.6, Swift 6.3.3,
Apple clang 21, OpenGL **4.1 Metal - 90.5**. Non è il Mac mini della venue.
I checkout principali contenevano WIP: la verifica ha usato copie isolate con
lo stesso contenuto iniziale, poi consegna limitata ai file del manifest.

- Repository verificato STK: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/stk`.
- Repository verificato host: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/host`.
- Evidenze e tentativi precedenti conservati: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence`.
- [Esiti, macchina e binari](verification.json); [hash prima/dopo per i file modificati](source-manifest.json).
- Baseline completa e inventario iniziale: `stk-baseline.json` e `host-baseline.json` nella radice del task. Nessun commit o push.

## Controlli eseguiti

| Controllo | Esito | Evidenza nella cartella `evidence` |
|---|---|---|
| CTest C++ Release arm64 | **152/152** | `native-ctest-04.log`, `.xml` |
| Consumer SC1/SC3 | 80 casi, inclusi sessione prolungata, orologi, storico, riordino e coalescenza | stessa suite |
| Core SC2 / core SC3 | 25 / 24 casi | stessa suite |
| Bullet incluso effettivo | 10 casi | stessa suite |
| Trasporto HTTP / dispatcher | 7 / 6 casi | stessa suite |
| Controlli Python Scintilla | **50/50** | `scintilla-source-and-math-04.log` |
| Scanner proiezione | **5/5** | `projection-source-02.log` |
| Calcoli del piano SC3 | **10 gruppi** | `sc3-plan-math-01.log` |
| GLSL depth su GPU reale | **64/64**; originale rifiutato dal compilatore come atteso | `scintilla-depth-gpu-03.log`, `scintilla-depth-gpu-original-negative.log` |
| Build completa STK Relay e vanilla | **riuscite** | `relay-build-depth-fixed-05.log`, `vanilla-build-01.log` |
| Vanilla con mondo reale | **riuscito**, profilo automatico 5 s | `vanilla-runtime-02.log` |
| Lifecycle Relay, tre piste reali | **riuscito**: snowtuxpeak, lighthouse, olivermath | `runtime/equirectangular-02/equirectangular-legacy-lifecycle.log` |
| Equirettangolare 2048×1024 + Syphon | **riuscito**, 106 frame, 54 campioni variabili | `equirectangular-runtime-10.log` |
| Domemaster 1536×1536 + Syphon | **riuscito**, 100 frame, 51 campioni variabili | `fisheye_domemaster-runtime-01.log` |
| Swift TholusSpatialInput | **188/188**, 21 suite | `host-spatial-input-04/test.log` |
| Swift TholusAccessories | **72/72**, 7 suite | `host-accessories-01/test.log` |
| Runner Python host | **29/29** | `host-runner-python-01/test.log` |
| Build app Debug locale senza firma | **riuscita** | `host-app-build-02/build-result.json` |
| Build dei test app | **riuscita** | `host-scintilla-03/build-for-testing.log` |
| Esecuzione test app | **bloccata dal sistema operativo; 0 asserzioni eseguite** | `host-qualification-summary.json` |

Il ricevitore Syphon esegue readback e analisi CPU: i suoi FPS non sono una
misura della prestazione del proiettore. `orientation_grid=pass` nel probe con
griglia disattivata non è una prova di orientamento fisico.

## Problemi corretti e ricontrollo

1. **Perdita del contesto SC3 durante attesa GPU/coalescenza.** Una mira HUD
   poteva essere sostituita dal successivo campione world senza rendere
   persistente l’interruzione. Il consumer conserva subito la partenza da
   contesto/settore della pressione, anche se la pressione affidabile attende
   ancora la GPU. Questa informazione può soltanto invalidare; non concede
   una prova positiva anticipata. Regressioni con vecchia mira pendente e
   pressione iniziale pendente; **80/80 consumer, 152/152 suite completa**.
2. **Fixture native non compatibili con i contratti temporali effettivi.**
   Corretti storia/prova della sessione da 6000 edge, separazione fra clock
   dell’input e osservazione fence, frontiera conservativa della presentazione
   e margine degli eventi dopo rebind. I limiti di produzione e le aspettative
   sono conservati. I sette fallimenti iniziali restano nel log `native-ctest-01.log`.
3. **Inizializzatori Swift isolati in valori di default.** Le due costruzioni
   sono ora nei corpi MainActor; combat e defense condividono il medesimo
   trasporto. Nessuna soppressione dei controlli di isolamento. Build app e
   build test riuscite dopo la modifica.
4. **Espansione di sei macro Swift Testing annidate.** Separati unwrap del body
   e decode/unwrap JSON, conservando tutte le aspettative; build test riuscita.
5. **Due fixture native Swift dipendenti dallo scheduling reale.** Clock interno
   iniettabile e timeline deterministica; API pubblica e clock reale predefinito
   conservati. **6/6 mirati e 188/188 nel package completo**.
6. **Profilo di test app fermo a SC1.** Il runner richiede ora le dieci suite
   pertinenti SC0–SC3: 180 metodi più varianti parametrizzate, compresi casi
   handoff/reset ammessi e rifiutati. I 29 test del runner verificano anche
   copertura incompleta/varianti mancanti; non si converte uno skip in verde.
7. **GLSL 4.10: `const` inizializzato da parametri runtime.** Era una vera
   mancata compilazione e produceva Syphon nero. Rimossi tre qualificatori
   senza cambiare l’aritmetica conservativa. Il nuovo test nativo compila
   l’esatta funzione di produzione: **originale fallisce, correzione passa
   64 casi**. Entrambi i compositori reali ora ricevono immagini variabili.
   Riferimento: [specifica GLSL 4.10](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.10.pdf).
8. **Copia depth GPU con formati differenti.** Le RTT sorgente erano D24S8,
   il cubemap destinazione D24; il driver restituiva `GL_INVALID_OPERATION`
   e lo storico depth rimaneva vuoto. Destinazione/attachment ora D24S8 come
   la sorgente; un errore di copia viene propagato. Diagnostica temporanea
   rimossa. In entrambe le proiezioni, l’ID letto dai pixel ricevuti coincide
   con metadata e una copia depth con fence completata, osservata mentre il
   frame appartiene ancora allo storico limitato. Riferimento sul formato:
   [glBlitFramebuffer](https://wikis.khronos.org/opengl/GlBlitFramebuffer).
9. **Retrovisore capovolto.** SC3 campionava il cubemap grezzo, mentre il main
   usa le RTT con la loro base e FOV. Ora usa la stessa selezione di campionamento
   del main. Confronto su **153 raggi dell’immagine GPU equirettangolare**:
   errore mediano RGB medio 94 livelli prima, 0,33 dopo; percentile 90 da
   151,33 a 6. Verificata anche l’immagine domemaster.
10. **Scanner Ready obsoleto.** Aggiornato alla semantica corretta: Ready nei
    risultati resetta senza avviare; ogni vero avvio passa dai gate. **5/5**.

Il prerequisito Chroma del build host aveva ricevuta/lock non allineati al
checkout corrente. Riconciliato con i comandi supportati: **nessuna modifica
al repository Chroma, bundle runtime byte-identico**; cambiano solo lock e
ricevuta host. Dettagli in `chroma-prerequisite/mutations.json`.

Anche il driver di qualifica è stato corretto: non confronta un frame scaduto
con lo storico depth corrente dopo la compressione PNG; osserva lo storico
contemporaneamente al ricevitore. Scarta prima dell’invio misure NTP che
superano il limite reale di 50 ms e attende l’avvio effettivo del cambio pilota.
Queste correzioni di fixture non allargano i criteri del producer.

## Cosa provano i due smoke SC3

Con grant e dichiarazione di visibilità **esplicitamente sintetici**, il gioco
reale prova enable → intent → prepare SC1/SC2/SC3 → start → pausa/ripresa →
cambio pilota (conferma operatore e Ready telefono, nei due run) → rebind logico
con lo stesso grant → ripreparazione → risultati → Ready reset-only →
ripreparazione → nuova gara → reset esplicito → rebind/ripreparazione.
Sono verificati start prematuro rifiutato, rebind idempotente ed epoch crescenti.
Una prova futura volutamente invalida è rifiutata e chiude il grant.

Il test non invia **alcuna prova ottica positiva inventata**. La lettura THB1
controlla entrambe le righe alpha, CRC32, stream e frameId nei pixel reali Syphon;
non afferma che il frame sia stato mostrato da un proiettore. I controlli positivi
di tiro, fascio, minaccia/difesa/boost restano test di core/Bullet e aritmetica GPU,
non un flusso fisico end-to-end con sorgenti calibrate o readback PBO di un colpo reale.

[Immagine reale equirettangolare](</Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence/runtime/equirectangular-10/equirectangular-sc3.png>)
· [Immagine reale domemaster](</Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence/runtime/fisheye_domemaster-01/fisheye_domemaster-sc3.png>).

## Limiti ancora aperti

- **Test ospitati in TholusOS:** due tentativi, ad hoc e con identità di sviluppo
  stabile, compilano ma non raggiungono handshake/assertion. Sample e log macOS
  identificano `kTCCServiceSystemPolicyRemovableVolumes`, non un fallimento dei
  test. Serve il consenso umano macOS all’app di test **TholusOS** per SanDisk;
  poi rilanciare il runner esistente. Nessun bypass, copia su disco interno,
  modifica TCC o firma della produzione. Processi dei tentativi fermati.
- **VSync locale:** con l’attuale SDL2-compat/SDL3, VSync=1 resta in attesa
  DisplayLink durante lo swap. I run riusciti usano esclusivamente config isolate
  con `swap-interval-vsync="0"`; nessun default del prodotto modificato.
  VSync=1 e prestazioni/latency di venue non qualificati.
- **Hardware/venue:** servono calibrazioni e tracking fisici, latenza ottica
  misurata, output/proiezione visibile, audio, sequenza positiva reale di
  minaccia/difesa/boost, comfort e bilanciamento. La vista rear resta read-only.
- **Storage nativo non configurabile:** STK usa la directory Addons macOS
  preesistente e ha riscritto i metadata di `addons_installed.xml` (stessa
  dimensione; contenuto iniziale non acquisito). Config/cache/output controllabili
  sono sul SSD. Nessuno spostamento o pulizia globale dei dati Addons.

## Comandi principali

Dalla radice del worktree STK, con output sul SSD:

```sh
cmake -S tools/immersive/tests -B "/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/build/native" \
  -DENABLE_RELAY_TEST_TOOLS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build "/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/build/native" --parallel 6
ctest --test-dir "/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/build/native" --parallel 4 --output-on-failure
python3 -B -m unittest discover -s tools/immersive/tests -p 'check_scintilla_*.py' -v
python3 -B tools/immersive/CheckRelayProjectionSource.py
python3 -B docs/immersive/scintilla/sc3-plan-2026-10-15/verifica_calcoli.py
python3 -B tools/immersive/tests/run_scintilla_depth_gpu.py \
  --output-dir "/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/build/depth-gpu-NUOVO"
```

I comandi/sorgenti dei due smoke e la strumentazione minima del ricevitore sono
conservati in `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/tools` e `evidence/runtime-probe`. La variante aggiunge
soltanto il dump RGBA grezzo per leggere THB1, senza cambiare i criteri del probe.
`host-qualification-summary.json` contiene comandi, ambiente e log dei runner host.
Per riprendere i test app **dopo il consenso macOS**, nello stesso worktree host:

```sh
THOLUS_BUILD_CACHE_ROOT="/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/cache/host" \
THOLUS_COMPILATION_CACHE_ROOT="/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/.build-cache/BuildCache/CAS" \
CHROMA_SOURCE_ROOT="/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Tholus_Chroma" \
THOLUS_RUNTIME_TEST_PROFILE=scintilla \
THOLUS_TEST_DEVELOPMENT_IDENTITY=A4550167ABC2598A6E03A7153F73CB0CD0CC65E2 \
bash scripts/ci/test-app-isolated.sh "/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence/host-scintilla-NUOVO"
```

Worktree, build ed evidenze sono conservati per riprodurre i risultati e riprendere
la suite bloccata. Nessun processo di verifica lasciato attivo; rimossa soltanto
la strumentazione diagnostica temporanea dai sorgenti consegnati.
