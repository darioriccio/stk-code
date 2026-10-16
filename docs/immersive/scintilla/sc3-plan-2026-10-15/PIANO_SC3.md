# SC3 — Piano esecutivo: ritorsione, difesa, energia e vista posteriore

Data: 15 ottobre 2026. **Piano per una successiva implementazione; SC3 non implementata da questo incarico.**

Destinatario: LLM implementatore. Procedere nell'ordine della sezione 13, completando e ricontrollando un passo prima del successivo. Le formule, le convenzioni e i casi numerici qui fissati sono parte della specifica. Non sostituirli con approssimazioni inventate. Una premessa smentita dal checkout va documentata e risolta prima di modificare il relativo percorso.

## 1. Obiettivo, perimetro e decisioni

Fonte dei requisiti: [design v2](../../../Tholus_STK_Missione_Scintilla_Design_v2.md), sezioni 5, 7, 9, 14 e matrice D01–D04/G03/G04/H01–H03/R01–R03. Il precedente [piano SC2](../sc2-plan-2026-10-15/PIANO_SC2.md) descrive una baseline storica: SC2 e le sue correzioni ora esistono nei sorgenti. Non trattare quel piano come fotografia del codice corrente.

Consegnare un primo profilo locale completo: un'AI reale preannuncia un attacco, la crew può rinforzare il settore corretto, l'eventuale impulso percorre una traiettoria visibile e collide realmente. Energia e calore appartengono alla squadra; boost e difesa usano la stessa energia. L'impatto sul kart umano conserva controllo e camera stabilizzata.

Decisioni vincolanti di questo incremento:

- Un solo archetipo e **una sola minaccia globale attiva**, anche con cinque supporter. Le AI continuano a usare il driving STK.
- Quattro settori, un solo canale di rinforzo attivo di squadra, un solo fascio per partecipante. Gli altri possono continuare SC2.
- Retrovisore frontale **visibile, a orientamento fisso, non specchiato e non interattivo**, ricavato dalle sei facce già catturate. Il design ammette questa prima versione. Nessun secondo rendering della scena.
- `rear_alert=true`, `rear_visible_view=true`, **`rear_targetable_view=false`**, `projectile_interception=false`. Non chiamare completa la variante con tiro nel retrovisore o intercetto: sono estensioni esplicitamente escluse da questo profilo, ammesse ma non obbligatorie dal design.
- Niente carico, sgancio moduli, fari, vittoria missione, Party, ruoli o nuova difficoltà adattiva: SC4–SC6 restano fuori. Pubblicare l'evento di impatto necessario alla futura SC4, senza implementarne i consumatori.
- Solo gara locale ammessa dal gate SC2; nessuna promessa di network/rewind/replay deterministico.

I numeri di bilanciamento di questo piano sono **proposte iniziali di progetto**, non misure sperimentali di tempi umani, comfort o difficoltà. La matematica ne verifica coerenza e limiti; hardware e playtest ne devono confermare l'adeguatezza.

Questa consegna esegue solo calcoli Python e controlli documentali. Non autorizza build, CMake configure, compilazione/typecheck, avvio gioco/app/helper o hardware. Nella successiva implementazione mantenere il limite senza build finché l'utente non lo cambia; scrivere e revisionare le regressioni native senza presentarle come eseguite.

## 2. Baseline e punti di integrazione verificati

Repository STK: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Immersive SupertuxKart/stk-code`.

Repository host: `/Volumes/SanDisk 2TB/the usual neXt/03_PROGETTI/Social Table App/SocialTableController`.

Il [manifest](source-manifest.json) registra HEAD e hash dei riferimenti di questa pianificazione, incluso WIP. È una fotografia documentale, non certificazione dell'intero progetto. Leggere gli AGENTS applicabili. Usare SSD montato e scrivibile per worktree, temporanei e output. Un worktree creato da HEAD **non contiene il lavoro SC1/SC2 non committato**: copiarne una baseline verificata prima di intervenire. Preservare modifiche e chat concorrenti.

Nel seguito i percorsi STK sono relativi alla radice STK; `IR/` abbrevia `src/modes/immersive_relay/`. I nomi di nuovi file, campi ed endpoint sono **proposte**, non API già disponibili.

| Percorso attuale | Evidenza e istruzione per SC3 |
|---|---|
| `IR/scintilla_combat.hpp/.cpp` | Core C++11 separato da GL/Bullet/HTTP, owner/epoch, brecce e feedback tool. Conservarlo; aggiungere un core difesa separato, non trasformare le fasi dei rivali nelle fasi dello scudo umano. |
| `IR/spatial/scintilla_input.hpp/.cpp` | Prove storiche, aim persistenti, perdita sticky fino al rilascio, coda affidabile dei colpi. Attualmente `viewId` deve essere `main`. Classificare il contesto HUD prima del world picking, senza fidarsi di una stringa view inviata dal client. |
| `IR/spatial/scintilla_engine.cpp` | `updateCombat` è il commit autoritativo; `beforeCapture` registra trasformazioni smoothed e frame; `refreshCombatInput` pubblica gli stati. Nessun danno/costo da callback GL o HTTP. |
| `IR/spatial/scintilla_target_lifetime.hpp` | L'id non identifica da solo un'istanza viva. Riutilizzare la verifica proprietario/istanza/world, anche al commit dell'impatto. |
| `IR/relay_race_mode.cpp`, `src/physics/physics.cpp` | Il combat segue `StandardRace::update`; Bullet esegue uno step fisso in `Physics::update`. Campionare le trasformazioni prima e dopo lo step reale, non dedurre una traiettoria da due rendering. |
| `IR/render/camera/immersive_camera_rig.cpp` | Base camera: destra +X, alto +Y, avanti +Z; `right=up×forward`. Retro = yaw di π, non cambio del target camera. |
| `data/shaders/utils/immersive_scintilla.frag` | Picking/depth conservativi condivisi e badge persistenti SC2. Non reintrodurre tolleranze additive in depth normalizzata. |
| `data/shaders/utils/immersive_sphere_sampling.frag` | Campiona le facce già disponibili. Usarlo per il retrovisore senza invertire arbitrariamente UV o facce. |
| `IR/crew_action_handler.cpp` | Legacy shield è bubblegum omnidirezionale, boost usa percorsi legacy. Non sono lo stato SC3; non chiamare `findRivalKart` come fallback. |
| `src/karts/kart.cpp`, `src/karts/max_speed.cpp` | `adjustSpeed` scala velocità lineare **e angolare**, incluso moto verticale. Non usarlo per il nuovo impatto. `instantSpeedIncrease` e minimo velocità richiedono gestione esplicita del boost. |
| `lib/bullet/src/BulletCollision/NarrowPhaseCollision/btContinuousConvexCollision.cpp` | Il cast considera trasformazioni iniziali/finali di entrambe le forme e velocità angolare. Usare la libreria già inclusa, non implementare un nuovo GJK. |
| Host `Modules/GameShowControl/ImmersiveSTKSpatialInput.swift`, `ImmersiveSTKSpatialPreparation.swift`, `ImmersiveSuperTuxKartRuntimeController.swift` | Attualmente selezione diagnostica/SC2 e intent/preparazione/gate Ready SC2. Aggiungere SC3 esplicita lungo tutto il flusso, non soltanto nel picker UI. |

Conservare in particolare le correzioni SC2 di [perdita del fascio](../sc2-beam-loss-2026-10-15/README.md), [feedback/riarmo](../sc2-tool-feedback-2026-10-15/README.md), [volume fisico](../sc2-stagger-clearance-2026-10-15/README.md) e [Ready](../sc2-ready-gate-2026-10-15/README.md). Nessun nuovo stato può cancellare un negativo sticky o avviare la gara prima della preparazione richiesta.

## 3. Unità, parametri e aritmetica

Leggere e congelare la frequenza fisica F alla preparazione, senza assumerla uguale ai FPS. Configurazione attuale: **120 Hz**. Tick autoritativi `int64_t`; clock monotono in microsecondi per freschezza, transport e prove ottiche. La pausa sospende la simulazione, non i timeout di servizio.

Per una durata intera D in µs: `minTicks=(D*F+999999)/1000000`, `maxTicks=(D*F)/1000000`. Validare F, segni e overflow. Minimi arrotondati per eccesso; massimi per difetto. Nessun epsilon temporale. Un aggiornamento multiplo va suddiviso ai confini degli eventi, non moltiplicato usando solo lo stato finale.

| Parametro proposto | Valore | Tick a 120 Hz | Significato |
|---|---:|---:|---|
| Preavviso C=1 | 3 s | 360 | Minimo dopo prova di prima presentazione |
| Preavviso C=2–5 | 2,5 s | 300 | Stesso criterio, una sola minaccia |
| Rinforzo continuo finale | 0,6 s | 72 | Deve coprire l'intervallo finale fino alla decisione |
| Margine budget | 0,25 s | 30 | Riserva energetica, non tolleranza aim |
| Intervallo globale dopo esito | 8 s | 960 | Nessun nuovo preavviso prima di questa scadenza |
| Cooldown della stessa AI | 12 s | 1440 | Dall'esito anche se la minaccia è annullata |
| Protezione dopo impatto | 5 s | 600 | Dall'effettivo commit, soltanto contro SC3 |
| Boost: acquisizione / durata / fade / cooldown | 0,5 / 1 / 0,5 / 6 s | 60 / 120 / 60 / 720 | Cooldown e costo di squadra |
| Volo pianificato ammesso | 0,35–0,90 s | 42–108 | Limite geometrico; collisioni reali possono avvenire prima |
| Distanza candidato | 8–24 m | — | Centri/socket reali, non posizione in classifica |
| Velocità impulso / raggio | 40 m/s / 0,12 m | 0,333333 m/step | Distanza per tick maggiore del diametro: serve CCD |
| Energia iniziale/capacità | 100 | — | Unica risorsa shield+boost, distinta da nitro e carico |
| Costo attivazione rinforzo | 5 | — | Una volta per pressione ammessa |
| Consumo settore minacciato | 20/s | — | Include mantenimento, non sommare altri 2/s |
| Mantenimento fuori contrasto | 2/s | — | Solo canale realmente attivo |
| Rigenerazione | 5/s | — | Quando nessun rinforzo è effettivamente attivo |
| Calore max / iniziale | 100 / 0 | — | Condiviso, mai per dispositivo |
| Riscaldamento contrasto / standby | 25/s / 2/s | — | Raffreddamento 20/s quando inattivo |
| Costo boost | 25 | — | Una volta dopo conferma dell'applicazione |

Energia/calore: usare fixed point con **Q=1000·F unità interne per unità esposta**. A ogni tick, un rate intero r/s aggiunge/sottrae esattamente `1000*r`, senza perdita da divisione. Costo c = `c*Q`, capacità = `100*Q`. A 120 Hz Q=120000: energia cap=12000000, costo attivazione=600000, contrasto/tick=20000, rigenerazione/tick=5000. Usare interi a 64 bit e clamp; nessun resto conservato oltre saturazione. Il display può convertire a float, le decisioni no.

Tutti i confronti di eleggibilità sono sul valore prima dell'addebito, e risorsa/calore vengono aggiornati nell'ordine della sezione 5. Zero energia o calore 100 interrompono il canale e richiedono rilascio; un input ancora held non lo riattiva automaticamente.

### 3.1 Budget e impossibilità di tenere il bottone per sempre

Sia T il preavviso, M=0,25 s. Riserva difesa `R=5+20·(T+M)`: **70** con C=1, **60** con C≥2. Headroom termico: `H_admit=100−25·(T+M)`: **18,75** oppure **31,25**. Se F non consente durate esatte, calcolare con i tick effettivi per eccesso: `R_Q=5Q+20000*(warningTicks+marginTicks)` e `H_admit_Q=100Q−25000*(warningTicks+marginTicks)`.

Una difesa iniziata subito e mantenuta per tutto T costa 65/55 e scalda 75/62,5. Una difesa esatta negli ultimi 0,6 s costa **17** e scalda **15**. Non chiamare 17 il budget sufficiente per qualunque strategia: quello completo è R. Il margine non permette di accettare campioni più vecchi dei 60000 µs SC1.

Per impedire che un hold inattivo o boost ripetuti impediscano per sempre l'ammissione degli attacchi:

1. Fuori da una minaccia, boost/standby possono consumare solo energia **sopra R**; standby non può superare `H_admit`. Il raggiungimento del limite spegne il canale e pubblica `defense_reserve_protected`, con rilascio richiesto. Un nuovo owner non azzera questi valori.
2. Durante una minaccia il boost è indisponibile. Il canale sul settore promesso può usare la riserva; un settore sbagliato resta soggetto alla riserva. Cambi volontari di settore richiedono release/press e nuovo costo; non garantirne una sequenza illimitata.
3. Dopo l'esito, se E<R o H>H_admit, nessun nuovo attacco finché il recupero non è sufficiente. Held bloccati/inattivi **non** impediscono rigenerazione o raffreddamento. Il direttore non attende che l'utente rilasci per riabilitare i rivali.
4. Ogni attacco richiede una nuova pressione con prova del **nuovo threatId**. Un hold iniziato prima dell'allarme può consumare standby ma non blocca il nuovo attacco; viene chiuso con `release_required`. Dopo ogni esito si richiede di nuovo release/press.

Con C=1, partenza E=70/H=18,75 e difesa completa: E=5/H=93,75. Per la prossima ammissione servono `(70−5)/5=13 s` e `(93,75−18,75)/20=3,75 s`; domina il recupero energia rispetto al gap globale di 8 s. Con C≥2, E=60/H=31,25 → E=5/H=93,75: recupero 11 s e 3,125 s. Il direttore sceglie `max(cooldown, energia, calore)`, mai un attacco non difendibile per consumo automatico del sistema. Il blocco del comando resta anche quando le risorse tornano disponibili: premere senza mai rilasciare non dà invulnerabilità.

Boost con C=1 richiede E≥95; da pieno lascia 75, ancora sopra la riserva70. Con C≥2 richiede E≥85. La barra deve mostrare chiaramente la parte riservata e il costo, altrimenti «energia 80 ma boost indisponibile» sarebbe incomprensibile.

## 4. Stati autoritativi, identità e opportunità AI

Proporre `IR/scintilla_defense.hpp/.cpp`: core C++11 puro, strutture POD e vettori limitati, senza Bullet/GL/HTTP. Input di geometria e presentazione arrivano come fatti validati dall'adattatore. Dimensioni massime: 1 minaccia, 1 impulso, 1 impatto pendente, 1 canale rinforzo, 5 feedback sorgente, fino a 32 cooldown AI vivi. Niente cronologie crescenti con la gara.

Identità minima: `raceEpoch`, `inputEpoch`, `rosterRevision`, `profileRevision`, `threatId`, `attackerId+instance`, `humanId+instance`, `layoutRevision`, `routeRevision`, `projectionContextId`. ThreatId monotono per race; confrontare la coppia race/id. Per input mantenuto: participantId + controlEpoch + seriale proof. Non usare sourceId come identità della persona.

Stati:

```text
Idle → AwaitWarningPresentation → Telegraph → LaunchPending
     → Flying → AwaitImpactPresentation → Hit → Cooldown → Idle
                     ↘ Miss / WorldBlocked / Cancelled ↗
Telegraph → Blocked → Cooldown
qualsiasi stato non terminale → Cancelled → Cooldown
```

`LaunchPending` e `AwaitImpactPresentation` attendono prove ottiche, non sono estensioni del tempo per difendere. Il rinforzo si decide alla scadenza del Telegraph. Un nuovo target, settore o threatId richiede l'intero ciclo da capo. In `AwaitWarningPresentation` non consumare energia di contrasto né far avanzare countdown nascosti.

Candidato: AI realmente presente, non umana, non eliminata/arrivata, nessuna animazione/rescue/stagger/recovery/scudo nativo incompatibile, istanza viva, cooldown terminato; kart umano presente e controllabile; profilo e input validi. Distanza 8–24 m, linea libera e sorgente resa in un canale effettivamente visibile. Non basta essere in classifica dietro/al primo posto. Nessun rivale idoneo → nessun attacco, senza spawn né fallback.

Scegliere deterministically il candidato con ultimo attacco più remoto, poi distanza, poi id. Applicare prima tutti i filtri e la prova visiva; non scegliere il primo e sostituirlo silenziosamente se fallisce. Le opportunità dipendono da geometria reale e possono essere rare: misurarle, non aggirare i filtri per aumentare la frequenza.

Per C=1, iniziare il preavviso solo dopo la chiusura/scadenza dell'eventuale catena solo SC2 più 0,5 s di margine; nuove azioni offensive durante un preavviso sono una scelta volontaria, mai un requisito per sopravvivere. Per C≥2 non richiedere di completare un'offensiva prima di difendere. Un colpo SC2 che rende l'attaccante non idoneo annulla la minaccia sullo stesso tick, prima del lancio/commit.

## 5. Contratto del fascio e ordine degli aggiornamenti

Introdurre un contesto esplicito `None | WorldOffense | DefenseSector | ServiceBoost | ReadOnlyRear`. Il consumer lo ricava da **pixel e layout storici**, non dalla preferenza del client. Una pressione fissa contesto, target/settore e controlEpoch. Cambiare area con held attivo interrompe il fascio e richiede rilascio; non trasformare una breccia in difesa a metà pressione.

Il contesto diventa visibile nel mirino **prima** di premere: icona breccia, settore/numero di direzione, boost, retro sola lettura. Primaria su HUD/retro produce feedback `non_targetable_surface` e non passa alla scena sottostante. Un miss nel mondo non attiva difesa. La pressione secondaria inoltrata a difesa/boost non entra nel core offensivo; passare a difesa chiude prima qualsiasi contributo SC2 ancora legato a quel partecipante.

Rinforzo: owner unico scelto per ordine autoritativo di ammissione, spareggio participantId stabile; ulteriori sorgenti non moltiplicano forza o consumo, ricevono `sector_busy` con owner. Nessun takeover automatico mentre tenuto: dopo rilascio/perdita dell'owner, un altro può acquisire con nuova pressione. La continuità non passa fra owner. Un settore diverso da quello minacciato non accumula difesa utile.

Con `deadline=start+warningTicks`, bloccare solo se lo stesso owner ha coperto **tutto `[deadline−holdTicks, deadline)`**, possiede ancora held valido alla decisione e le prove appartengono al threat corrente. Rilascio/negativo ammesso sul tick deadline ha precedenza sul blocco. L'aim positivo precedente non viene reso valido retroattivamente dalla prova arrivata dopo deadline. Una perdita seguita da hit prima del tick resta un'interruzione, come nella correzione SC2; azzerare continuità e mostrare release-required.

Ordine per ogni passo autoritativo:

1. Applicare reset, pausa, cambio identità/roster/route e invalidazioni; verificare live owners, discontinuità e stato umano.
2. Consumare release/perdite sticky e prove già risolte; aggiornare contesti esclusivi. Le prove GPU/HTTP arrivate dopo la decisione non cambiano il passato.
3. Avanzare i contatori e addebitare l'intervallo appena trascorso in base al canale che lo ha realmente coperto. Campioni iniziali appena ammessi iniziano **ora**, senza credito del tick precedente.
4. Aggiornare SC2 e applicarne gli effetti; riconciliare nuovamente l'idoneità dell'AI. Poi risolvere scadenze SC3, block/cancel/launch e contatti raccolti dalla fisica.
5. Confermare un effetto una sola volta, aggiornare risorse/cooldown/protezioni e pubblicare uno snapshot coerente per frame, shader e API.

Le primarie SC2 mantengono il proprio controllo storico e nessun secondo limiter. Freschezza held **≤60000 µs monotoni**, uguale al consumer attuale: non arrotondare a 8 tick/66,67 ms. Aim coalescibili; release, press, perdita e transizioni non coalescibili.

## 6. Settori: formule e casi limite

Usare la base del kart umano fisico e la direzione opposta alla gravità normalizzata `U`; non la camera smoothed. Proiettare avanti sul piano ortogonale a U:

```text
F = normalize(forwardKart − U·dot(forwardKart,U))
R = normalize(U × F)
d = attackerSocket − humanSocket
theta = atan2(dot(d,R), dot(d,F))       // positivo verso destra
```

Se gravità/base/proiezione non finite o lunghezza proiettata <1e−6 m, annullare l'opportunità. I settori nominali sono fronte `[-45°,45°)`, destra `[45°,135°)`, retro `[135°,180°] ∪ [-180°,−135°)`, sinistra `[-135°,−45°)`. Normalizzare +180 a −180 per una sola rappresentazione. Centri: 0°,90°,180°,−90°.

Admission con margine 5°: distanza angolare dal centro **≤40°**. Mantenimento dello stesso settore: distanza **≤50°**. Esempi: 39° fronte ammesso; 44° nuovo attacco non ammesso; fronte già aperto a49° continua;51° annulla. Usare `atan2(sin(delta),cos(delta))` per il wrap: 179° e−179° distano2°, non358°. Non aggiornare il settore durante un attacco già promesso.

Durante il volo controllare il bearing del **proiettile** rispetto al kart umano; se esce dalla fascia di mantenimento prima del contatto, dissolvere l'impulso e pubblicare `sector_changed`. Non far colpire da sinistra un attacco annunciato a destra. È una scelta conservativa: una curva può far mancare l'attacco, non generare una nuova minaccia senza preavviso. Alla collisione non normalizzare il vettore quasi nullo: usare il bearing verificato del tratto pre-contatto e quello all'ingresso sul corpo.

## 7. Geometria del retrovisore e layout unico

Definire un `ScintillaLayout` versionato, generato dal producer, immutabile nel frame e usato da shader/resolver/host. Le coordinate seguenti hanno origine in alto a sinistra e pixel center coerenti con SC1. `S=min(W,H)`, centro canvas `(W/2,H/2)`. Per coordinate isotrope q: `pixel=(W/2+qx*S/2, H/2+qy*S/2)`. Non applicare questa formula direttamente a UV senza compensare l'aspect ratio.

| Elemento SC3 | Geometria nel piano q |
|---|---|
| Retrovisore | centro `(0,-0.52)`, half-size `(0.36,0.18)`, rapporto pixel 2:1 |
| Quattro nodi: sinistra, fronte, retro, destra | centri `(-0.6,0.62)`, `(-0.2,0.62)`, `(0.2,0.62)`, `(0.6,0.62)`, raggio `0.07` |
| Boost | centro `(0,0.85)`, raggio `0.06` |
| Energia/calore | due barre non interattive, `qx∈[-0.42,0.42]`, `qy∈[0.42,0.46]` e `[0.48,0.52]` |

I numeri con virgola sono decimali italiani; negli inizializzatori C++/GLSL usare punto. Distanze tra nodi0,4 e diametro0,14: margine0,26. Massimo raggio del box retro `sqrt(0,36²+0,70²)=0,78714675`; nodi esterni `sqrt(0,6²+0,62²)+0,07=0,93278618`; boost0,91. Queste aree rientrano nel disco unitario domemaster; **ciò non prova la leggibilità sulla cupola reale**.

I badge persistenti SC2 attuali a v=.62 e i feedback di fallback a v=.70 possono interferire con barre/testi nuovi. Nel nuovo layout collocare i cinque badge di ciascuna riga a qx=`[-0.32,-0.16,0.0,0.16,0.32]`, con qy=.08 (tool) e .25 (transitorio), box half-size≤.055. Verificare gli interi pittogrammi, non soltanto i centri. Il frame SC2 senza SC3 conserva il proprio layout precedente. Non mantenere due calcoli concorrenti per il medesimo profilo; fornire geometria e box al codice shader e CPU da una sola descrizione revisionata. La firma ottica THB1 attuale scrive alpha nei primi256 pixel delle righe estrema superiore e inferiore (`scintillaAlpha`). Conservare tale alpha come ultimo passaggio anche fuori dal disco; nessun overlay deve riscriverlo. Il layout proposto non tocca queste righe. Verificarlo anche dopo arrotondamento e resize.

A W=H=1024: x=[327,68;696,32], y=[153,60;337,92]. A2048×1024: x=[839,68;1208,32], stessi y e dimensioni in pixel. A2048²: dimensioni737,28×368,64. Arrotondare il rettangolo a pixel interi una sola volta nel producer; pubblicare il rettangolo finale e ricalcolare A=larghezza/altezza per le formule seguenti. Shader e resolver devono leggere quello stesso rettangolo, non ricalcolarlo indipendentemente.

Vista rear non specchiata: `Rrear=−Rcamera`, `Urear=Ucamera`, `Frear=−Fcamera`. Determinante della rotazione diag(−1,1,−1)=+1: yaw180°, non riflessione. FOV orizzontale100°, `h=tan50°=1,19175359259421`, `v=h/A`. Per A=2, `v=.595876796297105`, FOV verticale **61,5794660576643°**.

Per coordinate locali del rettangolo s,t∈[0,1]:

```text
a=(2s−1)*h; b=(1−2t)*v
directionCamera=normalize((-a,b,-1))
directionWorld=Rcamera*dx+Ucamera*dy+Fcamera*dz
```

Centro→(0,0,−1); lato destro→X negativo, corretto guardando dietro senza specchio. Angolo alto-destro→`(-.715365381959,.357682690979,-.600262828159)` per A=2. Mapping inverso per d camera con `−dz>0`: `s=.5*(1−dx/((-dz)*h))`, `t=.5*(1−dy/((-dz)*v))`. Nessun tilt domemaster va applicato una seconda volta al raggio rear: è una vista nel compositore dell'output.

Campionare `sampleEnvironment(directionCamera)` con lo stesso snapshot di facce; applicare l'overlay del lock nel dominio rear usando quella direzione e la stessa policy depth. Il lock deve identificare il kart reale, non solo un punto HUD. La prima versione rimane read-only; G03 si verifica come mapping visuale corretto **e rifiuto esplicito dei tiri**, non come tiro posteriore completato.

Ordine input: nodi interattivi visibili → rear read-only → altre aree HUD opache riservate → main world. Il nodo interattivo ha un disco di fondo visibile coincidente con l’area cliccabile, senza una hitbox trasparente più grande. Le aree esatte vanno mascherate anche nell'evidenza visiva della sorgente AI: non dichiararla visibile nella scena coperta da un pannello. Ai bordi, classificazione semiaperta del rettangolo e inclusione circolare deterministica; se l'incertezza temporale attraversa revisioni/layout o contesti differenti, rifiutare l'azione ambigua.

Per equirettangolare non assumere che il centro del buffer sia una superficie leggibile nella stanza: la preparazione deve associare `layoutProfile=front_rear_readonly_v1` al mapping venue con aree visibili. Se il profilo non lo prova, SC3 non è preparabile; SC1/SC2 restano selezionabili. Non spacciare il test del disco per commissioning fisico.

## 8. Preavviso realmente visibile e presentazione ottica

Un frame prodotto non è un frame presentato. Riutilizzare grant, clock interval, frame identity e presentazione SC1; non introdurre timestamp host fidati senza conversione né accettare intervalli futuri.

Ogni frame estende i metadati con snapshot SC3: threatId, fase, attacker instance, settore, layoutRevision, vista in cui la sorgente è visibile, impulso/eventuale contactId. Questi dati sono del producer; l'host restituisce le prove di presentazione dei frame già riconosciuti, **non un booleano arbitrario `warningSeen`**. Il server ricava da quei frame quali eventi potevano essere mostrati.

Per un intervallo ottico convertito `[L,H]`, usare **H** come ultimo istante possibile di presentazione. Entrare in Telegraph soltanto quando `nowMonotonic≥H` e il frame contiene sorgente/lock visibili, settore e pannello difesa. Impostare start al tick autoritativo corrente. Così il tempo disponibile dalla prima presentazione reale è almeno T: non sommare retroattivamente tick trascorsi prima dell'ack. Un `presentation_ready` generico non sostituisce questa prova per minaccia.

Durante Telegraph richiedere continuità del canale: frame valido presentato più recente con età pessimistica `now−L≤150000 µs`, senza lacune dichiarate, source instance ancora viva, linea di tiro libera e visibilità conservativa corrente. 150 ms è una nuova soglia di continuità video, non quella aim60 ms. Se storia/profilo/clock non possono provarla, cancellare e richiedere un nuovo preavviso completo. Non abbassare silenziosamente i controlli SC1 per rendere SC3 preparabile.

Alla scadenza: se difesa valida → Blocked; altrimenti LaunchPending. Pubblicare il lampo e l'impulso alla sorgente, senza avanzarlo; dopo prova di presentazione di questo frame iniziare Flying. Timeout massimo servizio500000 µs per ogni attesa di presentazione; cancellare se non soddisfatto. Lo stesso vincolo di continuità vale durante il volo: anche il pulse deve avere rappresentazione realmente visibile in main o rear, con depth/clipping corretti. Se esce da entrambi i canali, cancellare; non basta continuare a vedere la sorgente AI. Lo spostamento del kart durante LaunchPending richiede ricalcolo della soluzione balistica, stesso settore e nuova convalida di sorgente/linea libera; se non valida, annullare, non lanciare dal vecchio punto.

Al contatto fisico fermare il pulse, mostrare l’esito sul settore HUD promesso ed eventualmente sull’ultimo punto world se ancora proiettabile, entrare AwaitImpactPresentation. Il contactId deve essere associato al feedback di settore effettivamente composto: il corpo umano può essere troppo vicino alla camera per mostrare il punto fisico. Non inventare un punto world visibile per aggirare il clipping. Applicare la conseguenza soltanto dopo prova di presentazione del frame di contatto, con race/istanze ancora valide e guard live finale. Timeout500 ms → esito annullato, nessun danno tardivo. Questa latenza è esplicita; misurarla. Se il profilo produce sistematicamente cancellazioni, non è qualificato: non convertire il mancato ack in un hit.

Non aggiungere un secondo archivio depth illimitato. Conservare al massimo 8 riferimenti candidati di metadati per la singola transizione, con le lease esistenti quando serve depth; imporre i limiti SC1 di frame/depth/memoria. Eviction o oracle unavailable annullano l'opportunità. Il feedback diagnostico registra il motivo; il ring degli eventi non diventa una coda gameplay.

La prova di visibilità della sorgente deve corrispondere a un punto riconoscibile del kart/socket e del suo marker nel canale effettivo, con la policy depth SC1 conservativa e clipping coerente, non solo a un segmento Bullet libero. World occlusion e renderer sono controlli complementari. Visuale, nodo e allarme sonoro vengono emessi insieme; audio deduplicato per threatId, mai unica informazione. L'effettiva percezione umana resta da verificare.

## 9. Impulso: intercettazione predittiva e collisione continua

L'impulso è un oggetto SC3 a velocità costante senza gravità, non una powerup vanilla. Una volta lanciato non insegue il kart; può mancare. Origine A = socket dell'AI ricavato dal **corpo fisico e dai child transform del compound**, non dal suo bounding box visuale centrato sull'origine. Destinazione B = centro fisico del corpo umano. Non riaprire il bug baricentro SC2. Calcolare un socket esterno al corpo sorgente lungo la direzione di lancio; se spostarlo richiede attraversare geometria/altro kart, non lanciare. Verificare la soluzione definitiva dopo lo spostamento.

### 9.1 Soluzione analitica

Velocità umana v, velocità impulso s=40 m/s, r=B−A. Non ereditare la velocità dell'AI: la velocità è definita in world space. Cercare il primo t>0 con `|r+v*t|=s*t`:

```text
a=dot(v,v)−s²; b=2*dot(r,v); c=dot(r,r)
a*t²+b*t+c=0
```

Calcolare in double. Caso quasi lineare: `abs(a)≤1e−12*max(dot(v,v),s²,1)`; se `abs(b)>1e−12*max(abs(b),1)` nelle unità SI del profilo, t=−c/b, altrimenti nessuna soluzione (c≈0 è sovrapposizione e rifiuta il lancio). Discriminante D=b²−4ac; tolleranza numerica relativa `1e−12*max(b²,abs(4ac),1)`. D<−tolleranza → nessuna soluzione; −tolleranza≤D<0→D=0. Non è una tolleranza sulla collisione.

Per evitare cancellazione catastrofica: `q=−.5*(b+copysign(sqrt(D),b))`; radici `q/a` e `c/q`. Se q è zero, usare la radice doppia `−b/(2a)`. Scartare NaN/Inf e t≤0; scegliere il minimo positivo. Poi ammettere soltanto `t∈[ceilTicks(350000)/F, floorTicks(900000)/F]` (a120Hz: [.35,.90]). Direzione `d=(r+v*t)/(s*t)`; verificare norma entro tolleranza relativa1e−9, poi normalizzare. Non arrotondare t a tick prima del calcolo: la collisione usa tempo continuo entro lo step.

| r (m), v (m/s) | t (s) | d atteso |
|---|---:|---|
| (0,0,20), (0,0,0) | .5 | (0,0,1) |
| (0,0,20), (0,0,10) | .666666666667 | (0,0,1) |
| (0,0,20), (10,0,0) | .516397779494 | (.25,0,.968245836552) |
| (0,0,20), (0,0,−10) | .4 | (0,0,1) |
| (0,0,20), (0,0,40) | nessuno positivo | non lanciare |
| (0,0,20), (0,0,−40) | .25 | soluzione matematica, ma profilo la esclude |

La previsione assume velocità costante; **non** è prova di hit né garanzia contro accelerazione/sterzata. Posizione impulso a tempo τ: `P=A+s*d*τ`, fino al massimo di volo quantizzato del profilo (.90 s a120Hz). Il contatto reale può precedere il centro previsto per effetto dei raggi: non usare t come scadenza del danno. La cancellazione settore/visibilità può far terminare prima il volo.

### 9.2 Collisione senza tunnelling

Proporre un adattatore `IR/scintilla_defense_physics.*`. Un hook minimo, gated Relay/SC3, attorno allo step Bullet raccoglie trasformazioni T0/T1 e collision snapshot. Non muta il mondo e non applica effetti nel ciclo delle collisioni native. Il commit resta nel race loop. Non alterare il timestep globale; se manca una coppia di trasformazioni o si rileva teleport/rescue, cancellare il volo.

Per ogni step: sweep sfera r=.12 dalla posizione precedente alla nuova contro track/statici; per umano e ostacoli dinamici usare cast con entrambe le trasformazioni. Per il compound iterare le forme effettive: `childWorld(t)=bodyWorld(t)*childLocal`, includendo baricentro, orientamento, margini Bullet e scaling della forma già costruita. Non sostituire il telaio con marker SC1 r=.35. Usare `btContinuousConvexCollision` per convex-convex/plane; per static mesh usare il percorso `btCollisionWorld::objectQuerySingle/convexSweepTest` della versione inclusa. Per dinamici non supportati rilevanti nel broadphase: `collision_uncertain`, annullamento, mai attraversamento silenzioso.

Il cast Bullet assume interpolazione lineare del centro e rotazione dell'intervallo; non è dimostrazione di traiettoria fisica arbitraria. Campionare ogni step1/F. Per child offset, l'interpolazione dei centri estremi non è esattamente l'arco del compound: suddividere l'intervallo con bounding error `e=rho*(1−cos(phi/(2n)))`, dove rho è la distanza del centro child dal centro del corpo e phi l'angolo ruotato nello step. Richiedere e≤0,001 m e `n≤16`; in caso contrario annullare. Formula `n≥phi/(2*acos(1−e/rho))`, ceil, almeno1; rho=0 o phi=0→1. Per rho=1 m, phi=30°, e=.001: **n=6**, errore≈.00095178 m. Ogni estremo child va ricavato dal body interpolato, non interpolando una seconda volta i centri.

Usare l'errore residuo come incertezza geometrica conservativa: sweep ostacoli con sfera r+e+ε, conferma di hit umano con sfera r−e−ε (r−e−ε>0). Il primo è una sovracopertura, il secondo un contatto conservativo; casi nell'intervallo non risolto non danno danno. Non aggiungere e come tolleranza favorevole al bersaglio. ε deve coprire la soglia di arresto del cast incluso (il sorgente usa `radius=0.001f`) e l’errore numerico validato alla scala del mondo; non assumere che il booleano del solver dimostri penetrazione. Verificare il contatto conservativo e il bound di separazione con la narrowphase alle trasformazioni dell’istante candidato. Se non si riesce a delimitare l’errore, se la convergenza è incerta o la frazione non è finita, annullare. Questo controllo numerico va provato con Bullet reale nella qualifica, non è certificato dall’oracle a sfere.

Confrontare tutte le frazioni di contatto nello step, non fermarsi sul primo candidato iterato. Ostacolo prima/pari all'umano → WorldBlocked. Un pareggio numericamente indistinguibile è bloccato. Chiave finale per eventi duplicati: race+threat+contact; al massimo un effetto. Altri kart intercettano fisicamente e dissipano il pulse senza ricevere danni SC3; il mittente viene escluso soltanto dopo aver validato socket esterno senza sovrapposizione. L'ordine dei candidati non cambia il risultato.

Oracle semplificato per le regressioni matematiche, **non collisione di produzione**: due sfere, `r0=P0−B0`, `w=(P1−P0)−(B1−B0)`, R=rPulse+rTarget. Risolvere `dot(w,w)u²+2dot(r0,w)u+dot(r0,r0)−R²=0`, u∈[0,1]. Esempio P0=−1, P1=1, B0=B1=0, R=.2 → ingresso u=.4; entrambi gli endpoint sono fuori ma il colpo esiste. Target B0=1,B1=−1 contro impulso −1→1 → ingresso u=.45. Un test solo sugli endpoint perde entrambi.

## 10. Effetti sul kart, boost e lifecycle

Impatto proposto: riduzione **una tantum del 15% della componente tangenziale** della velocità, senza animazione, rotazione forzata, spostamento o cambio camera. Applicare solo su kart controllabile a terra, senza rescue/animazione o boost nativo incompatibile; se lo stato cambia prima del commit, annullare con motivo leggibile.

Con U normalizzato (alto rispetto a gravità), `vn=U*dot(v,U)`, `vt=v−vn`, `v'=vn+.85*vt`. Velocità angolare invariata. Per v=(10,−2,20), U=(0,1,0): v'=(8.5,−2,17). Energia cinetica tangenziale proporzionale al quadrato: fattore .7225, riduzione27,75%; non equivale a una perdita del27,75% del tempo gara. Il profilo non impone arresto né stun. La perdita effettiva di tempo dipende da accelerazione e pista, da misurare.

Il minimo velocità del veicolo non deve ripristinare immediatamente la velocità precedente: nell'adattatore SC3 usare gli accessor del veicolo e la stessa precauzione `resetMinSpeed/setMinSpeed` del codice nativo, con minimo tangenziale ridotto. Non chiamare `Kart::adjustSpeed`, non scalare gravità o velocità angolare. Non cambiare categorie MaxSpeed/serializzazione per questa riduzione istantanea. Verificare e rifiutare l'effetto se un boost nativo attivo lo renderebbe ininfluente; il direttore evita già candidati con umano in tale stato. Pubblicare `impact_applied` soltanto dopo applicazione effettiva, con idempotency key e protezione600 tick. Non usare invulnerabilità globale STK, che cambierebbe altri meccanismi.

Boost: nodo tool con acquisizione continua0,5 s, nuova pressione obbligatoria per ogni uso, owner/risorse di squadra. Se ammissibile: `instantSpeedIncrease(MS_INCREASE_ZIPPER, 2.0, 2.0, 0.0, durationTicks, fadeTicks)` come proposta iniziale (m/s, m/s, forza extra0); durata1 s, fade0,5 s. Prima validare kart a terra, in marcia avanti, non animato, nessun zipper/nitro/altro speed increase incompatibile e nessuna minaccia attiva. Non sovrascrivere un boost da bonus box e non sommare più richieste. Riservare costo25 durante il comando, confermarlo e avviare cooldown solo con applicazione riuscita; fallimento libera la riserva ma non autorizza retry automatici dello stesso held. Nessun accesso a `m_engineer_boost_charge`, nitro kart o carico missione.

Il nodo boost comunica `boost_ready`, `boost_cooldown`, `defense_reserve_protected`, `threat_active` o `vehicle_not_eligible` prima della pressione; costo e parte riservata sono visibili. Difesa: `sector_busy`, `reinforcing`, `blocked`, `release_required`, `energy_depleted`, `overheated`, `wrong_sector`. Feedback persistenti separati dagli esiti primari; snapshot source include owner, threatId, sector, epoch e motivo. C++→DTO→shader devono concordare sui codici; i testi diagnostici si localizzano lato host senza introdurre un sistema di testo nuovo nel renderer.

| Evento lifecycle | Comportamento obbligatorio |
|---|---|
| Pausa/handoff o perdita presentazione | Annullare minaccia/volo/contatto pendente; chiudere tool e richiedere rilascio. Conservare energia, calore, cooldown e protezione nello stesso raceEpoch. Timer gameplay sospesi. |
| Ripresa | Nuovo input valido e nuova minaccia con preavviso completo; nessun lancio da una vecchia deadline. |
| Reprepare/reconnect stessa gara | Conservare risorse e limiti; nuovo inputEpoch, nessun owner/held precedente. |
| Cambio supporter/pilota | Nuove identità, invalidare prove/owner; conservare budget della squadra. Ricalcolare R/H_admit se C cambia; attendere risorse sufficienti senza regalare ricarica. |
| Layout/route/output/clock non più ammessi | Disarmare SC3, bloccare ripresa se profilo SC3 richiesto; mostrare ripristino preparazione. Nessun fallback silenzioso a SC2. |
| Arrivo/abort/world distrutto | Cancellare effetti non committati prima di qualsiasi altra elaborazione; nessun impatto post-arrivo. |
| Nuova gara confermata | Nuovo raceEpoch, E100/H0, nessuna lease/owner/protezione/threat persistente. |

## 11. Contratto API e flusso dell'app

Mantenere intatta la superficie congelata di [integrazione TholusOS](../../../immersive_relay_tholustos_integration.md) e la semantica SC2 esistente. Proporre namespace additivo `/immersive/v1/scintilla/defense/{intent,prepare,state}` con versione schema1. Intent/prepare sono POST autenticati; state è GET sul game thread. Documentare schema rigoroso, limiti e codici errore prima di scrivere il client. Endpoint nuovi non bypassano code, token o controlli di origine esistenti.

Intent SC3 deve persistere lato server **prima** della preparazione input; implica requisito SC2, non lo sostituisce. Preparazione include/rilegge raceEpoch, input session/epoch, roster revision, combat profile revision, output route/context, layoutRevision e capacità rear. Valori stringa per ID64bit; numeri finiti e range stretti per configurazione. Parametri di bilanciamento restano profilo producer versionato, non modificabili liberamente da un client.

`prepare` è idempotente solo con la stessa tupla di autorità; un retry non azzera energia/cooldown. La risposta restituisce profileId `sc3_sector_rear_readonly_v1`, revisione, prerequisiti, capabilities e snapshot. `state` espone configured/required/prepared/blockedReason, E/R/H, canale/owner, threatfase/settore/AI/tempo, cooldown, rear capabilities, statistiche limitate di cancellazione. Non esporre puntatori né campi di fisica mutabili.

Condizione di avvio/ripresa:

```text
baseStartGate
&& (!SC2_required || SC2_prepared_current)
&& (!SC3_required || (SC3_prepared_current && layout/presentation admitted))
```

Applicarla a **tutti** i percorsi reali: Ready pilota, start operatore, resume, retry/ripristino, cambio pilota, reset e avvio automatico esistenti. Il controller server è autoritativo; non basta disabilitare il pulsante. Tentare SC3 con peer vecchio → errore esplicito, nessun downgrade automatico. Uscire da SC3 soltanto attraverso reset/cambio profilo consentito a mondo non avviato; perdere il backend non cancella l'intent.

Host: aggiungere `combatSC3` al modello modo, mostrare scelta e requisiti, inoltrare intent, preparare SC1→SC2→SC3 nella medesima transazione logica con riconvalida dopo ogni await. Tutti i guard attuali `mode == .combatSC2` vanno rivisti semanticamente: alcuni devono diventare «combat richiesto», altri restare specifici SC2. Non usare sostituzione testuale indiscriminata. Conservare la protezione contro risposta asincrona vecchia, cleanup e riarmo. Fallimento SC3 lascia gara bloccata con causa, senza azzerare la policy richiesta.

Non introdurre un nuovo contratto SI5 generico per il solo HUD STK: l'host invia pixel sulla superficie output principale; il producer classifica main/rear/HUD tramite frame/layout. Se si aggiunge un campo opzionale di layout al messaggio, è solo corroborazione e va validato, mai autorità del client sul contesto.

## 12. Rendering, prestazioni e prove

Comporre scena principale, retro, minaccia/impulso e HUD nello stesso output e frame identity. Entrambe le proiezioni usano una sola definizione di layout, basi e codici. Non disegnare indicatori di lock attraverso la pista o dentro regioni coperte. Un bordo di allarme può indicare direzione, ma da solo non soddisfa la visibilità della sorgente. Aggiornamento gameplay a F; rendering alla sua frequenza; nessun timer basato sul numero di draw.

Budget iniziali: una query di visibilità sorgente pendente con coalescenza dell'ultimo candidato; un impulso; un solo set delle sei capture; array fissi per HUD e massimo32 rivali. Retro area `.36*.18*S²=.0648*S²`: **6,48%** del frame quadrato, **3,24%** di un equirect2:1. È percentuale pixel, non promessa di costo GPU: sampleEnvironment può usare blending e più texture fetch. Misurare draw/capture count, memoria, tempo GPU e latenze quando sarà autorizzato, senza assumere «costo zero».

La suite nativa esistente è in `tools/immersive/tests/`, con runner CMake e casi `.inc`. Aggiungere regressioni core, consumer, adapter e host nei framework già adottati, non soltanto scanner di stringhe. Il [calcolatore](verifica_calcoli.py) e [risultati](calcoli.json) di questo piano sono oracle matematici indipendenti, non test dell'implementazione C++/Swift/GLSL.

| ID | Caso obbligatorio | Risultato atteso |
|---|---|---|
| S01 | Preavviso3s/2,5s con ack ritardato e clock interval | Il countdown parte dopo H; mai lancio prima del minimo visibile |
| S02 | Proof futura, altro stream/threat/layout o timeout | Nessun attacco; motivo distinto, risorse non azzerate |
| S03 | No AI, AI ritirata/staggered, fuori range o occlusa | Nessun fallback; annullamento prima dell'effetto |
| S04 | Angoli39/44/49/51°, wrap179/−179 | Classificazione e hysteresis della sezione6 |
| S05 | Difesa71/72tick, rilascio esatto deadline, late proof | Solo continuità completa ancora valida blocca |
| S06 | Miss→hit prima del tick, vecchio epoch dopo rearm | Il negativo resta; nessuna continuità retroattiva |
| S07 | Due controller stesso settore / cambio owner | Una sola forza/costo; niente trasferimento del tempo |
| S08 | Hold per minuti, switch controller, boost spam | Nessuna invulnerabilità/ricarica/reset del calore; riserva protetta |
| S09 | E esatta al costo, E un quantum sotto, H100 | Confini deterministici e feedback persistente |
| S10 | Update segmentato vs singoli tick, F60/120/144 | Stessi integrali; finestre arrotondate come specificato |
| S11 | Retro centro/angoli, W=H e W=2H, resize | Matrice corretta, niente specchio; layout revision invalidata |
| S12 | Primaria su retro/nodo/barra, bordo tra contesti | Nessun tiro nella scena coperta o difesa da miss |
| S13 | Foreground occludente e sorgente rear | Non ammessa; niente epsilon depth favorevole |
| S14 | Radici balistiche lineari/doppie/negative/NaN | Soluzioni della tabella e rifiuto sicuro |
| S15 | Pulse attraversa corpo fra endpoint, body ruota con offset | CCD e compound reali, casi numerici della sezione9 |
| S16 | Muro appena prima/pari all'umano, candidati permutati | WorldBlocked invariato; niente effetto oltre occludente |
| S17 | Contact ripetuto, due callback, proof scaduta | Al massimo un effetto; timeout senza danno |
| S18 | Pausa/resume/reprepare in ogni fase | Transienti annullati, risorse/protezioni conservate |
| S19 | Ready/start/resume con SC3 richiesta non pronta | Server e host bloccano tutti i percorsi |
| S20 | Impatto a terra, in aria, rescue, zipper attivo | Solo caso ammesso; nessun cambio verticale/angolare/camera |
| S21 | SC3 disattivata, SC2 sola, vanilla e telefoni legacy | Comportamenti precedenti e limiti SC1/SC2 preservati |
| S22 | Fine gara/reset con hit in attesa, roster C1↔C5 | Nessun effetto vecchio; reset solo nel nuovo raceEpoch |
| S23 | Output a frame rate basso, GPU lenta, cache piena | Cancellazioni esplicite, memoria limitata, mai danno senza evidenza |
| S24 | Fonte rear esce dal canale durante lock/flight | Annullamento e nuovo preavviso necessario |

Fixture integrate: almeno due AI, due sorgenti con eventi riordinati, camera e corpi in movimento, cambi frequenza render e latenza ottica. Non limitarsi a camera statica/una sorgente. Definire nomi dei test e aspettative prima dell'implementazione di ciascun passo.

## 13. Ordine di lavoro per l'agente implementatore

| Passo | Consegna concreta | Ricontrollo prima di proseguire | Complessità |
|---|---|---|---|
| 0 — Baseline | Worktree con WIP verificato, manifest, lettura AGENTS/design/piano; inventario gate e test attuali | Confermare versioni, hook e regressioni SC1/SC2; nessuna modifica estranea | Media |
| 1 — Contratto e modello | Schema SC3, tipi identità, stati, layout e parametri, API additive documentate | Nessun endpoint congelato cambia significato; tutte le invalidazioni hanno un esito | Medio-alta |
| 2 — Core risorse/difesa | `scintilla_defense.*`, tick, fixed point, esclusività, latch, riserve, cooldown | S05–S10, trace numeric equivalenti ai calcoli; nessuna dipendenza GL/Bullet | Alta |
| 3 — Layout/retrovisore | Layout condiviso, formule forward/inverse, compositing nelle due proiezioni, maschera input | S11–S13, contorni/badge/firma ottica disgiunti; nessun secondo capture | Alta |
| 4 — Input contestuale | Routing storico per pixel; difesa/boost esclusivi rispetto a SC2, feedback source persistente | S06/S07/S12, nessuna perdita sticky cancellata né target HUD invisibile | Alta |
| 5 — Direttore e preavviso | Selezione AI vera, settori, prove presentazione, timer, cancellazioni | S01–S04/S24; mai partenza countdown da frame soltanto prodotto | Alta |
| 6 — Impulso e collisione | Solver predittivo, hook pre/post fisica, CCD compound/static/dynamic, rendering pulse | S14–S17; baricentro reale e primo ostacolo autoritativo | Molto alta |
| 7 — Impatto e boost | Adattatori stretti, conferma effetto, risorse condivise, protezione e camera | S08/S17/S20; nessuna chiamata explosion/bubblegum o costo su effetto rifiutato | Alta |
| 8 — Lifecycle/server | Intent/prepare/state, start/resume gate, reset/handoff/reconnect | S18/S19/S22 e tutti gli ingressi Ready, non solo il percorso operatore | Alta |
| 9 — Host e UX operatore | Modalità SC3, transazione preparazione, DTO, capacità rear, errori e recupero | Risposte vecchie ignorate; profilo SC3 richiesto resta tale dopo errore | Medio-alta |
| 10 — Audit trasversale | Test scritti/registrati, budget, protocollo e documenti riconciliati | S01–S24 e regressioni precedenti, distinguendo eseguito/non eseguito | Alta |
| 11 — Qualificazione futura | Build/test nativi, receiver/GPU e venue/playtest, soltanto se autorizzati | Matrice design D01–D04/G03/G04/H/R con evidenze reali e misure comfort | Alta; richiede ambiente/hardware |

Non saltare dal core direttamente all'app: il percorso visuale/input deve esistere prima di dichiarare operativa la difesa. Nel passo6 non sostituire CCD con una sfera finale per risparmiare lavoro. Nel passo8 non replicare il bug «Ready senza preparazione». Dopo ogni passo: diff circoscritto, lettura di chiamanti/consumatori, casi limite pertinenti e registrazione onesta delle verifiche consentite. Fermarsi alla consegna SC3, non avviare SC4.

## 14. Criteri di chiusura e consegna

SC3 è **implementata a livello sorgenti** quando tutti i passi0–10 sono coerenti, il percorso dalla scelta host fino al risultato/riarmo esiste, e nessuna nuova funzione essenziale resta scollegata. Con divieto build, tale stato non equivale a software compilato o qualificato.

SC3 è **qualificata in runtime** solo dopo esecuzione autorizzata di test nativi e prova del flusso reale: scelta SC3→preparazione→Ready→preavviso front/rear→difesa→release→nuova minaccia→hit/miss/occlusione→boost→pausa/ripresa→arrivo/reset. Includere configurazione/revisioni, frame presentati, latenza ottica, perdita tracking, consumo GPU e comportamento degli stessi input nel rendering reale.

Accettazione venue: sorgente posteriore riconoscibile nel retro, nodo raggiungibile senza gesti ampi, istruzione di rilascio comprensibile, una persona riesce a passare da offesa a difesa, nessuna raffica incontrollabile, bilanciamento di energia/heat/boost verificato e camera confortevole. Misurare tempo di reazione/acquisizione, errori di contesto, cancellazioni tecniche, impatti, blocchi e tempo gara effettivamente perso. Numeri del piano modificabili solo con profilo revisionato e test aggiornati, non per nascondere errori di mapping.

Consegna dell'implementatore: elenco file e comportamento, regressioni aggiunte, verifiche eseguite/non eseguite, limiti del profilo (`rear_targetable_view=false`, intercetto disabilitato), istruzioni per futura qualifica. Non dichiarare test GPU, hardware, comfort o completa missione Scintilla dal solo codice. Il presente piano include calcoli riproducibili e baseline, non quell'evidenza.

### Verifiche eseguite su questa consegna

Eseguito `python3 -B verifica_calcoli.py`: **10 gruppi di controlli matematici superati**, con risultati in `calcoli.json`. Verificati collegamenti locali, 24 scenari di regressione specificati, coerenza delle tabelle e hash di36 riferimenti sorgenti/documentali. Il controllo comprende tempi a60/120/144Hz, riserve e integrali, settori, mapping rear diretto/inverso, ingombri HUD, radici balistiche, due esempi CCD a sfere, errore del child offset, effetto tangenziale e minimo preavviso ottico. Nessuna build, esecuzione nativa, prova GPU o hardware; nessuna implementazione SC3.
