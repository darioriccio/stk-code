# SuperTuxKart / TholusOS — implementazione del piano MVP

Data: 7 ottobre 2026. Piano di riferimento: `/Users/dario/Downloads/PLAN (3).md`.
Stato: **implementata e revisionata da codice; compilazione e funzionamento in esecuzione non verificati**.

## Risultato consegnato

Il piano è implementato nei repository principali `stk-code` e
`SocialTableController`. La modalità è una staffetta cooperativa da **1–6
partecipanti su un kart condiviso**: un pilota guida, gli altri possono aiutare
dai telefoni, poi si alternano. I telefoni sono facoltativi. I partecipanti e
l'ordine di guida esistono anche senza connessioni telefoniche.

- Scelta tra Snow Peak, Around the Lighthouse e Oliver’s Math Class quando
  installate; kart ricavati dal catalogo del producer, valori indisponibili
  rifiutati prima della gara. Durata: 6/8/10/12 giri secondo il gruppo.
- Cinque ruoli assegnati soltanto ai telefoni collegati della crew, ricalcolati
  dopo presenza e cambio pilota; supporto facoltativo sul telefono nel caso solo.
  Console multi-ruolo con tutte le azioni, scelta partecipante, nickname,
  identità conservata dopo timeout, scambio e liberazione dei telefoni.
- Dodici attività ordinarie tra gli incontri: navigazione/ritmo, turbo completo,
  raffreddamento, scudo/pulizia, sincronizzazione, rallentamento/impulso e diversivo.
  Il diversivo dà attività all'artigliere anche senza avversari. Cooldown e
  risorse restano autorevoli in STK e non si ricaricano ricollegando il telefono.
- Otto incontri in ciascuno dei due script aggiornati; combo con requisiti
  limitati ai ruoli disponibili. Le attività ordinarie non assegnano successi
  di incontri inesistenti. Gli effetti pubblici descrivono bonus arcade reali.
- Pronto da Invio, pulsante SDL configurato o telefono del pilota corretto.
  Gara congelata al cambio, almeno 6 secondi di passaggio e countdown di 3
  dopo conferma. Il timeout non fa ripartire il kart. Cambio automatico
  separatamente selezionabile, pausa predefinita 10 secondi.
- Pronto anche per briefing, recupero dopo perdita input e nuova gara dai
  risultati; ricreazione del mondo sul ciclo principale, tra i frame. Input
  azzerati al cambio, ripartenza rifiutata finché il dispositivo è scollegato.
- Selezione dispositivo SDL/tastiera, assi sterzo/gas/freno, Pronto, inversione,
  pedali su corsa completa o asse condiviso, zona morta e valori dal producer.
  Il confronto Swift/C++ tollera soltanto l'arrotondamento della zona morta.
- Impostazioni nella sezione Giochi: partecipanti/nomi, pista/kart, difficoltà,
  AI, crew, cambio, audio, profilo Syphon e launcher. Configurazione effettiva
  verificata contro quella richiesta; modifiche strutturali bloccate in gara,
  input modificabile prima del countdown e durante il recupero in pausa.
- Diagnosi di avvio e log locale limitato con token oscurati anche quando divisi
  tra callback. Nessuna installazione o aggiornamento dell'app in questa fase.
- Overlay in cupola attraverso MediaBus e renderer esistenti: pilota/giro,
  briefing/countdown/cambio, effetti e combo, risultati e invito a rigiocare.
  Alla perdita della proprietà elimina esclusivamente i propri elementi.

## Revisione del codice e controlli statici

Verificati i percorsi browser → LocalServer → adapter/controller Swift → API
locale autenticata → coda del ciclo principale → sessione/effetti → snapshot →
telefoni/overlay. Riveduti i casi zero telefoni, ruoli concentrati, 1–6 persone,
risposte asincrone dopo riassegnazione/riduzione/chiusura, cooldown, recupero
input e replay. Questa è evidenza di sorgente, non una prova funzionale.

La revisione ha corretto il mismatch `/input/config` contro `/input/configure`,
il riavvio con dispositivo ancora assente, gli input mantenuti al cambio, il
confronto Float/Double e la verifica del token (header esatto, sensibile alla
maiuscola, senza autenticazione tramite corpo o prefisso del token).

Controlli eseguiti: `git diff --check` sui file pertinenti nei repository
principali; sintassi JavaScript con `node --check`; parsing JSON dei due script
(8 incontri ciascuno); parsing AST dei tre probe Python; sola analisi sintattica
shell del launcher e dello script di build tramite `bash -n`.
**Nessuna build, compilazione, esecuzione di test, avvio STK/TholusOS o prova
hardware è stata eseguita per questa implementazione.** I log delle build della
fase precedente non attestano queste modifiche.

Test di regressione scritti e revisionati, non eseguiti:

- `ProbeRelayCooperativeMVP.py`: 1–6, presenza zero/parziale, tutti i ruoli,
  turbo, duplicati/cooldown, nessun timeout automatico, Pronto e recupero/replay.
- `ProbeRelayInputMVP.py`: catalogo input, valori effettivi, rifiuto configurazioni
  errate senza mutazione, header autenticati esatti; non avvia o compila STK.
- TholusOS: `ImmersiveSuperTuxKartIntegrationTests`, `GamesPhonePresenceTests`,
  `ImmersiveSTKPresentationTests`: ruoli, presenza, identità e risposte tardive,
  impostazioni, readiness, HUD/ownership, arrotondamento input e log.

## Accettazione ancora da eseguire

Quando l'utente riaprirà esplicitamente la fase di build: controlli mirati, una
build alla volta, poi gare complete e replay sulle tre piste. Verificare
1–6 partecipanti con tutti/alcuni/zero telefoni, ingresso e riconnessione,
effetti fisici e combo, nessuna attesa oltre 15 secondi dovuta alle attività
ordinarie, pause/recupero e chiusura senza interferire con altre esperienze.

Il volante/pedaliera non è ancora scelto. SDL e configurazione sono implementati;
guida, pedali, pulsante Pronto e scollegamento richiedono collaudo sul modello
reale. Force feedback fuori dall'MVP. Infine: staffetta con sei persone,
proiezione in cupola e partita gestita dal gruppo dopo la preparazione senza
interventi continui dell'operatore. **Supporto hardware e MVP funzionante non
sono ancora qualificati.**

## Integrazione e documentazione

Integrati selettivamente 27 file STK e 20 TholusOS, con merge a tre vie e
revisione dei conflitti. Le aggiunte concorrenti di distribuzione/catalogo giochi
in `GamesWorkspaceStore.swift` sono conservate. Il manifest con hash del
sorgente consegnato è `MVP_COMPLETA_SORGENTI_2026-10-07.json`.

Contratto additivo aggiornato in
`stk-code/docs/immersive_relay_tholustos_integration.md`.
Evoluzione separata, non implementata:
`stk-code/docs/immersive_relay_future_interactive_tracks.md` (cancelli, rampe e
scorciatoie reali: geometrie, collisioni, AI, segnali e cooperazione).
L'audit iniziale e il resoconto della precedente fase restano documenti storici.

Nessun commit, push, cambio upstream, installazione o pulizia di cache condivise.
I worktree sono conservati perché contengono modifiche non committate.

Rimosse le copie temporanee create esclusivamente per il merge; manifest ed evidenze di sorgente conservati nel resoconto.
