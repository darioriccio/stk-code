# SuperTuxKart Immersive Relay — Master Spec definitivo per agente AI

**Versione:** 3.0 — crew gameplay + arcade/LBE + multi-ambiente
**Data:** 2026-06-08
**Destinatario:** agente AI / sviluppatore incaricato della fork di SuperTuxKart
**Contesto:** app Swift/macOS proprietaria che gestisce cupola, proiezione, esperienze, escape room, telefoni come controller e timeline evento
**Primo target:** macOS su Mac mini M1, volante + pedali per il pilota, fino a 5 telefoni per la crew, output fulldome via Syphon verso l'app Swift
**Target futuri:** Windows/Spout, equirettangolare, LED wall, cilindri, CAVE/wall atlas, profili VR/OpenXR in una fase successiva

---

## 0. Sintesi esecutiva

Creare una fork sottile di **SuperTuxKart** che trasformi il gioco in una attrazione cooperativa immersiva da sala giochi.

Il prodotto non deve essere “SuperTuxKart proiettato in cupola”. Deve essere una modalità nuova:

```text
SuperTuxKart Immersive Relay
```

La prima esperienza vendibile è:

```text
Dome Relay Crew Mode
```

Formula:

```text
1 pilota attivo guida con volante + pedali
fino a 5 supporter usano telefoni come console di ruolo
il pilota cambia a ogni checkpoint o segmento
la crew mantiene il kart performante e apre opportunità di gara
output immersivo vero fisheye/equirect verso l'app Swift
```

La crew non deve sembrare “bonus appiccicato”. Deve essere il cuore della modalità. Il pilota deve guidare, ma deve anche parlare con la crew:

```text
“Apritemi la porta sinistra.”
“Boost dopo la curva.”
“Scudo adesso.”
“Qual è il percorso sicuro?”
“Sto perdendo grip, mi serve stabilizzazione.”
```

Se il pilota può giocare quasi uguale senza crew, il design è fallito.

---

## 1. Fonti e lezioni di design usate per questa specifica

Questa specifica incorpora pattern da giochi cooperativi asimmetrici e party/LBE già collaudati. L'agente deve usarli come riferimento di design, non come asset o codice.

### 1.1 Keep Talking and Nobody Explodes

Pattern utile:

- un giocatore vede/interagisce con l'oggetto critico;
- gli altri hanno informazioni/istruzioni che il giocatore principale non ha;
- la comunicazione è obbligatoria;
- round brevi, tesi, rumorosi e memorabili.

Applicazione a Immersive Relay:

```text
Pilota = vede la pista in FPV e deve guidare.
Crew = ha strumenti/informazioni parziali che cambiano la pista, boost, difesa, scelta percorso.
```

Rischio da evitare:

```text
La crew non deve diventare un manuale statico.
Deve avere azioni fisiche/di gioco, cooldown, rischio, timing e feedback visibile.
```

Fonte:

- https://www.nintendo.com/us/store/products/keep-talking-and-nobody-explodes-switch/
- https://www.wired.com/2014/09/keep-talking-nobody-explodes

### 1.2 Spaceteam

Pattern utile:

- telefoni come pannelli personali;
- comandi temporizzati;
- istruzioni spesso destinate ad altri;
- caos sociale controllato;
- round molto brevi e ad alta energia.

Applicazione:

```text
Ogni telefono è una console di ruolo, non un mini-gioco separato.
Le richieste devono generare parlato naturale tra i giocatori.
```

Esempio:

```text
Telefono Navigator: “GATE BLU TRA 7s — chiedi Hacker di aprirlo.”
Telefono Hacker: bottoni Gate Rosso / Gate Blu / Gate Giallo.
Pilota: vede fisicamente il bivio arrivare.
```

Fonte:

- https://apps.apple.com/gb/app/spaceteam/id570510529
- https://playspaceteam.com/

### 1.3 Artemis Spaceship Bridge Simulator

Pattern utile:

- ogni giocatore ha una stazione specializzata;
- il divertimento nasce dalla comunicazione, non dalla complessità individuale;
- il capitano/coordinatore può essere utile, ma alcune stazioni possono risultare meno attive se progettate male.

Applicazione:

```text
Ogni ruolo deve avere azioni ricorrenti e conseguenze visibili.
Nessun ruolo deve essere “guarda la mappa e ogni tanto parla”.
```

Fonte:

- https://www.wired.com/2010/11/artemis-spaceship-bridge

### 1.4 Lovers in a Dangerous Spacetime

Pattern utile:

- tutti i giocatori presidiano sistemi diversi dello stesso veicolo;
- ogni ruolo è importante;
- nessuno deve essere lasciato indietro;
- ruoli e stazioni creano urgenza locale ma obiettivo comune.

Applicazione:

```text
Il kart è trattato come una “nave” condivisa.
Il pilota controlla guida e traiettoria.
La crew controlla sistemi del kart e della pista.
```

Fonte:

- https://store.steampowered.com/app/252110/Lovers_in_a_Dangerous_Spacetime/
- https://www.nintendo.com/us/store/products/lovers-in-a-dangerous-spacetime-switch/

### 1.5 Overcooked

Pattern utile:

- azioni semplici;
- interazioni emergenti complesse;
- pressione temporale;
- comunicazione continua;
- caos leggibile e divertente.

Applicazione:

```text
Ogni azione supporter deve essere semplice.
La complessità nasce da timing, priorità e coordinazione con il pilota.
```

Fonte:

- https://www.playstation.com/en-mt/games/overcooked-/
- https://www.wired.com/story/overcooked-2-review

### 1.6 Mario Kart: Double Dash — lezione negativa e positiva

Pattern positivo:

- due persone sullo stesso veicolo è una fantasia comprensibile;
- separare guida e oggetti può funzionare.

Lezione negativa:

- se il passeggero “fa solo item ogni tanto”, può sembrare marginale;
- la nostra crew deve avere compiti più frequenti, più visibili e più interdipendenti.

Applicazione:

```text
Non basta un “passeggero che lancia oggetti”.
Serve un sistema crew con ruoli multipli, finestre di azione e dipendenze reali.
```

Fonti:

- https://www.mariowiki.com/Mario_Kart%3A_Double_Dash%21%21
- https://www.reddit.com/r/mariokart/comments/177dpsg/i_miss_mario_kart_double_dash_i_wish_they_would/

### 1.7 Ricerca su co-op/asimmetria

Pattern da applicare:

- ruoli asimmetrici;
- informazione asimmetrica;
- interdipendenza progettata;
- feedback diverso per giocatori diversi;
- tempi stretti per evitare quarterbacking;
- obiettivo comune con responsabilità separate.

Fonti:

- Game Design Patterns for Collaborative Player Interactions — https://dl.digra.org/index.php/dl/article/view/639
- To Asymmetry and Beyond! — https://uwaterloo.ca/touchlab/references/asymmetry-and-beyond-improving-social-connectedness
- Exploring Asymmetry of Information in Cooperative Games — https://researchportal.ulisboa.pt/en/publications/exploring-asymmetry-ofinformation-incooperative-games/
- You have got a different friend in me — https://journals.sagepub.com/doi/full/10.1177/2042753018757757

### 1.8 LBE / sala giochi immersiva

Pattern operativo:

- esperienze immersive vendute a slot;
- massimo 6 persone è un formato ricorrente nelle sale immersive;
- sessioni più lunghe esistono, ma per pay-per-play arcade è meglio puntare a partite brevi, ripetibili, con reset rapido.

Fonti:

- Immersive Gamebox: massimo 6 persone per stanza — https://immersivegameboxhelp.zendesk.com/hc/en-gb/articles/5055793545375-Can-I-have-another-Gamebox-or-room
- Immersive Gamebox: 30/60 minuti e multi-room sincronizzato — https://immersivegameboxhelp.zendesk.com/hc/en-gb/articles/5472991975199-How-are-group-bookings-and-game-options-managed-for-large-parties-at-Immersive-Gamebox
- Zero Latency: breakdown 15/30 min — https://zerolatencyvr.zendesk.com/hc/en-us/articles/9503761347727-How-do-the-Zero-Latency-sessions-work-Small-Scale-Arena

---

## 2. Principi di design non negoziabili

### 2.1 La crew deve essere indispensabile ma non frustrante

Il pilota deve poter guidare e sopravvivere a livello base, ma per ottenere il punteggio alto, gli shortcut, la stabilità, il boost e il finale migliore deve dipendere dalla crew.

```text
Senza crew:
  il kart finisce lentamente, perde opportunità, subisce hazard, fa punteggio basso.

Con crew coordinata:
  shortcut aperti, boost chain, scudi perfetti, target rimossi, grip stabile, punteggio alto.
```

Non usare fallimenti punitivi tipo “se il supporter sbaglia, il kart si ferma per 10 secondi”. Usare penalità morbide:

```text
perdita boost
perdita grip temporanea
traiettoria più lunga
AI che recupera
scudo non disponibile
score multiplier perso
```

### 2.2 Ogni ruolo deve avere quattro cose

Ogni ruolo supporter deve avere:

```text
1. informazione unica
2. azione unica
3. timing importante
4. feedback visibile sulla cupola
```

Se una di queste manca, il ruolo sembra finto.

### 2.3 I telefoni non sono puzzle autonomi

I telefoni non devono rubare attenzione alla cupola.

Telefono corretto:

```text
bottoni grandi
slider discreti
cooldown
messaggio ruolo
feedback aptico/audio
informazione parziale
```

Telefono sbagliato:

```text
mini-mappa completa
minigioco lungo
testo da leggere per 30 secondi
puzzle indipendente non legato alla guida
```

### 2.4 La comunicazione deve essere naturale

Il gioco deve generare frasi naturali:

```text
Navigatore: “Tra 5 secondi scegli sinistra.”
Hacker: “Posso aprire solo il gate blu.”
Ingegnere: “Boost pronto ma motore caldo.”
Defender: “Scudo frontale in cooldown.”
Gunner: “Ho libero il drone destro.”
Pilota: “Mi serve subito il gate, arrivo veloce.”
```

Non devono servire spiegazioni da manuale di due pagine.

### 2.5 Anti-quarterbacking

Un giocatore esperto non deve poter comandare tutto da solo.

Usare:

```text
informazioni diverse per ruolo
tempi brevi
cooldown personali
azioni simultanee
telefoni con interfacce diverse
ruoli che cambiano quando cambia il pilota
```

Evitare:

```text
mappa completa su tutti i telefoni
un ruolo “coordinatore” onnipotente
azioni lente dove tutti possono discutere senza pressione
```

### 2.6 Nessun ruolo inattivo per più di 20–30 secondi

Ogni supporter deve ricevere almeno un evento utile ogni 20–30 secondi.

Se in un segmento non serve il suo ruolo, dargli almeno:

```text
preparazione risorsa
scan informativo
cooldown management
score opportunity
combo assist
```

### 2.7 Fail-forward

La sessione deve finire bene anche quando la squadra gioca male.

Finali:

```text
Gold Finish: missione completata + tutti i bonus principali
Silver Finish: missione completata ma con danni/tempo alto
Bronze Finish: arrivo d'emergenza / evacuazione / “reattore contenuto”
Timeout Cinematic: il gioco chiude con un finale scenico e punteggio basso
Operator Abort: chiusura controllata senza rompere la sala
```

Non bloccare mai la timeline della sala giochi.

---

## 3. Prodotto: Dome Relay Crew Mode

### 3.1 Configurazione fisica raccomandata

```text
Mac mini M1
  esegue SuperTuxKart Immersive Relay
  esegue app Swift cupola oppure invia a essa sullo stesso Mac

Volante + pedali
  controlla solo il pilota attivo

5 smartphone
  controllano i ruoli crew

Cupola / ambiente immersivo
  riceve output Syphon dall'app SuperTuxKart
  app Swift fa warp/blend/proiezione
```

La configurazione iniziale deve supportare anche:

```text
1 pilota + 1 supporter
1 pilota + 2 supporter
1 pilota + 3 supporter
1 pilota + 4 supporter
1 pilota + 5 supporter
```

### 3.2 Ruoli massimi

Con 6 partecipanti:

```text
1. Pilota attivo — volante/pedali
2. Navigatore — telefono
3. Ingegnere — telefono
4. Defender — telefono
5. Hacker pista — telefono
6. Gunner/Utility — telefono
```

Quando cambia il pilota, il vecchio pilota entra nella crew e un altro giocatore prende il volante.

### 3.3 Ruoli con meno giocatori

```text
2 giocatori:
  1 pilota
  1 co-pilota combinato: Navigatore + Ingegnere + azioni automatiche assistite

3 giocatori:
  1 pilota
  1 Navigatore/Hacker
  1 Ingegnere/Defender/Gunner

4 giocatori:
  1 pilota
  1 Navigatore
  1 Ingegnere
  1 Defender/Hacker/Gunner

5 giocatori:
  1 pilota
  1 Navigatore
  1 Ingegnere
  1 Defender
  1 Hacker/Gunner

6 giocatori:
  ruoli completi
```

I ruoli combinati devono essere espliciti e progettati, non casuali.

---

## 4. Durata e formato arcade

### 4.1 Preset di durata

```json
{
  "quick_play": {
    "gameplay_seconds": 300,
    "slot_seconds": 480,
    "target": "demo / fiere / alta rotazione"
  },
  "standard_arcade": {
    "gameplay_seconds": 420,
    "slot_seconds": 720,
    "target": "formato principale consigliato"
  },
  "premium_ride": {
    "gameplay_seconds": 660,
    "slot_seconds": 900,
    "target": "esperienza più lunga e scenografica"
  }
}
```

Target iniziale consigliato:

```text
7 minuti gameplay
10–12 minuti slot operativo totale
```

### 4.2 Segmenti e cambio pilota

Per 6 giocatori in 7 minuti:

```text
00:00–00:20 countdown / lancio
00:20–01:20 pilota 1
01:20–02:20 pilota 2
02:20–03:20 pilota 3
03:20–04:20 pilota 4
04:20–05:20 pilota 5
05:20–06:20 pilota 6
06:20–07:00 final sprint / punteggio
```

Ogni segmento deve avere:

```text
1 evento Navigatore
1 evento Ingegnere
1 evento Defender/Hacker/Gunner alternato
1 opportunità combo
1 feedback punteggio
```

### 4.3 Handoff volante

Il cambio pilota deve essere sicuro e scenografico.

A checkpoint:

```text
1. Il kart entra in una zona handoff.
2. Autopilot stabilizza traiettoria per 5–8 secondi.
3. Cupola mostra “CAMBIO PILOTA: Player 3”.
4. Telefono del nuovo pilota vibra/lampeggia.
5. Volante viene preso dal nuovo pilota.
6. Countdown 3–2–1.
7. Controllo restituito al volante.
```

Se il cambio fisico richiede più tempo:

```text
- mantenere autopilot fino a handoff_timeout_seconds;
- se timeout scade, continuare con ghost autopilot a bassa velocità;
- operatore può forzare resume/skip via app Swift.
```

Non far cambiare pilota durante curve strette, salti, collisioni o hazard.

### 4.4 Modalità alternative handoff

Implementare configurabile:

```text
checkpoint_or_timeout      default
checkpoint_only            più arcade/classico
fixed_time_segments        utile se checkpoint design non è pronto
operator_manual_handoff    utile per test/live
single_pilot_demo          nessun cambio pilota, ruoli crew attivi
```

---

## 5. Crew gameplay dettagliato

Questa è la sezione più importante. L'agente deve implementare prima una versione semplice ma reale di queste dinamiche.

### 5.1 Regola d'oro

Ogni azione crew deve attraversare questa pipeline:

```text
Telefono supporter
  → Swift app / session server
  → comando locale a SuperTuxKart
  → modifica reale di kart/pista/AI/HUD
  → feedback visibile in cupola
  → feedback sul telefono
  → evento telemetria/punteggio
```

### 5.2 Tipi di azioni crew

Non usare joystick continui. Usare azioni atomiche:

```text
select
confirm
charge
release
toggle discreto
choose left/right/front/back
choose A/B/C
hold 1–3 secondi
sync tap
```

Ogni azione ha:

```json
{
  "id": "engineer_release_boost",
  "role": "engineer",
  "cooldown_ms": 18000,
  "window_ms": 6000,
  "requires": ["battery >= 30", "engine_heat < 80"],
  "effect": "boost impulse + score multiplier",
  "fail_soft": "minor heat increase",
  "feedback": "cupola: boost trail, telefono: haptic + ACK"
}
```

### 5.3 Ruolo 1 — Navigatore

#### Fantasia di ruolo

Il Navigatore è il co-driver/radar. Sa cosa sta arrivando prima del pilota.

#### Informazione unica

Vede:

```text
prossimo bivio
pericolo imminente
migliore linea / traiettoria suggerita
shortcut disponibile
tipo di gate richiesto
prossimo ruolo coinvolto
```

Il pilota vede la pista, ma non sempre sa quale percorso è vantaggioso.

#### Interfaccia telefono

```text
PROSSIMO: BIVIO IN 9s
Opzioni:
[ SINISTRA — più breve, gate blu ]
[ DESTRA — più sicura, no gate ]

Callout:
[ CURVA STRETTA ] [ SALTO ] [ HAZARD ]

Prossimo ruolo:
“Hacker prepara gate blu”
```

#### Azioni

```text
nav_select_route(route_id)
nav_mark_hazard(hazard_id)
nav_call_pace_note(note_id)
nav_request_role(role_id, reason)
nav_reveal_shortcut(shortcut_id)
```

#### Effetti sulla cupola

```text
frecce luminose sulla pista
traiettoria ghost per 2–4 secondi
ping sul gate giusto
warning grande su hazard
icona ruolo successivo
```

#### Successo

Il pilota prende il percorso giusto, aumenta combo e tempo.

#### Fallimento morbido

Il percorso resta aperto ma meno chiaro; la squadra perde score/combo o prende strada più lenta.

#### Frequenza evento

Almeno ogni 20 secondi.

---

### 5.4 Ruolo 2 — Ingegnere

#### Fantasia di ruolo

L'Ingegnere gestisce motore, boost, temperatura e grip.

#### Informazione unica

Vede:

```text
batteria boost
temperatura motore
grip/stabilità
energia disponibile
prossima finestra boost
rischio overload
```

#### Interfaccia telefono

```text
BOOST BATTERY: 72%
HEAT: 48%
GRIP: OK

Distribuzione:
[ BOOST ] [ RAFFREDDA ] [ STABILIZZA ]

Azioni:
[ CARICA ] [ RILASCIA BOOST ] [ DUMP HEAT ]
```

#### Azioni

```text
engineer_charge_boost(amount)
engineer_release_boost(mode: small|large|chain)
engineer_dump_heat()
engineer_stabilize_grip(duration)
engineer_rebalance_power(target: boost|cooling|grip)
```

#### Effetti sulla cupola

```text
scia boost visibile
effetto tunnel/velocità
indicatore heat vicino al kart
stabilizzazione camera/kart
effetto scintilla se overload
```

#### Successo

Boost in rettilineo o uscita curva, migliore velocità, combo.

#### Fallimento morbido

Heat sale, boost minore, cooldown più lungo.

#### Frequenza evento

Sempre attivo, ma decisioni significative ogni 15–25 secondi.

---

### 5.5 Ruolo 3 — Defender

#### Fantasia di ruolo

Il Defender protegge il kart da hazard, colpi, trappole e collisioni critiche.

#### Informazione unica

Vede:

```text
prossimo hazard direzionale
cooldown scudo
stato integrità kart
minacce AI in arrivo
angolo di impatto previsto
```

#### Interfaccia telefono

```text
MINACCIA: FRONT-RIGHT IN 5s
Scudi:
[ FRONT ] [ LEFT ] [ RIGHT ] [ REAR ]

Utility:
[ CLEANSE ] [ ARMOR PULSE ]
```

#### Azioni

```text
defender_raise_shield(direction)
defender_cleanse_status(status_id)
defender_absorb_hit(threat_id)
defender_repair_armor_small()
defender_emergency_bubble()
```

#### Effetti sulla cupola

```text
arco scudo visibile intorno al kart
deflessione projectile/hazard
suono impatto
flash colore nella direzione protetta
```

#### Successo

Il pilota mantiene velocità e combo.

#### Fallimento morbido

Piccola perdita velocità, wobble, cooldown assist più lungo.

#### Frequenza evento

1 minaccia ogni 25–35 secondi in media; più nel final sprint.

---

### 5.6 Ruolo 4 — Hacker pista

#### Fantasia di ruolo

L'Hacker controlla elementi del tracciato: gate, shortcut, rampe, barriere, semafori.

#### Informazione unica

Vede:

```text
quali gate sono hackabili
codice/colore richiesto
finestra temporale di apertura
rischio chiusura
shortcut disponibili
```

#### Interfaccia telefono

```text
GATE IN 8s
Codice richiesto: BLU-2

[ GATE ROSSO ] [ GATE BLU ] [ GATE GIALLO ]
[ CODICE 1 ] [ CODICE 2 ] [ CODICE 3 ]

Shortcut:
[ ARM SHORTCUT ]
```

#### Azioni

```text
hacker_open_gate(gate_id)
hacker_close_hazard_barrier(barrier_id)
hacker_rotate_ramp(ramp_id, angle_state)
hacker_arm_shortcut(shortcut_id)
hacker_disable_trap(trap_id)
```

#### Effetti sulla cupola

```text
gate che si apre/chiude
rampe che ruotano
laser/barriere che si spengono
shortcut evidenziato
```

#### Successo

Percorso più corto/spettacolare, punteggio combo.

#### Fallimento morbido

Percorso alternativo più lento; mai muro invalicabile a velocità alta.

#### Frequenza evento

Ogni 20–40 secondi.

---

### 5.7 Ruolo 5 — Gunner / Utility

#### Fantasia di ruolo

Il Gunner non è un normale “sparatore”: libera la pista, marca obiettivi, usa utility e ostacola AI.

#### Informazione unica

Vede:

```text
minacce targettabili
lock-on status
munizioni utility
priorità target
AI rivali vicini
```

#### Interfaccia telefono

```text
TARGETS:
[ DRONE SX ] [ DRONE DX ] [ OSTACOLO ]

Utility:
[ FIRE ] [ EMP ] [ DROP DECOY ] [ TAG RIVAL ]
```

#### Azioni

```text
gunner_lock_target(target_id)
gunner_fire_utility(target_id)
gunner_emp_zone(zone_id)
gunner_drop_decoy()
gunner_tag_rival(rival_id)
```

#### Effetti sulla cupola

```text
reticolo lock-on diegetico
proiettile/EMP visibile
ostacolo distrutto
rival rallentato
score popup
```

#### Successo

Linea libera, AI rallentata, combo.

#### Fallimento morbido

Target resta, ma con percorso alternativo; munizioni cooldown.

#### Frequenza evento

Ogni 20–30 secondi, con più densità nel final sprint.

---

## 6. Crew Event Director

### 6.1 Scopo

Implementare un sistema `CrewEventDirector` che crea e gestisce finestre di azione supporter durante la gara.

Non affidarsi a eventi casuali puri. Gli eventi devono essere progettati, leggibili e sincronizzati con la pista.

### 6.2 Ciclo base di una finestra crew

```text
1. Telegraph
   Il gioco anticipa un evento: gate, hazard, boost lane, drone, bivio.

2. Assignment
   Uno o due ruoli ricevono prompt specifici sul telefono.

3. Verbal Coordination
   I giocatori devono comunicare con il pilota e tra loro.

4. Action
   Il supporter invia un comando.

5. World Effect
   Il comando modifica pista/kart/AI.

6. Driver Payoff
   Il pilota sente/vede il risultato in FPV.

7. Score + Feedback
   Cupola e telefoni mostrano successo/combo/cooldown.
```

### 6.3 Frequenza eventi consigliata

In 7 minuti:

```text
8–10 eventi navigazione
6–8 eventi engineer/boost
5–7 eventi defender
5–7 eventi hacker
5–7 eventi gunner
2–3 combo multi-ruolo
1 final sync
```

Non tutti devono agire ogni secondo. Ma nessuno deve restare irrilevante.

### 6.4 Tipi di finestre evento

#### A. Route Choice

Ruolo primario: Navigatore
Ruolo secondario: Hacker

```text
Il navigatore sceglie percorso.
Se sceglie shortcut, l'hacker deve aprirlo.
Il pilota deve fidarsi e prendere la linea giusta.
```

#### B. Boost Chain

Ruolo primario: Ingegnere
Ruolo secondario: Pilota

```text
Il pilota entra in rettilineo/uscita curva.
L'ingegnere rilascia boost entro una finestra di 2–4s.
Se il timing è buono: combo e velocità.
Se troppo presto/tardi: boost ridotto o heat.
```

#### C. Shield Timing

Ruolo primario: Defender
Ruolo secondario: Navigatore/Scanner implicito

```text
Hazard direzionale arriva.
Defender deve scegliere direzione corretta.
Il pilota può anche evitare, ma lo scudo permette traiettoria aggressiva.
```

#### D. Gate Hack

Ruolo primario: Hacker
Ruolo secondario: Navigatore

```text
Gate colorato/codificato in arrivo.
Navigatore vede quale conviene.
Hacker apre la porta.
Pilota prende la traiettoria.
```

#### E. Clear Target

Ruolo primario: Gunner
Ruolo secondario: Pilota

```text
Drone/ostacolo/rivale minaccia la linea.
Gunner locka e neutralizza.
Pilota mantiene velocità.
```

#### F. Team Sync

Ruoli: tutti

```text
Finale o checkpoint speciale.
Ogni supporter deve premere/tenere/confirmare nel proprio timing.
Il pilota attraversa un arco sync.
Risultato: huge boost / finale cinematico / score multiplier.
```

### 6.5 Esempio di timeline 7 minuti

```text
00:00 briefing / role assign
00:20 launch + rocket sync
00:35 route choice 1
00:55 boost chain 1
01:10 checkpoint handoff 1
01:25 shield timing 1
01:45 gate hack 1
02:05 checkpoint handoff 2
02:20 clear target 1
02:40 route choice 2
03:00 checkpoint handoff 3
03:20 boost chain 2 + defender combo
03:45 hazard canyon
04:05 checkpoint handoff 4
04:25 shortcut gate
04:50 gunner target swarm
05:10 checkpoint handoff 5
05:30 overheat crisis
05:50 final route
06:10 checkpoint/final driver
06:20 final sprint
06:45 team sync
07:00 finish/results
```

---

## 7. Track design requirements

### 7.1 Non usare gare normali lunghe

Una gara standard SuperTuxKart potrebbe non avere ritmo arcade/crew. Serve un layer missione sopra la pista.

### 7.2 Segmenti

Ogni pista usata da Immersive Relay deve essere divisa in segmenti:

```json
{
  "track_id": "xr_mine_run",
  "segments": [
    {
      "id": "seg_01_launch",
      "target_duration_sec": 60,
      "handoff_at_end": true,
      "events": ["route_choice_01", "boost_chain_01"]
    }
  ]
}
```

### 7.3 Checkpoint speciali

Ogni checkpoint relay deve avere:

```text
safe camera orientation
no hard turn immediately after resume
visual gate / tunnel / arch
autopilot path
handoff cue
fallback timeout
```

### 7.4 Pista iniziale consigliata

Per MVP non creare subito asset di pista enormi. Usare una pista esistente e aggiungere metadata/eventi.

Target MVP:

```text
pista esistente STK
modalità time mission
AI/rivali opzionali
6 checkpoint relay virtuali
10–14 crew events
finish cinematico
```

Poi creare una pista custom “Dome Relay Test Track” con:

```text
settori larghi
curve leggibili
shortcut visibili
gate grandi
hazard non eccessivi
rettilinei per boost
final sprint scenografico
```

---

## 8. Controller e input

### 8.1 Pilota

Input primario:

```text
volante + pedali
```

Mapping consigliato:

```text
steer axis        volante
accelerate axis   pedale acceleratore
brake axis        pedale freno
drift             paddle/button
fire/use item      button opzionale
rescue/reset       button operatore o volante
camera recenter    button operatore/dev
```

Il pilota non deve usare il telefono mentre guida.

### 8.2 Supporter

Input primario:

```text
smartphone via PWA dell'app Swift
```

Requisiti telefono:

```text
UI grande
azioni discrete
cooldown visibili
ack entro <200 ms ideale
haptic/audio feedback
reconnect senza perdita ruolo
nessun controllo continuo critico
```

### 8.3 Perché non guidare con smartphone

La guida richiede controllo continuo, feedback tattile e bassa latenza. Lo smartphone può essere fallback/demo, ma non è il controller principale per una attrazione pay-per-play.

### 8.4 Compatibilità macOS

L'agente deve testare presto il volante specifico scelto. Apple Game Controller supporta controller fisici e menziona anche racing wheels, ma la compatibilità reale del singolo modello deve essere verificata con una build concreta.

Fonte:

- https://developer.apple.com/documentation/gamecontroller/

---

## 9. Architettura software

### 9.1 Obiettivo architetturale

Separare nettamente:

```text
gameplay relay
camera immersiva
proiezione
output video
API locale
session envelope
```

### 9.2 Moduli suggeriti

Usare nomi reali del repository dopo ispezione. Questi sono nomi concettuali.

```text
src/immersive/
  ImmersiveRelayManager
  EnvironmentProfile
  ImmersiveCameraRig
  ProjectionAdapter
  OutputAdapter
  ImmersiveRenderGraph
  ImmersiveTelemetry

src/immersive/projections/
  FisheyeDomemasterProjection
  EquirectangularProjection
  CylindricalProjection
  PerspectiveStripProjection
  WallAtlasProjection

src/immersive/outputs/
  SyphonOutputAdapter_macOS
  SpoutOutputAdapter_Windows
  WindowOutputAdapter
  NullOutputAdapter
  NDIOutputAdapter_optional

src/modes/immersive_relay/
  RelayRaceMode
  RelaySessionController
  RelayPilotRotation
  CrewRoleManager
  CrewEventDirector
  CrewActionHandler
  RelayScoring
  RelayAutopilotHandoff
  RelayTrackScript

src/remote/
  LocalControlServer
  RelayCommandRouter
  RelayStatePublisher
```

### 9.3 Swift app come session authority

I telefoni devono parlare all'app Swift esistente. SuperTuxKart deve ricevere solo comandi validati dalla Swift app.

Pipeline:

```text
Phone PWA
  → Swift app / LocalServerManager / EventHTTPBridge
  → local API SuperTuxKart
  → RelaySessionController
  → CrewActionHandler
  → game state
  → output immersive
  → Syphon
  → Swift dome capture / warp / blend
```

Motivo:

```text
la Swift app già gestisce sessioni, ruoli, telefoni, timeline, operatore, reset, escape room
SuperTuxKart non deve diventare server principale della venue
```

### 9.4 Local API STK

SuperTuxKart deve esporre una API locale limitata.

Vincoli:

```text
bind solo localhost o rete venue configurata
token/session key
nessun accesso pubblico internet
comandi idempotenti
action_id e state_version
rate limit
log telemetria
```

Endpoint minimi:

```http
POST /immersive/v1/session/start
POST /immersive/v1/session/stop
POST /immersive/v1/session/pause
POST /immersive/v1/session/resume
POST /immersive/v1/session/reset
POST /immersive/v1/crew/action
POST /immersive/v1/operator/handoff
GET  /immersive/v1/state
GET  /immersive/v1/health
WS   /immersive/v1/events
```

### 9.5 Esempio start session

```json
{
  "session_id": "tholus-20260608-001",
  "preset": "standard_arcade",
  "track_id": "xr_relay_test",
  "players": [
    {"player_id": "p1", "display_name": "Player 1"},
    {"player_id": "p2", "display_name": "Player 2"},
    {"player_id": "p3", "display_name": "Player 3"}
  ],
  "pilot_order": ["p1", "p2", "p3"],
  "projection": {
    "type": "fisheye_domemaster",
    "width": 2048,
    "height": 2048,
    "fov_degrees": 180,
    "yaw_degrees": 0,
    "pitch_degrees": -15,
    "roll_degrees": 0
  },
  "output": {
    "type": "syphon",
    "sender_name": "STK_ImmersiveRelay"
  }
}
```

### 9.6 Esempio crew action

```json
{
  "session_id": "tholus-20260608-001",
  "action_id": "a-uuid-123",
  "state_version_seen": 1024,
  "player_id": "p4",
  "role": "hacker",
  "type": "hacker_open_gate",
  "payload": {
    "gate_id": "gate_blue_02",
    "code": "blue_2"
  }
}
```

Risposta:

```json
{
  "accepted": true,
  "state_version": 1025,
  "result": "success",
  "cooldown_ms": 18000,
  "score_delta": 250,
  "message": "Gate opened"
}
```

Errori:

```json
{"accepted": false, "error": "wrong_role", "status": 403}
{"accepted": false, "error": "wrong_phase", "status": 409}
{"accepted": false, "error": "cooldown", "status": 429}
{"accepted": false, "error": "stale_state", "status": 409}
{"accepted": false, "error": "unknown_session", "status": 404}
```

---

## 10. Rendering immersivo

### 10.1 Non fare stretch 16:9

Output immersivo deve derivare dal rendering 3D vero.

Pipeline consigliata:

```text
STK scene camera rig
  → cubemap / multi-view render
  → projection shader
  → offscreen texture
  → Syphon server
  → Swift app cupola
```

### 10.2 Primo target: fisheye/domemaster

Per Mac mini M1:

```text
fisheye/domemaster 2048×2048 @ 60 fps target
```

Motivi:

```text
frame quadrato naturale per fulldome
meno pixel di equirect 4096×2048
più semplice per cupola se l'app Swift sa già warpare/blendare
```

### 10.3 Secondo target: equirettangolare

```text
equirect 2048×1024 @ 60 fps se possibile
4096×2048 solo se performance consente
```

L'equirect è utile per ambienti 360 e pipeline generiche, ma può essere più costoso o meno diretto per cupola.

### 10.4 Orientamento configurabile

La visuale immersiva deve supportare:

```text
yaw
pitch
roll
fov
horizon stabilization
camera height offset
look-ahead distance
roll clamp
pitch clamp
smoothing
```

Esempio:

```json
{
  "camera": {
    "mode": "driver_fpV_stabilized",
    "yaw_degrees": 0,
    "pitch_degrees": -12,
    "roll_degrees": 0,
    "roll_stabilization": 0.85,
    "lookahead_meters": 16,
    "shake_amount": 0.15,
    "comfort_level": "arcade_safe"
  }
}
```

### 10.5 Camera comfort

In cupola, FPV estrema può causare nausea. Per MVP:

```text
stabilizzare orizzonte
limitare roll
ridurre shake
evitare tagli camera improvvisi
anticipare curve con look-ahead morbido
usare cockpit/nose reference leggero
non ruotare tutta la cupola in modo aggressivo
```

Modalità camera:

```text
driver_fpV_stabilized       default spettacolare ma comfort-aware
cockpit_reference           include elemento fisso/kart per orientamento
near_chase_immersive        più confortevole, leggermente dietro kart
cinematic_handoff           usata solo in checkpoint/handoff
operator_debug_flat         debug non immersivo
```

### 10.6 Output adapter macOS: Syphon

Usare Syphon come primo standard locale macOS.

Requisiti:

```text
Syphon server name configurabile
texture output RGBA 8-bit compatibile
zero-copy dove possibile
window fallback se Syphon non disponibile
health status
frame counter
fps metric
```

Fonti:

- Syphon: https://syphon.info/
- TouchDesigner Syphon/Spout In: https://docs.derivative.ca/Syphon_Spout_In_TOP
- TouchDesigner Syphon/Spout Out: https://docs.derivative.ca/Syphon_Spout_Out_TOP

### 10.7 Output adapter Windows futuro: Spout

Non implementare subito se rallenta MVP, ma progettare l'interfaccia.

```text
OutputAdapter
  SyphonOutputAdapter_macOS
  SpoutOutputAdapter_Windows
```

Fonte:

- TouchDesigner documenta Syphon su macOS e Spout su Windows: https://docs.derivative.ca/Syphon_Spout_In_TOP

### 10.8 NDI opzionale

NDI è utile per trasporto su rete o media server esterno, non per primo path locale same-machine.

```text
Syphon/Spout = texture locale same machine
NDI = video-over-IP tra macchine
```

---

## 11. Profili ambiente

### 11.1 Dome profile

```json
{
  "environment": "fulldome",
  "projection": "fisheye_domemaster",
  "width": 2048,
  "height": 2048,
  "fov_degrees": 180,
  "output": "syphon"
}
```

### 11.2 Equirect profile

```json
{
  "environment": "spherical_360",
  "projection": "equirectangular",
  "width": 2048,
  "height": 1024,
  "horizontal_degrees": 360,
  "vertical_degrees": 180,
  "output": "syphon"
}
```

### 11.3 Panoramic LED wall profile

```json
{
  "environment": "panoramic_wall",
  "projection": "perspective_strip",
  "width": 3840,
  "height": 1080,
  "horizontal_fov_degrees": 120,
  "vertical_fov_degrees": 45,
  "output": "syphon"
}
```

### 11.4 Cylinder profile

```json
{
  "environment": "cylinder",
  "projection": "cylindrical",
  "width": 4096,
  "height": 1024,
  "horizontal_degrees": 270,
  "vertical_degrees": 70,
  "output": "syphon"
}
```

### 11.5 CAVE/wall atlas profile

```json
{
  "environment": "cave",
  "projection": "wall_atlas",
  "walls": ["front", "left", "right", "floor", "ceiling"],
  "per_wall_resolution": [1024, 1024],
  "output": "syphon"
}
```

Non implementare tutto nel MVP. Ma progettare `ProjectionAdapter` in modo che non renda impossibile aggiungere questi profili.

---

## 12. Session state machine

### 12.1 Stati

```text
attract
loading
ready_for_players
briefing
countdown
running
handoff
last_minute
final_sprint
finished_gold
finished_silver
finished_bronze
finished_timeout
finished_operator_abort
results
resetting
error_recoverable
error_fatal
```

### 12.2 Comandi operatore

```text
start_session
pause_session
resume_session
extend_session(seconds)
force_handoff(player_id)
skip_handoff
force_final_sprint
finish_now
abort_session
reset_session
recenter_camera
set_projection_profile
set_output_active
```

### 12.3 Reset rapido

Dopo una partita:

```text
stop local timers
clear crew events
clear cooldowns
reset kart/track/AI
reset camera
reset Syphon frame to attract/black/logo
publish ready state
```

Target:

```text
reset software < 15 secondi
slot operativo totale prevedibile
nessun menu manuale STK per l'operatore
```

---

## 13. Scoring e replay value

### 13.1 Score components

```text
base_distance_score
finish_bonus
checkpoint_bonus
crew_action_success
perfect_timing_bonus
combo_multiplier
all_players_drove_bonus
shortcut_bonus
clean_driving_bonus
crew_communication_bonus proxy: successful multi-role chains
time_bonus
```

### 13.2 Punteggio individuale e di squadra

Mostrare sempre:

```text
Team Score grande
Medaglie ruolo piccole
Best Moment / MVP
```

Non creare una classifica che faccia litigare troppo la crew. La metrica principale è di squadra.

### 13.3 MVP / awards

Awards leggeri:

```text
Best Driver Segment
Best Save
Best Boost Timing
Best Navigator
Best Hack
Most Improved
Chaos Medal
```

### 13.4 Replay

Per replay value:

```text
ruoli ruotano
pilot order cambia
track events randomizzati ma controllati
shortcut alternativi
score target medaglie
premium/preset difficoltà
```

---

## 14. Difficulty e comeback

### 14.1 Adaptive difficulty

Il gioco deve mantenere durata e spettacolo.

Variabili:

```text
team_performance
crash_count
crew_success_rate
current_time_vs_target
distance_vs_target
heat/overload analoghi
```

Adattamenti possibili:

```text
ridurre AI pressure
aprire shortcut di recupero
estendere finestra crew di 1–2 secondi
ridurre cooldown
attivare rescue boost
semplificare eventi finali
```

### 14.2 No patronizing

Il comeback non deve sembrare assistenza ovvia. Presentarlo come:

```text
“Emergency route opened”
“Crew overdrive available”
“Final sprint boost charged”
```

### 14.3 Difficoltà preset

```json
{
  "difficulty": "family",
  "crew_window_multiplier": 1.4,
  "hazard_density": 0.7,
  "ai_pressure": 0.6
}
```

```json
{
  "difficulty": "arcade",
  "crew_window_multiplier": 1.0,
  "hazard_density": 1.0,
  "ai_pressure": 1.0
}
```

```json
{
  "difficulty": "challenge",
  "crew_window_multiplier": 0.8,
  "hazard_density": 1.25,
  "ai_pressure": 1.2
}
```

---

## 15. Phone UI specifica

### 15.1 Regole UI

```text
1 schermata per ruolo
massimo 3–5 controlli principali
testi brevi
timer/cooldown grandi
colori + icone + vibrazione, non solo colore
ack immediato
no scrolling durante partita
no login complicato
```

### 15.2 Stato telefono comune

Ogni telefono mostra:

```text
nome giocatore
ruolo attuale
prossimo cambio pilota
stato connessione
azione principale
cooldown
messaggio breve
```

### 15.3 Riassegnazione ruoli

Quando cambia pilota:

```text
il telefono del nuovo pilota passa a “Sei il prossimo pilota / prendi il volante”
il vecchio pilota riceve nuovo ruolo crew
la UI si ricarica senza refresh manuale
```

### 15.4 Haptic/audio

Usare vibrazioni brevi per:

```text
prompt evento
azione accepted
azione rejected
cooldown ready
handoff imminent
final sync
```

---

## 16. Integrazione Swift app

### 16.1 Responsabilità Swift app

La Swift app resta responsabile di:

```text
roster giocatori
join/rejoin smartphone
sessione commerciale / timeline
assegnazione ruoli
validazione utenti
eventi operatore
cattura Syphon
warp/blend/proiezione
audio/lighting/eventi sala se già presenti
risultati e reset
```

### 16.2 Responsabilità STK

SuperTuxKart fork è responsabile di:

```text
simulazione gara
modalità relay
crew events
camera immersiva
rendering output
ricezione comandi validati
telemetria stato
```

### 16.3 Stato pubblicato a Swift

```json
{
  "session_id": "...",
  "state_version": 2048,
  "phase": "running",
  "time_remaining_ms": 312000,
  "active_pilot": "p3",
  "next_pilot": "p4",
  "current_segment": "seg_03",
  "score": 18420,
  "combo": 2.5,
  "crew_events": [
    {
      "id": "gate_04",
      "role": "hacker",
      "status": "active",
      "time_remaining_ms": 5400
    }
  ],
  "health": {
    "fps": 59.7,
    "output_active": true,
    "syphon_sender": "STK_ImmersiveRelay"
  }
}
```

---

## 17. Licenze e fork strategy

### 17.1 SuperTuxKart

SuperTuxKart usa Git per il codice e SVN per i dati/assets. Il codice è GPL v3.0 o successiva; gli asset hanno documentazione/licenze da rispettare.

Fonti:

- Source control: https://supertuxkart.net/Source_control
- Licensing: https://supertuxkart.net/Licensing

### 17.2 Regole

```text
mantenere fork sottile
non introdurre asset proprietari non documentati
non usare loghi/marchi non autorizzati
mantenere file LICENSE/credits aggiornati
se distribuita, rispettare GPL e fornire sorgenti della fork secondo obblighi applicabili
consultare legale per uso commerciale onsite/distribuzione
```

### 17.3 Upstream sync

Branch consigliati:

```text
upstream/main          mirror SuperTuxKart ufficiale
tholus/immersive-main  branch stabile fork
tholus/dev             sviluppo attivo
release/YYYY.MM        build venue
```

Workflow:

```bash
git fetch upstream
git checkout tholus/immersive-main
git merge upstream/master
# oppure rebase se il team lo preferisce e lo documenta
svn update ../stk-assets
cmake --build build
ctest --test-dir build
```

Regola:

```text
Più i hook sono piccoli, più gli update saranno rapidi.
```

---

## 18. Build, feature flags e fallback

### 18.1 Flag

```text
ENABLE_IMMERSIVE_RELAY=ON/OFF
ENABLE_SYphon_OUTPUT=ON/OFF   # nome reale secondo build system
ENABLE_SPOUT_OUTPUT=ON/OFF
ENABLE_NDI_OUTPUT=ON/OFF
ENABLE_RELAY_LOCAL_API=ON/OFF
ENABLE_RELAY_TEST_TOOLS=ON/OFF
```

Nota: usare naming coerente con CMake/repo reale. Non inventare flag senza integrarli.

### 18.2 Vanilla behavior

Con immersive disabilitato:

```text
SuperTuxKart vanilla deve continuare a compilare e funzionare
nessun regression test rotto
nessun menu standard modificato se non necessario
```

### 18.3 Fallback output

```text
Syphon disponibile → usare Syphon
Syphon non disponibile → finestra offscreen/window output
headless test → NullOutputAdapter
```

---

## 19. Performance target Mac mini M1

### 19.1 Target MVP

```text
fisheye 2048×2048
60 fps desiderato
30 fps minimo accettabile per debug, non per show
latenza crew action visibile < 200 ms ideale
local API < 50 ms su localhost
```

### 19.2 Preset

```json
{
  "m1_safe": {
    "projection": "fisheye_domemaster",
    "resolution": [2048, 2048],
    "views": "cubemap_5_or_6",
    "shadows": "low",
    "post_fx": "minimal",
    "particles": "medium"
  },
  "m1_equirect_test": {
    "projection": "equirectangular",
    "resolution": [2048, 1024],
    "views": "cubemap_6",
    "shadows": "low",
    "post_fx": "minimal"
  },
  "dev_low": {
    "projection": "fisheye_domemaster",
    "resolution": [1024, 1024],
    "views": "cubemap_5_or_6",
    "shadows": "off"
  }
}
```

### 19.3 Ottimizzazioni

```text
aggiornare simulazione crew a 10–20 Hz, non 60 Hz
renderizzare animazioni GPU localmente
limitare post-processing
ridurre ombre/riflessi
LOD aggressivo fuori FOV utile
non duplicare texture CPU/GPU
misurare frame time per vista cubemap
```

---

## 20. Test e acceptance criteria

### 20.1 Test tecnici

```text
build macOS Debug/Release
vanilla mode smoke test
relay session start/reset smoke test
local API action tests
fake Swift client
fake phone clients 1–5
projection output tests
Syphon visible in test receiver / TouchDesigner / vostra app
performance capture 5 min
handoff 6 giocatori simulati
reconnect telefono
operator abort
```

### 20.2 Test gameplay

Una build è giocabile solo se:

```text
il pilota deve parlare almeno 5 volte con la crew in 7 minuti
ogni supporter compie almeno 5 azioni significative in 7 minuti
ogni ruolo ha almeno 3 feedback visibili sulla cupola
nessun giocatore rimane inattivo >30s
la partita finisce sempre con risultato e reset
la crew può distinguere chiaramente successo/fallimento delle azioni
un gruppo nuovo capisce cosa fare entro 60s
```

### 20.3 Playtest checklist

Durante playtest osservare:

```text
Guardano la cupola o i telefoni?
Il pilota chiede aiuto?
I supporter parlano tra loro?
Qualcuno resta zitto/inattivo?
I prompt sono troppo lunghi?
La FPV dà nausea?
I cambi pilota sono sicuri?
La durata sembra corretta?
Vogliono rigiocare?
Ricordano un momento spettacolare?
```

### 20.4 Metrics da loggare

```text
session_duration
active_pilot_time per player
crew_actions_count per role
crew_success_rate
time_between_prompts per role
crash_count
handoff_duration
fps_avg/fps_p1/fps_min
api_latency_avg
phone_reconnect_count
score
finish_type
```

---

## 21. MVP implementativo consigliato

### Fase 0 — repo audit

Output richiesto:

```text
build funzionante vanilla
mappa file camera/render/input/checkpoint
piano hook minimali
rischi reali rilevati
```

### Fase 1 — Sessione relay senza rendering immersivo

Implementare:

```text
RelaySessionController
PilotRotation
Checkpoint/handoff
Local API base
Swift fake client
```

Output:

```text
gara normale 16:9 ma con pilota che cambia e sessione controllabile
```

### Fase 2 — Crew actions mock ma reali

Implementare:

```text
CrewRoleManager
CrewActionHandler
5 ruoli
azioni base che modificano kart/pista/score
fake phone clients
```

Output:

```text
pilot + crew giocabile anche senza fulldome
```

### Fase 3 — Crew Event Director

Implementare:

```text
track script JSON
finestre evento
cooldowns
score combo
feedback cupola/telefono
```

Output:

```text
sessione 7 minuti completa in 16:9 debug
```

### Fase 4 — Fisheye/domemaster + Syphon

Implementare:

```text
ImmersiveCameraRig
Cubemap render
FisheyeDomemasterProjection
SyphonOutputAdapter
camera orientation controls
```

Output:

```text
2048×2048 fisheye via Syphon visibile nella app Swift / receiver
```

### Fase 5 — Stabilizzazione arcade

Implementare:

```text
state machine completa
operator controls
reset rapido
performance pass M1
playtest 2/4/6 players
```

Output:

```text
MVP venue-ready
```

### Fase 6 — Equirect e profili ambiente

Implementare:

```text
equirect 2048×1024
projection profile JSON
panoramic/cylindrical stub
```

### Fase 7 — Windows port

Implementare:

```text
SpoutOutputAdapter
Windows build flags
controller tests
```

---

## 22. JSON schemas essenziali

### 22.1 Role assignment

```json
{
  "active_pilot": "p2",
  "roles": {
    "p1": "engineer",
    "p2": "pilot",
    "p3": "navigator",
    "p4": "defender",
    "p5": "hacker",
    "p6": "gunner"
  },
  "next_pilot": "p3",
  "handoff_in_ms": 48000
}
```

### 22.2 Crew event

```json
{
  "id": "event_gate_blue_04",
  "type": "gate_hack",
  "primary_role": "hacker",
  "secondary_role": "navigator",
  "status": "telegraph",
  "starts_in_ms": 5000,
  "duration_ms": 8000,
  "world_target": "gate_blue_04",
  "success_effect": {
    "open_gate": true,
    "score": 300,
    "combo_add": 1
  },
  "fail_effect": {
    "route_penalty_sec": 4,
    "combo_break": true
  }
}
```

### 22.3 Phone role surface

```json
{
  "player_id": "p5",
  "role": "hacker",
  "state_version": 3002,
  "prompt": "Gate blu tra 6s",
  "controls": [
    {"id": "gate_red", "type": "button", "label": "Rosso", "enabled": true},
    {"id": "gate_blue", "type": "button", "label": "Blu", "enabled": true},
    {"id": "gate_yellow", "type": "button", "label": "Giallo", "enabled": true}
  ],
  "cooldowns": {
    "hacker_open_gate": 0
  }
}
```

---

## 23. Rischi principali e mitigazioni

### 23.1 Crew percepita come appiccicata

Mitigazione:

```text
azioni crew modificano fisicamente pista/kart/AI
feedback enorme in cupola
eventi frequenti
ruoli con informazione unica
pilot voice prompts incoraggiati
```

### 23.2 Troppo telefono, poca cupola

Mitigazione:

```text
UI telefono minimal
prompt brevi
cupola mostra risultato
telefono non mostra mappa completa
```

### 23.3 FPV scomoda

Mitigazione:

```text
stabilized FPV
roll clamp
near chase option
sessioni brevi
comfort presets
```

### 23.4 Handoff volante caotico

Mitigazione:

```text
autopilot checkpoint
timer handoff
cue chiari
operator override
single pilot mode per demo
```

### 23.5 Fork difficile da aggiornare

Mitigazione:

```text
moduli nuovi
hook minimi
feature flags
CI vanilla
no core rewrites
```

### 23.6 Performance M1 insufficiente

Mitigazione:

```text
fisheye 1024/2048 preset
riduzione effetti
window fallback
equirect solo fase successiva
profiling continuo
```

### 23.7 Licenze

Mitigazione:

```text
mantenere licenze/crediti
non importare asset non chiari
documentare modifiche GPL
consultare legale per distribuzione/uso commerciale
```

---

## 24. Prompt operativo breve per l'agente

L'agente deve trattare questo come mandato di prodotto:

```text
Implementa SuperTuxKart Immersive Relay come fork sottile.
Prima milestone: macOS, pilota con volante, fino a 5 telefoni crew via app Swift, modalità Relay con cambio pilota a checkpoint, sessione arcade 7 minuti, crew actions reali, output fisheye 2048×2048 via Syphon.

Non costruire una gara normale con bonus telefono.
Costruisci una attrazione arcade cooperativa: il pilota guida, la crew rende possibile guidare bene.

Mantieni gameplay, proiezione e output separati.
Mantieni vanilla SuperTuxKart funzionante.
Progetta per aggiungere equirect, Windows/Spout e altri profili dopo l'MVP.
```

---

## 25. Definition of Done MVP

La prima release MVP è accettabile solo se:

```text
[ ] compila su macOS
[ ] vanilla SuperTuxKart resta avviabile
[ ] modalità Immersive Relay avviabile da API locale
[ ] 1 pilota guida con volante/pedali
[ ] 5 ruoli crew ricevono UI/azioni tramite fake Swift o Swift reale
[ ] pilota cambia a checkpoint con autopilot handoff
[ ] almeno 10 crew events in una sessione da 7 minuti
[ ] ogni ruolo modifica realmente kart/pista/AI/score
[ ] output fisheye/domemaster non 16:9 stretch
[ ] Syphon server visibile dalla app Swift o receiver esterno
[ ] camera yaw/pitch/roll configurabile
[ ] reset sessione senza menu manuale
[ ] telemetria base esportata
[ ] playtest 2, 4, 6 giocatori completato
[ ] documentati build commands, limiti, performance M1, known issues
```
