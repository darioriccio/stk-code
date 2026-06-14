# Immersive Relay Adaptive Difficulty Director Brief

Questo documento e' un brief operativo per una chat/agente che deve progettare
e implementare la difficolta' adattiva di Immersive Relay / modalita'
salagiochi.

## Leggere Prima

Prima di modificare codice, leggere questi documenti del progetto:

- `docs/AGENTS.md`
- `docs/supertuxkart_immersive_relay_master_spec.md`
- `docs/immersive_relay_upstream_sync.md`
- `docs/immersive_relay_phase2_api_contract.md`
- `docs/immersive/repo_audit.md`

In particolare:

- preservare SuperTuxKart vanilla;
- mantenere il fork sottile e facile da rebase;
- mettere ogni feature relay dietro flag CMake/runtime;
- Swift/macOS resta autorevole per configurazione venue/sessione;
- STK deve ricevere configurazione locale validata, non decidere flussi venue
  globali da solo;
- la modalita' salagiochi corrente e' attivata da `--relay-arcade-ui`.

## Contesto Attuale

La modalita' relay/arcade ha gia':

- `RelaySessionController`
- `RelayRaceMode`
- `RelayPilotRotation`
- cambio pilota lap-based;
- freeze/hold durante handoff;
- local API;
- player count 1-6;
- giri automatici multipli dei piloti;
- launcher direct keyboard race;
- launcher sala giochi: kart -> track -> champions/high scores -> race.

Nota importante: aprire la `.app` direttamente resta vanilla-like. Per testare
relay serve un launcher/command con `--immersive-relay`. Per la sala giochi serve
anche `--relay-arcade-ui`.

## Obiettivo

Implementare un sistema di difficolta' adattiva real-time per rendere la gara
entusiasmante per giocatori molto diversi, senza far percepire che il gioco sta
barando.

Nome consigliato:

```text
ArcadeDifficultyDirector
```

oppure, se si preferisce tenerlo dentro il namespace relay:

```text
RelayDifficultyDirector
```

La raccomandazione e': implementare un director deterministico e telemetrico,
non un sistema ML runtime.

## Stato Dell'Arte Riassunto

Dynamic Difficulty Adjustment (DDA) e' maturo come area di ricerca: player
modeling, performance metrics, flow, engagement, adattamento real-time.
Pero' nei racing game il caso piu' rischioso e' il rubber-banding evidente:
rallentare chi e' davanti o potenziare troppo chi e' dietro puo' creare gare
ravvicinate ma danneggia mastery, fairness e fiducia.

La direzione giusta per questo progetto:

- no "AI magica" runtime;
- no vittoria garantita;
- no boost impossibili;
- si' a piccoli aggiustamenti graduali;
- si' a assist fisici e recovery;
- si' a pacing degli AI;
- si' a eventi crew/director piu' o meno intensi;
- leaderboard separate per adaptive/fixed.

Fonti esterne utili gia' considerate nell'audit:

- Hunicke, "The Case for Dynamic Difficulty Adjustment in Games" - DDA deve
  mantenere feedback coerente ed evitare cheating percepito.
- Rietveld et al., "Circuit-Adaptive Challenge Balancing in Racing Games" -
  rubber-banding puo' sembrare ingiusto/paternalistico; alternative migliori
  adattano la sfida in modo meno diretto.
- Forza Motorsport support docs - difficolta' e assist sono modulari e
  configurabili.
- Gran Turismo Sophy / Sony AI - deep RL e' possibile ma e' un progetto enorme;
  in prodotto viene comunque reso regolabile per vari livelli.

## Vincoli Di Design

Il director deve:

- mantenere la gara viva, non garantire la vittoria;
- evitare che gli umani siano staccati e frustrati;
- evitare che vincano con distacco enorme e si annoino;
- cambiare lentamente, con smoothing/hysteresis;
- non ribaltare una gara negli ultimi metri;
- non far sembrare gli AI "imbattibili" o "finti";
- non penalizzare pesantemente un team che sta giocando bene;
- tenere conto degli handoff: subito dopo un cambio pilota serve tolleranza.

Target di esperienza:

```text
La gara deve spesso sentirsi recuperabile, ma non regalata.
```

Metriche indicative:

- gap in secondi dal gruppo;
- posizione del kart umano;
- trend posizione negli ultimi 15-30 secondi;
- rescue/off-track/fall/water count;
- collisioni importanti;
- velocita' media rispetto a baseline pista/giro;
- lap progress;
- handoff appena concluso;
- numero piloti;
- difficolta' sessione richiesta da Swift/preset venue.

## Cosa Non Fare

Non implementare:

- deep learning runtime;
- training AI;
- modifiche globali alla fisica vanilla;
- modifiche non flaggate;
- teletrasporto kart;
- velocita' AI impossibili;
- nerf evidente del player quando sta vincendo;
- UI iniziali personalizzate: verranno create dopo;
- classifiche mischiate tra adaptive e fixed.

## Strategia Consigliata

Usare tre livelli di adattamento, dal meno controverso al piu' visibile.

### 1. Assist Al Kart Umano

Usare assist leggeri e fisici:

- recovery piu' morbido dopo rescue;
- stabilizzazione minima se il kart sta perdendo controllo;
- minor penalita' off-road in casi limitati;
- aiuto anti-acqua/anti-barriera solo in modalita' arcade;
- item/nitro leggermente piu' favorevoli se il team e' molto indietro.

Questi interventi sono piu' accettabili del rallentare apertamente gli
avversari.

### 2. Pacing AI

Regolare AI con limiti morbidi:

- cap velocita' AI;
- probabilita' uso nitro;
- item skill/aggressivita';
- skid/rubber-band parameters gia' esistenti;
- piccoli errori o minore perfezione se gli umani sono troppo lontani;
- AI piu' pulita se gli umani dominano.

Non superare mai limiti fisici credibili.

### 3. Crew/Event Director

Quando la crew esiste:

- se il team soffre, eventi crew piu' semplici o bonus piu' utili;
- se domina, eventi piu' intensi;
- usare narrativa/missioni invece di manipolare troppo la gara.

Questo e' ideale per l'esperienza immersiva: sembra contenuto, non trucco.

## Touchpoint Locali In STK

Punti gia' presenti nel codice:

- `src/race/race_manager.hpp`
  - enum difficolta': `DIFFICULTY_EASY`, `MEDIUM`, `HARD`, `BEST`
  - kart info include `m_boosted_ai` e `m_handicap`
- `src/config/user_config.hpp`
  - `m_difficulty`: 0 easy, 1 medium, 2 hard, 3 supertux
  - `m_per_player_difficulty`
- `data/stk_config.xml`
  - parametri AI per easy/medium/hard/best
  - commenti e parametri gia' collegati a rubber-banding/skid probability
- `src/modes/immersive_relay/relay_session_controller.*`
  - stato/sessione/snapshot/local API
- `src/modes/immersive_relay/relay_race_mode.*`
  - update race, lap progress, handoff handling
- `src/modes/immersive_relay/crew_event_director.*`
  - futuro layer per eventi/crew intensity

Il director dovrebbe vivere sotto:

```text
src/modes/immersive_relay/
```

Non introdurre sorgenti relay fuori da questa directory.

## API / Config Da Aggiungere

Proposta per `POST /immersive/v1/session/start`:

```json
{
  "adaptive_difficulty": {
    "enabled": true,
    "profile": "arcade_balanced",
    "visibility": "leaderboard_bucket",
    "min_intensity": 0,
    "max_intensity": 100
  }
}
```

Oppure, se si vuole tenere semplice:

```json
{
  "adaptive_difficulty_enabled": true,
  "adaptive_difficulty_profile": "arcade_balanced"
}
```

Proposta per `/immersive/v1/state`:

```json
{
  "adaptive_difficulty_enabled": true,
  "adaptive_difficulty_profile": "arcade_balanced",
  "adaptive_difficulty_intensity": 42,
  "adaptive_pressure": "recovering",
  "human_gap_ms": 6800,
  "human_position": 5,
  "assist_level": 2,
  "ai_pace_level": -1,
  "leaderboard_bucket": "arcade_adaptive"
}
```

La config deve poter essere decisa da Swift. STK puo' avere default locali solo
per test.

## Leaderboard / Record

Le classifiche sono una parte importante della gamification. Non eliminarle.

Pero' separare almeno:

```text
arcade_adaptive
arcade_fixed
solo_no_assist
```

Se una gara usa assist adattivo pesante, non deve finire nella stessa classifica
di una gara fixed/expert. Questo mantiene la competizione onesta.

## Algoritmo Deterministico Proposto

Schema iniziale:

```text
sample every 250-500 ms
compute performance window over 15-30 s
compute target pressure/intensity
apply hysteresis
rate-limit changes
apply small deltas to assist/AI/event director
log decisions
expose current state in API
```

Stati possibili:

```text
neutral
recovering
struggling
dominating
final_lap_lock
handoff_grace
```

Regole indicative:

- `handoff_grace`: 5-10 s dopo handoff, niente aumento difficolta'.
- `struggling`: ultimo posto + gap grande + errori ripetuti -> assist umano e
  lieve calo AI.
- `recovering`: dietro ma stabile -> piccoli aiuti, nessun intervento brusco.
- `dominating`: primo con gap grande per >30 s -> AI piu' pulita/rapida, eventi
  crew piu' intensi.
- `final_lap_lock`: niente grandi cambi negli ultimi tratti, per evitare furia
  da "mi ha rubato la vittoria".

## Test Minimi

Build:

```bash
cmake --build cmake_build_phase1_vanilla --parallel 8
cmake --build cmake_build_relay_phase1 --parallel 8
tools/immersive/check_thin_fork.sh
```

Smoke API:

- `adaptive_difficulty_enabled=false` deve lasciare comportamento invariato.
- `adaptive_difficulty_enabled=true` deve comparire in `/state`.
- 1 pilota: niente handoff, adaptive puo' agire solo come solo/AI pacing.
- 3 piloti: giri restano 6.
- 6 piloti: giri restano 12.
- durante handoff non deve alterare freeze/hold.

Test logici:

- se player e' ultimo con gap crescente per 30 s, intensity aumenta verso
  recovery;
- se player domina per 30 s, intensity aumenta verso pressure;
- oscillazioni rapide non devono causare flip-flop;
- final lap lock impedisce cambi estremi;
- leaderboard bucket cambia quando adaptive e' attivo.

## Acceptance Criteria

La PR/patch e' accettabile solo se:

- vanilla build passa;
- relay build passa;
- thin-fork check passa;
- adaptive disabled = comportamento precedente;
- adaptive enabled e' runtime-gated e API-visible;
- nessun nuovo menu iniziale custom;
- nessun cambiamento non richiesto a rendering immersivo;
- decisioni del director sono loggate in modo leggibile;
- documentazione aggiornata con configurazione e rischi noti.

## Output Atteso Dalla Chat Implementatrice

Alla fine, rispondere con:

- summary;
- files changed;
- build/test commands run;
- API fields added;
- behavior verified;
- known risks;
- follow-up tuning/playtest checklist.
