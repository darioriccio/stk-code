> **Prima esecuzione dei test del 16 ottobre:** sul branch `scintilla/sc4-test-fixes`
> la suite nativa passa 251/251 e la build Relay riesce; test app TholusOS 205/211
> prima dell'ultima correzione: [risultati e limiti](../sc4-test-fixes-2026-10-16/README.md).

# SC4 — implementazione software, 16 ottobre 2026

Codice e asset per carico, fari, esito della missione e introduzione sono consegnati nei repository STK e TholusOS. **La qualifica eseguibile resta aperta:** questa sessione vieta build e avvio nativo. Gli ancoraggi rimangono `development_unqualified`; il profilo pubblico non ammette Ready finché mancano misure native valide e la normale preparazione SC1–SC3.

Riferimento: [piano esecutivo e allegati](../sc4-plan-2026-10-16/PIANO_SC4.md). SC5 non è stata avviata. Nessun commit o push.

## Consegna

- Core C++11 puro con tre slot, proprietà esclusiva dei moduli, guscio/apertura/raccolta, ricevute limitate, recupero sul cavo e risultati immutabili.
- Adattatore nativo con tick autoritativo, singolo dispatch SC1, istanze vive, attraversamenti senza farming e barriera dell'arrivo naturale prima del report.
- HUD condizionale SC4, capsule e cavo con profondità coerente, fari narrativi, feedback e quattro risultati naturali. I fari non costituiscono un gate di vittoria.
- API autenticata additiva e package host `TholusScintillaMission`; preparazione, Ready, telefono, handoff, pausa, reset e risultati verificati nei sorgenti.
- Intro italiana finale: tre varianti C1/C2/C3plus, 18 pannelli originali, tre video H.264 da 40 s/1200 frame e WebVTT. Uso della composizione/player esistente e licenza playback reale; completamento correlato al media effettivo e ritorno alla route STK.
- Recorder diagnostico nativo, query della sagoma raster completa, esportatore e due lettori concordi delle misure. Nessun recorder diagnostico assegna carico o crea un grant ottico.

La vittoria implementata è esclusivamente `naturalFinish && officialTeamRacePosition == 1 && securedCount == 3`. Un modulo Tethered non è consegnato. Force-finish, abort e simulazione accelerata producono `not_completed` con posizione e tempo ufficiali assenti.

## Passaggi e verifica

| Passo del piano | Consegna e controllo svolto | Prova ancora necessaria |
|---|---|---|
| 0 | Baseline HEAD/WIP, istruzioni e calcoli verificati; checkout isolati sul SSD. | Nessuna esecuzione implicita delle vecchie prove SC0–SC3. |
| 1 | Core, conservazione, identità, ownership, reset e limiti revisionati; 32 casi C++ registrati. | Compilare ed eseguire i casi. |
| 2 | Tick unico e `newLap → freeze → notifyLap/report`; 20 controlli sorgente passati. | Arrivo nativo naturale e classifica reale. |
| 3 | Target tipizzati, prove storiche, release/press e live oracle; suite input estesa a 103 casi. | Eseguire consumer e integrazione compilati. |
| 4 | Loader chiuso, sei gruppi e tre stazioni separati dagli asset originali; 16 casi anchor registrati. | Misurare e, se necessario, correggere le posizioni nel gioco. |
| 5 | Capsule, feedback e profondità condivisa revisionati. | Raster/picking GPU e raccolta durante guida reale. |
| 6 | Solo impatto SC3 applicato; presentazione reale prima del timer, watchdog tecnico e recupero; 12 controlli sorgente passati. | Prima presentazione e recupero con rendering eseguito. |
| 7 | Layout v2 condizionale, slot/fari, atlas italiano e audio; 18 controlli sorgente/asset passati. | Leggibilità, audio e GPU a tutte le risoluzioni. |
| 8 | API/DTO/gate e recupero reset; 17 controlli API/cache e 16 lifecycle passati. | HTTP nativo e test host compilati. |
| 9 | Tutti i video decodificati offline, conteggi/durate/hash verificati; grafica ispezionata. Integrazione media revisionata. | Playback AVFoundation/MediaBus e ritorno STK nell'app. |
| 10 | Strumenti di misura e reader nativo/offline implementati; 25 test Python di matematica/fixture negative e 10 di trasporto mock passati. | **Qualifica reale degli ancoraggi non eseguita.** |
| 11 | Audit sorgente finale, contratti, matrice M01–M40, evidenze e trasferimento protetto. | Build vanilla/Relay, C++/Swift/GPU, gara naturale e nuovo ciclo. |

I test C++ e Swift citati sono **scritti e revisionati, non eseguiti**. Le query GPU non sono state eseguite. I controlli Python sul sorgente sono supplementari e non sostituiscono compilazione, chiamanti runtime o prove ottiche.

Sono passati anche i controlli Python sorgente/matematici condivisi: combat 10, defense 20, depth 8, geometria del sobbalzo 6 e tool feedback 6. Quattro checker con stringhe o conteggi precedenti a SC4 sono stati aggiornati mantenendo i controesempi SC2/SC3 e i casi obbligatori. Localizzazione host: 1657 file Swift, zero nuovi rilievi. Nessun test valido è stato rimosso per ottenere il pass.

## Decisioni rispetto al piano

- Layout legacy `front_rear_readonly_v1`/1 conservato; SC4 usa `front_rear_cargo_v2`/2. La revisione viene controllata anche prima della configurazione del core, senza reinterpretare i frame legacy.
- Qualifica geometrica non circolare: diagnostica senza SI5 configurato, guscio e Open costanti in catture distinte. Ogni oggetto/fase deve avere una finestra propria maggiore di 2 s; ogni lato/stile del cavo almeno 3 s. Non si sommano oggetti, fasi o interruzioni.
- Il limite di velocità di una capsula è il massimo dei minimi di velocità fisica nelle finestre complete; il minimo fra le due fasi determina l'envelope dell'oggetto/giro. Un picco isolato non alza tale limite. All'attivazione servono anche attraversamento, prefetch e sagoma interamente ammessa. Fuori envelope si consuma l'occorrenza; una capsula già attiva viene ritirata se la velocità lo supera.
- Il bound di velocità del cavo descrive le misure ottenute. Il cavo è relativo al kart: a runtime valgono sagoma corrente presentata, geometria, profondità e prove SC1. Nessuna frenata, perdita automatica o sospensione per sola velocità; nessun cambiamento ai cooldown o alla forza SC3. Le misure non garantiscono la visibilità di ogni traiettoria futura.
- Preflight esplicito held prima del consumer: hash e lettura delle tracce possono essere costosi. Intent/prepare ordinari usano la cache coerente; un preflight mancante mantiene Ready chiuso. Il reset della stessa gara/pista conserva il certificato; un vero cambio mondo/risorse richiede nuova verifica.

Il recorder usa OpenGL 4.1: tre pass senza scritture, `GL_SAMPLES_PASSED`, output non multisample, controllo della capacità del contatore e polling di disponibilità prima della lettura. Conserva lo stato GL modificato. La sagoma ideale, quella ammessa da range/layout e quella visibile devono avere lo stesso numero positivo di campioni. Queste condizioni seguono le sezioni 4.1.6 e 6.1.7 della [specifica ufficiale OpenGL 4.1](https://registry.khronos.org/OpenGL/specs/gl/glspec41.core.pdf). L'esecuzione sul driver resta da provare.

## Documenti ed evidenze

- [Contratto SC4](SC4_API.md): identità, endpoint, result, intro, misura e qualifica.
- [Procedura operatore e verifica futura](OPERATORE.md).
- [Matrice M01–M40](REGRESSIONI.md): codice/test consegnati e prove mancanti.
- [Esiti strutturati](verification.json), [manifest delle modifiche](source-manifest.json) e [evidenze](evidence/).
- [Asset intro, licenza e riproduzione](../../../../data/immersive_relay/scintilla/intro_it_v1/README.md).

I due repository originali erano già modificati. La consegna confronta HEAD e hash della baseline, salva i file interessati prima del trasferimento e copia soltanto il delta SC4. Le vecchie evidenze di build SC0–SC3 restano storiche e non qualificano questa versione.

Restano distinti: codice consegnato, verifiche offline passate, compilazione/esecuzione locale, gara naturale e qualifica fisica. Le ultime tre non sono state provate in questa sessione.
