# SC2 — invalidazione persistente del fascio, 2026-10-15

Correzione del punto 3 dell’audit SC2. Lavoro esclusivamente sui sorgenti:
nessuna build, compilazione, configurazione CMake, esecuzione C++ o avvio SC3.

## Ricontrollo della correzione precedente

Prima della modifica, i nove hash del checkpoint
`../sc2-stagger-clearance-2026-10-15/source-manifest.json` coincidevano con i
sorgenti correnti. Rieseguiti con esito positivo i sei controlli Python del volume
del sobbalzo: AABB della forma fisica composta, centro reale e arrotondamento
conservativo, inviluppo della rotazione e traslazione, supporti ammessi e soffitto
a 1,15 m. Riletto il percorso dalla forma Bullet alla factory: sovrapposizione
iniziale e sweep devono entrambi consentire il movimento; altrimenti resta il
solo arresto. Questi sorgenti non sono modificati da questa correzione.
Rimane da verificare il comportamento Bullet/animazione nel gioco eseguito.

## Comportamento corretto

Ogni sorgente conserva un bit `proofInterrupted` associato alla pressione tool.
Dopo una prova storica positiva, una prova negativa definitiva oppure il cambio
di target o dell’istanza combat imposta il bit prima di sostituire l’ultima prova.
Un miss iniziale, prima di qualsiasi prova positiva, non perde un lock inesistente.
Prove positive successive aggiornano posizione, identità e freschezza, ma non
cancellano l’interruzione: `beamProofs()` continua a esporre `visible=false`.
Le letture sono osservative e non consumano la barriera.

Il controllo epoch/seriale precede anche questa mutazione: un completamento
obsoleto non interrompe una nuova pressione. Solo una nuova pressione secondary
ammessa, già validata per rilascio, tracking, gameplay, posa e capacità, azzera il
bit e la vecchia prova positiva. Il rilascio invalida l’epoch precedente e serve
comunque una nuova prova storica; una pressione primary non riabilita il tool.
La memoria aggiunta è costante: un booleano per sorgente, senza journal di perdite.

Il percorso autoritativo già esistente riceve la prova negativa, chiude e azzera
acquisizione/contributi, blocca il medesimo tool e solo dopo elabora i colpi.
Il successivo hit non può mantenere o riaprire la vecchia finestra. Se
release/press avviene interamente fra due tick, il cambio epoch fa comunque
chiudere la finestra precedente prima di iniziarne una nuova da zero. Le prove
sono isolate per sorgente e nessun dato nuovo attraversa HTTP o shader.

Riferimenti di requisito: piano SC2 §5–6, M21–M24, M41. Non cambia la policy di
campionamento a 60 ms né quella delle prove GPU ancora pending; una perdita non
risolta non viene inventata. Il feedback visuale del tool resta un punto separato.

## Verifiche

- `python3 -B tools/immersive/tests/check_scintilla_combat.py`: 10 controlli
  source/math, inclusi predicato C++ della barriera valutato senza compilatore,
  sequenze positivo/negativo/positivo e cambio target/istanza, protezione epoch,
  reset solo dopo ammissione e ordine chiusura/prima dei colpi.
- `python3 -B tools/immersive/tests/check_scintilla_stagger_clearance.py`: 6
  controlli geometrici; nessuna esecuzione Bullet.
- Quattro regressioni native aggiunte e registrate (64 casi spatial totali):
  integrazione consumer/core in Acquiring e Breached, miss/target alternativo/
  depth indisponibile/istanza ritirata seguiti da hit prima del tick, contributi
  cancellati e colpo pendente respinto; riarmo con nuova pressione; miss iniziale
  e positivi continui; isolamento sorgenti; completamento negativo del vecchio epoch.
- Diff e registro dei casi verificati. Test nativi scritti e revisionati,
  **non compilati né eseguiti**. Nessuna qualificazione runtime, GPU o hardware.

Il manifest adiacente fotografa esclusivamente i file di questa consegna;
i manifest delle consegne precedenti restano evidenze storiche immutabili.
