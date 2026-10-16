# SC2 — feedback persistente del tool, 2026-10-15

Correzione del punto 4 dell’audit SC2: il blocco fino al rilascio e la contesa
raggiungono ora snapshot pubblici per partecipante, feedback della sorgente e
shader delle due proiezioni. Solo sorgenti: nessuna build, compilazione,
configurazione CMake, esecuzione C++/GLSL, avvio del gioco o SC3.

## Ricontrollo del punto precedente

Prima della modifica, tutti i sette hash del checkpoint
`../sc2-beam-loss-2026-10-15/source-manifest.json` coincidevano con i sorgenti.
Rieseguiti con successo i dieci controlli combat, compresi predicato persistente,
protezione epoch/seriale, riarmo solo dopo ammissione e chiusura prima dei colpi.
Riletto il percorso consumer/core: un hit successivo non cancella il negativo;
una nuova pressione azzera la vecchia prova e richiede una prova nuova.
La correzione corrente conserva quei comportamenti e rende visibile il blocco.
Le quattro regressioni native di quel checkpoint rimangono non eseguite.

## Stato e percorso autoritativo

`ScintillaCombat::Runtime::toolSnapshots()` espone al massimo cinque record con
partecipante, epoch del controllo, target, stato e indice crew dell’opener.
`ToolStatus` ha valori stabili: None=0, ReleaseRequired=1, TargetBusy=2.

Il blocco privato esistente produce ReleaseRequired per scadenza, perdita del
lock, incompatibilità, ritiro target e completamento, anche se l’applicazione
fisica viene rifiutata. L’invalidazione del consumer produce la stessa istruzione
immediatamente, anche prima del tick autoritativo: uno snapshot precedente del
core non può nasconderla. Nessuna modifica alle soglie o alle regole di riarmo.

TargetBusy deriva dalla prova valida corrente su un target Acquiring/Breached
con un opener diverso. Il record indica il vero owner, senza cambiare la priorità
di acquisizione o sottrarre il lock. Un record non rimane busy dopo che il target
ha perso l’opener o completato l’effetto nello stesso tick. La contesa non è un
blocco permanente: quando il bersaglio torna disponibile si applicano le regole
esistenti; la perdita del proprio lock continua invece a richiedere rilascio.

L’engine pubblica i record dopo update, colpi e conferma dell’effetto. Il consumer
li abbina a partecipante e tool epoch attuali; completamenti vecchi non possono
ripristinare un messaggio dopo rilascio/nuova pressione. Pausa, disarmo, reset,
reprepare e disabilitazione del contesto cancellano il feedback applicabile.

Il canale persistente è separato da `Feedback` dei colpi: una primaria, un miss,
un contributo o la scadenza dei suoi 500 ms non sostituiscono l’istruzione tool.
Non aggiunge code, dipendenze, query GPU o memoria crescente con la sessione.

## Feedback della sorgente e API additiva

Ogni elemento di `GET /immersive/v1/spatial/state` → `feedback[]` conserva i
campi esistenti e aggiunge `tool`, per esempio:

```json
{
  "status": 1,
  "reason": "tool_release_required",
  "instruction": "Release the tool button, then press it again.",
  "controlEpoch": "7",
  "targetId": 3,
  "ownerCrewIndex": -1
}
```

Status 2 usa `reason: "target_busy"` e indica l’opener in `ownerCrewIndex`.
L’istruzione spiega che si può sparare quando lo scudo si apre oppure rilasciare
il tool e scegliere un altro target. Le istruzioni sono stringhe diagnostiche
inglesi; un client può localizzare i due reason stabili. Status 0 ha reason e
istruzione vuoti. L’epoch è una stringa decimale; l’indice crew non è uno slot
shader. Nessuna modifica agli endpoint congelati o al contratto SI5.

## Segnale nel gioco

Un nuovo uniforme `scintilla_tools[5]` trasporta soltanto stato numerico e slot
dell’opener. Il recipient mantiene colore e numero di punti già usati dal mirino.

- ReleaseRequired: due pulsanti collegati, freccia di rilascio verso l’alto e
  successiva freccia di pressione verso il basso. Rimane finché serve riarmare.
- TargetBusy: lucchetto con barre di pausa. Sopra appare l’identità dell’opener;
  sotto quella del destinatario. La forma distingue il caso anche senza colore.

Gli indicatori sono badge di controllo in posizioni fisse, esterni alla geometria
di gioco: non promettono contatto né ampliano hitbox, clipping o visibilità depth.
La scala mantiene separati i cinque badge, anche alle risoluzioni ridotte; gli
interi box rientrano nel disco fisheye e nel quadro equirettangolare. Il cerchio
di tool held non appare operativo quando il tool segnala riarmo o contesa.
Il rendering del canale persistente segue gli esiti transitori dei colpi, che
quindi non possono coprire l’istruzione. Non è stato introdotto un renderer testo;
l’indicazione nel video è il pittogramma, la frase esplicita è nel DTO diagnostico.

## Verifiche eseguite e limiti

- `python3 -B tools/immersive/tests/check_scintilla_tool_feedback.py`: 6 controlli
  sorgenti/geometria; valutazione del predicato di visibilità C++ senza compilatore,
  precedenza della barriera, ordine di pubblicazione, codici shader e mapping,
  forme distinte e interi box non sovrapposti nel dominio delle due proiezioni.
- `python3 -B tools/immersive/tests/check_scintilla_combat.py`: 10 controlli.
- `python3 -B tools/immersive/tests/check_scintilla_depth.py`: 6 controlli; regole
  di occlusione, campionamento e clipping conservativo restano coerenti.
- Sei regressioni native aggiunte e registrate: quattro combat (25 totali) e due
  spatial (66 totali). Coprono cause del blocco, owner busy e priorità, completamento
  nello stesso tick, reset/pausa/reprepare, primaria indipendente e vecchi epoch.
  **Scritte e revisionate, non compilate né eseguite.**
- Diff, registro dei casi e manifest verificati prima della consegna.

Restano da eseguire, con nuova autorizzazione di build: regressioni native,
compilazione shader e verifica integrata dei due output. La riconoscibilità e
leggibilità dei pittogrammi devono essere valutate nel gioco e sulla proiezione
reale; i controlli geometrici non le certificano. Nessuna qualificazione hardware
né dichiarazione di completamento integrato di SC2.

Il manifest adiacente fotografa questa consegna; quelli precedenti restano
checkpoint storici e non vengono riscritti dopo le successive modifiche.
