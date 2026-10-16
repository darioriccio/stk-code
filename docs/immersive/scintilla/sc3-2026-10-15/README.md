# SC3 — Implementazione sorgenti e verifica senza build

15 ottobre 2026. Attuazione dei passi **0–10** del [piano SC3](../sc3-plan-2026-10-15/PIANO_SC3.md). Il passo11 richiede successiva autorizzazione per compilazione, test nativi, GPU, receiver e sala. SC4 non avviata.

## Comportamento consegnato

La scelta SC3 nell'host registra l'intent prima del grant, prepara input → SC2 → SC3 e mantiene chiusi avvio/ripresa finché tutte le autorità correnti sono confermate. L'intent sopravvive agli errori e al reset. Il server applica lo stesso vincolo agli ingressi Ready, countdown, start, resume e avanzamento automatico.

Una sola AI realmente presente può preannunciare una minaccia. L'ammissione richiede cooldown, energia/calore sufficienti, istanza e geometria valide, sorgente libera da occludenti e contenuta nelle regioni di output dichiarate visibili dal commissioning. Il conto alla rovescia parte dopo la prova ottica del frame di avviso, con3s per una persona e2,5s per2–5 persone. Un solo owner può rinforzare il settore: servono0,6s finali continui e input ancora valido alla decisione. Ogni interruzione o completamento impone rilascio/nuova pressione.

Il fallimento della difesa genera un impulso a40m/s, raggio0,12m. Il lancio richiede un suo frame presentato; il percorso viene verificato attorno al vero step Bullet contro track, compound del kart e ostacoli dinamici. Il primo ostacolo prevale. La conferma dell'impatto attende un frame presentato del contatto e riconvalida le istanze. L'effetto riduce una volta del15% la componente tangenziale, conserva componente normale/velocità angolare e non applica animazioni o invulnerabilità native. Un evento di impatto con identità race/threat/humanInstance viene pubblicato per SC4, senza consumatori di carico.

Energia, riserva e calore sono unici per squadra e usano aritmetica intera con Q=1000F. Il boost richiede acquisizione, una nuova pressione e costo25 confermato solo dopo applicazione; conserva la riserva difensiva e controlla tutti i boost nativi, incluso il fade. HUD, feedback persistenti e snapshot espongono indisponibilità e riarmo.

Il retrovisore usa le facce già catturate, yaw180° senza specchio, FOV orizzontale100°. È **visibile e read-only**: nessun tiro nel retro e nessun intercetto di proiettili. Il layout producer revisionato governa rendering, maschere e classificazione storica del pixel. Primaria e tool su HUD non ricadono nel world picking.

## Passi e ricontrolli

| Passo | Consegna e controllo effettuato sui sorgenti |
|---|---|
| 0 | Worktree STK/host su SSD, baseline completa di HEAD e WIP copiata e verificata; istruzioni e piano letti. Delta consegnato con hash e guard contro modifiche concorrenti. |
| 1 | [Contratto API](API.md), enum stabili, identità, parametri fissi e layout. DTO host compatibile con campi additivi. |
| 2 | Core difesa senza GL/Bullet/HTTP: riserve, costi, calore, owner, epoch/serial, timer e cooldown. Revisionati confini, rilascio alla deadline e nuove pressioni fra tick. |
| 3 | Layout/retro, campionamento unico, maschere HUD, barre e badge disgiunti dalla firma THB1; regole comuni alle due proiezioni. |
| 4 | Contesto da frame storico; esclusività offesa/difesa/boost; perdita sticky, nessun credito retroattivo e feedback distinti. |
| 5 | Direttore su AI reali, filtri SC2/live, priorità deterministica, query sorgente limitata, preavviso da intervallo ottico validato. Negativi espliciti annullano anche gli stati in attesa. |
| 6 | Solver double stabile, socket esterno con sweep/overlap, hook prima/dopo Bullet, CCD compound con bound angolare e narrowphase conservativa. Nessun confronto solo agli endpoint. |
| 7 | Impatto tangenziale, conferma boost, costo dopo esito, protezione specifica SC3 e evento futuro. Verificata compatibilità con fade nativi e accessor del veicolo. |
| 8 | API sul game thread, intent latched, gate server, sospensioni e risorse conservate. Rebind logico con CAS/epoch/idempotenza mantiene il grant fisico e il registro anti-replay. |
| 9 | Modalità host SC3, commissioning esplicito, preparazione sequenziale con riconvalida dopo gli await, UI/errori en/it, recupero cambio pilota. [Checkpoint host](</Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController/docs/immersive-stk/sc3-2026-10-15/README.md>). |
| 10 | Audit incrociato host/server, controlli Python consentiti, regressioni native scritte e registrate, [matrice S01–S24](REGRESSIONI.md), manifest e limiti di evidenza. |
| 11 | Da eseguire con autorizzazione: build/test nativi, profilo misurato della sala, sequenza reale, GPU/receiver e playtest. |

## Decisioni ricavate dal checkout

- **Handoff e grant SI5:** chiudere un endpoint concesso richiede il vero termine del produttore prima di crearne un altro. Per la rotazione con gli stessi dispositivi, il nuovo endpoint `/spatial/rebind` cambia solo l'associazione STK sorgente→persona. SI5 continua a possedere sessione, lease, calibrazioni e lifecycle. Durante il rebind rimangono abilitati stati canonici, cancellazioni, rinnovi e presentazione; aim/azioni sono sospesi. Il server incrementa inputEpoch, cancella effetti pendenti e riprepara SC2/SC3. Energia/calore/cooldown non vengono ricaricati. Un cambio del set fisico o delle calibrazioni mantiene il percorso di isolamento completo esistente; questa modifica non simula un'uscita del produttore per ottenere un nuovo grant.
- **Bullet incluso:** il fork usa la callback `ContactResultCallback` con `const btCollisionObject*`, senza `btCollisionObjectWrapper`. Adeguata anche la callback del volume SC2 introdotta nel WIP precedente, conservando geometria e sostegno della correzione. Il calcolo dell'angolo usa i quaternion effettivamente disponibili nella versione inclusa.
- **Fade del boost:** il getter dei tick indica il tempo prima del fade, non l'intera durata attiva. Aggiunto un accessor di sola lettura sul kart che inoltra `MaxSpeed::isSpeedIncreaseActive`; SC3 usa questo per evitare sovrapposizioni anche durante il fade.
- **Intervalli fisici mancanti:** il race loop supportato aggiorna a un singolo tick. Un salto osservato durante una minaccia annulla con `physics_step_gap`; una coppia di trasformazioni mancante annulla con `collision_step_missing`. Nessun effetto viene ricostruito dal solo rendering. Gli integrali di standby sono verificati anche con avanzamento segmentato.
- **Collisioni incerte:** broadphase e forme sono limitati e verificati. Forme cambiate, teleport, dinamici non supportati, solver non delimitabile o precisione fuori scala annullano il volo. I bounds precedenti sono copiati prima dello step: il puntatore alla vecchia shape non viene dereferenziato dopo eventuale sostituzione. Il settore al contatto usa la posa umana interpolata alla stessa frazione del CCD.
- **Visibilità fisica:** le regioni ammesse governano sia la preparazione dell'HUD sia l'intera impronta della sorgente/impulso nel main. Le prove GPU e ottiche non trasformano una dichiarazione di commissioning in una misura hardware.

## Verifiche eseguite e limiti

Eseguiti soltanto controlli Python di sorgenti, geometria e calcoli:

- `check_scintilla_depth.py`:6 controlli.
- `check_scintilla_stagger_clearance.py`:6 controlli.
- `check_scintilla_combat.py`:10 controlli.
- `check_scintilla_tool_feedback.py`:6 controlli.
- `check_scintilla_defense.py`:13 controlli.
- Oracle del piano `verifica_calcoli.py`:10 gruppi matematici.
- Host: guard localizzazione su1648 file Swift, **0 nuovi finding**;17 nuove voci complete in en/it.

Esiti e comandi sono in [verification.json](verification.json). [source-manifest.json](source-manifest.json) distingue hash di baseline e consegna nei due repository. I due vecchi scanner SC2 sono stati aggiornati per il nuovo gate composto, il cleanup difesa e l'aggiunta dei casi: nessuna regressione precedente è stata rimossa.

Scritti/registrati **21 casi core difesa**, **6 casi Bullet reale** e **8 nuovi casi consumer** oltre ai66 precedenti (74 totali), più le regressioni host nel framework esistente. Questi test nativi sono stati letti e revisionati, **non compilati o eseguiti**. Gli scanner di sorgenti non provano esecuzione C++, GLSL o Swift, collisioni Bullet reali, latenza/GPU, presentazione fisica, audio, comfort o bilanciamento.

Nessuna build, configure, compilazione/typecheck, avvio app/producer/helper, preview o hardware. La qualifica runtime resta il passo11 del piano. Il numero di supporter può cambiare nel core senza ricaricare risorse; la gestione dell'autorizzazione di un diverso set fisico rimane soggetta al lifecycle SI5 reale, da verificare nel relativo scenario operativo.

## Correzioni successive all'audit

Il [checkpoint delle quattro correzioni](../sc3-audit-fixes-2026-10-15/README.md)
aggiorna handoff operatore, sweep socket, continuità SC2 durante gli allarmi e
reset in-place con riuso del grant. I conteggi sopra descrivono la prima consegna;
nuove regressioni e limiti di verifica sono registrati nel checkpoint successivo.

Il [checkpoint dei tre finding residui](../sc3-final-three-fixes-2026-10-15/README.md)
corregge il commit istantaneo del boost, la priorità dei candidati con query GPU
pendenti e la visualizzazione del costo. Registra 47 controlli Python superati,
10 gruppi matematici e le tre nuove regressioni native di selezione, non eseguite.
