# VSync — verifica locale SC0–SC3, 16 ottobre 2026

**Esito:** VSync attivo verificato sulla build precedente e dopo le build
incrementali vanilla/Relay. Il blocco del 15 ottobre **non si è riprodotto** nelle
condizioni attuali. **Nessuna correzione al codice del prodotto:** il caso di
avvio con display inattivo resta aperto. SC4 non avviata.

Il checkpoint precedente descriveva correttamente un tentativo bloccato e smoke
con VSync=0. Questi nuovi risultati aggiungono prove con VSync=1; non dimostrano
che un difetto intermittente sia stato eliminato.

## Prove eseguite

| Prova | Esito |
|---|---|
| Primo test sulla precedente vanilla, senza build né strumentazione | Gara/profilo completato, exit 0, config VSync=1 |
| Primo smoke Relay equirettangolare sulla precedente build | Lifecycle SC0–SC3 e Syphon passati, config VSync=1 |
| Osservazione esterna della precedente vanilla | SDL applica −1, DisplayLink avviati, 632 swap completati; massimo 30,794 ms nello swap |
| Build CMake incrementale vanilla e Relay | Entrambe riuscite; binari byte-identici ai precedenti |
| Probe nativo SDL sulla libreria realmente collegata | **60/60 casi, 1500 swap, 3 monitor, 0 errori/timeout** |
| Smoke Relay domemaster 1536×1536 | Passato; SDL effettivo −1, frame Syphon/THB1 e storico depth coerenti |
| Smoke Relay equirettangolare 2048×1024 dopo build | Passato; SDL effettivo −1, frame Syphon/THB1 e storico depth coerenti |
| CTest nativi SC0–SC3 | **152/152**, 0 fallimenti |

Il probe copre `0 → 1 → −1 → 0 → 1` su ciascun monitor, in finestra visibile,
ridimensionata, minimizzata e ripristinata. Verifica l’intervallo ottenuto, il
monitor effettivo e i flag di minimizzazione/ripristino. Il timeout esterno
termina un eventuale blocco invece di attendere indefinitamente.

Tempi mediani dello swap in finestra visibile (ms):

| Monitor SDL | Refresh | Disattivato | Strict 1 | Adaptive −1 |
|---|---:|---:|---:|---:|
| 0 | 60 Hz | 0.306 | 16.608 | 16.792 |
| 1 | 120 Hz | 0.289 | 8.413 | 8.239 |
| 2 | 60 Hz | 0.392 | 16.584 | 16.695 |

Il codice esistente prova adaptive −1 quando il parametro utente vale 1. Non è
VSync disattivato: [SDL documenta −1 come adaptive VSync e 1 come sincronizzazione
ordinaria](https://wiki.libsdl.org/SDL2/SDL_GL_SetSwapInterval). Qui sono verificati
sia il valore applicato sia l’attesa reale nello swap. I contatori `profile`
del gioco non sono utilizzati come misura della frequenza dei frame presentati.

Entrambi gli smoke reali verificano preparazione SC1/SC2/SC3, gate di avvio,
pausa/ripresa, cambio pilota, rebind con lo stesso grant, nuova preparazione,
risultati, Ready senza avvio prematuro, reset e ripreparazione. La prova futura
intenzionalmente invalida è rifiutata. Grant/layout sono dichiaratamente
sintetici; nessuna prova di presentazione ottica positiva è inventata.
Sono state ispezionate anche le immagini Syphon acquisite in entrambi i formati.

## Cosa sappiamo del blocco precedente

Il campione del 15 ottobre alle 23:34 mostra il main thread fermo nella condition
SDL dello swap iniziale, prima della gara. Il log unificato dello stesso PID
24770, alle 23:32:53, riporta due volte il display OpenGL 2 come `inactive`.
Oggi lo stesso BenQ/display 2 è online, insieme al display integrato a 120 Hz e
all’MSI a 60 Hz. Questa è una differenza documentata dell’ambiente; il nesso
causale completo non è dimostrato.

Nel [sorgente ufficiale SDL 3.4.12 installato](https://github.com/libsdl-org/SDL/blob/release-3.4.12/src/video/cocoa/SDL_cocoaopengl.m)
la sincronizzazione Cocoa aspetta una condition alimentata da CVDisplayLink;
l’avvio del DisplayLink non controlla i valori restituiti. Questo spiega perché
l’assenza di callback può lasciare lo swap in attesa, ma non stabilisce perché
mancassero in quel processo. Non è stata introdotta una patch ipotetica al driver,
né una disattivazione silenziosa di VSync. Per chiudere il caso residuo serve
riprodurre l’avvio con display inattivo o il ciclo sleep/wake e osservare i codici
CoreVideo nello stesso processo. Nessuno spegnimento, blocco sessione o distacco
di monitor è stato imposto durante questa verifica.

## Ambiente, artefatti e riproduzione

Mac locale Apple M1 Pro, macOS 26.5.2, OpenGL 4.1 Metal 90.5. SDL2 effettivo:
Homebrew sdl2-compat 2.32.70 con backend SDL 3.4.12. Dipendenze e preferenze globali
conservate. Non è una qualifica del Mac della venue, di proiettori, latenza ottica,
comfort, input fisico o prestazioni sotto carico prolungato.

- [Esiti e hash dei binari](verification.json).
- Evidenze complete: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/vsync-2026-10-16/evidence`.
- Build vanilla: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/build/vanilla`; build Relay: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/build/relay`.
- Diagnostica riproducibile e sorgenti dei probe: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/vsync-2026-10-16/tools`.
- Probe finale: `sdl-probe-03.log`, `sdl-probe-result-03.json` e `sdl-probe-summary.json`.
- Stack originale: `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15/evidence/vanilla-runtime-01-sample.txt`.
- Log originale macOS: `original-hang-unified.log`; inventario attuale: `current-displays.json`.

Build eseguite dalla radice `/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc0-sc3-native-qualification-2026-10-15`:

```sh
cmake --build build/vanilla --parallel 6
cmake --build build/relay --parallel 6
ctest --test-dir build/native --output-on-failure
```

I runner di osservazione e lifecycle restano strumenti dedicati di questa
qualifica. La prima versione della dylib di osservazione aveva un errore di
interposizione e ha causato crash del solo tentativo strumentato; è stata corretta
e la vanilla e i due smoke hanno poi passato le prove con la dylib corretta.
Nel probe sono stati corretti anche l’attesa degli eventi e il controllo esplicito
dei flag di finestra. Tutti i tentativi precedenti sono conservati; il totale
60/60 si riferisce alla versione finale 03.

Output/config/temp configurabili sul SSD; l’app mantiene il percorso Addons
interno imposto dall’implementazione esistente. L’hash di `addons_installed.xml`
prima/dopo i run osservati è identico. Nessuna modifica ai sorgenti del prodotto,
nessun commit/push. Processi di prova chiusi; build, strumenti ed evidenze
conservati per riprodurre il caso residuo.
