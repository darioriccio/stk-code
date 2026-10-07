# Ricontrollo da codice — STK su superfici TholusOS

Data: 7 ottobre 2026. Perimetro: implementazione di PLAN (4), dal producer equirettangolare alla preparazione dell'uscita e al consenso alla guida. Nessuna build, compilazione, apertura del gioco, richiesta HTTP al producer o prova GPU/Syphon eseguita.

## Esito

La versione precedente aveva problemi concreti, compreso un riferimento Swift a un caso enum inesistente. Sono stati corretti e integrati nei due repository principali, preservando il lavoro preesistente. La seconda revisione del codice non ha individuato altri blocchi certi nei percorsi esaminati. Questo è un esito di revisione sorgente: compilabilità complessiva, funzionamento reale e compatibilità fisica restano NON VERIFICATI.

## Problemi corretti

| Problema | Correzione |
|---|---|
| `snapshot.syphonState == .ready` in due punti, mentre `VideoInputConnectionState` espone `.connected` | Usato il caso effettivo, sia per il formato ricevuto sia per l'abilitazione dell'uscita. Il precedente codice non era compilabile in questi punti. |
| UV rinormalizzate dalle regioni di contenuto e dal bounding box della camera prima del campionamento STK | STK conserva le UV canoniche richieste dalla geometria; maschera e alpha restano applicate. Corretti entrambi i rami Metal interessati. |
| Ready possibile senza completamento del rendering; cache di preparazione fallita riutilizzata | Ogni attivazione ha un nuovo `preparationID`. Il renderer conferma dopo un comando GPU completato con video e mapping calibrato. Il runtime richiede quella specifica conferma. Errori e callback superati sono scartati. Una nuova preparazione ritenta le risorse fallite. |
| Configurazioni sovrapposte, vecchia preparazione riattivabile dopo rilascio, consenso riapribile durante la preparazione | Configurazione e ripreparazione sono serializzate; la revisione viene invalidata prima degli await di chiusura/rilascio; le operazioni obsolete si fermano. Il gate resta chiuso durante la preparazione. L'inizializzazione delle osservazioni è sincrona, evitando l'invalidazione tardiva della prima configurazione. |
| POST di readiness con esito incerto lasciava valida la vecchia cache | Su errore/timeout la cache torna sconosciuta; il polling ritenta lo stato desiderato, compresa la chiusura. Anche il cambio pilota verifica il consenso locale. |
| Numeri JSON finiti in double ma fuori dall'intervallo float potevano rendere infinite le coordinate della camera | Validazione dei campi camera prima di modificare la configurazione; risposta 400 specifica. |

I delta esatti e gli hash prima/dopo sono allegati nello stesso percorso: `SURFACES_RICONTROLLO_DELTA_2026-10-07.patch` e `SURFACES_RICONTROLLO_MANIFEST_2026-10-07.json`.

## Percorsi riesaminati

- Sei facce native → shader equirettangolare → texture rettangolare Syphon; convenzione delle direzioni e ribaltamento verticale.
- Negoziazione capabilities, applicazione del formato, dimensioni API/texture, proprietà e generazione della connessione.
- Riconnessione dopo ricreazione del server Syphon: il descriptor STK non fissa il vecchio UUID e il provider ricerca nuovamente il server quando il client non è più valido.
- Geometria metrica, raggi qualificati, vista frontale UV, maschere, HUD e preparazione delle risorse.
- Consenso GPU anche nel percorso domemaster: non richiede i flag o le texture specifiche dell'equirettangolare.
- Preparazione, ripreparazione, cambio stanza/impostazioni, pausa, rilascio, risposte tardive e timeout.

Il modello cooperativo resta invariato: 1–6 partecipanti, un pilota alla volta e telefoni facoltativi. Questo ricontrollo non costituisce un playtest delle meccaniche o dei telefoni.

## Verifiche effettuate

- PASS: 5 controlli Python di sorgente/matematica della proiezione, inclusi 4.000 round trip, seam/poli, copertura delle sei facce e cablaggio del gate.
- PASS: controllo delle referenze allo stato Syphon contro i casi enum dichiarati, binding della texture metrica e completamento dei percorsi di disegno.
- PASS: AST del probe Python e sintassi zsh del launcher; nessuna esecuzione di launcher o richieste di rete.
- PASS: `git diff --check` nei due repository principali e corrispondenza byte per byte degli 8 file integrati rispetto ai worktree di lavoro.
- Predisposti, NONRUN: due test Swift di regressione per configurazioni simultanee e rilascio durante il preflight; 80 casi HTTP per valori camera non validi con verifica di conservazione della configurazione precedente.
- Il controllo globale delle localizzazioni non passa: 39 rilievi nel lavoro già presente, fra pannelli STK precedenti e distribuzione giochi. Le correzioni di questo ricontrollo non aggiungono testi UI. Il tool mancava nel vecchio worktree isolato ed è stato eseguito nel repository principale. Non si dichiara il repository globalmente conforme.

## Limiti e verifiche ancora necessarie

Le superfici devono avere calibrazione valida, osservatore/fronte utilizzabili e una zona leggibile per guida e HUD. Il tipo di stanza da solo non garantisce la preparazione.

Le mesh hanno un budget esplicito di 4.194.304 coppie nel preflight condiviso. Nell'attuale costruzione dei pacchetti, una singola chart oltre 2.048 triangoli viene rifiutata: il supporto alle mesh non equivale al supporto a complessità arbitraria. Le point cloud grezze rimangono escluse come previsto dal piano.

NONRUN: compilazione Swift/C++/Metal/GLSL, test Swift, probe HTTP live, allocazione GPU reale, ricezione e riconnessione Syphon, resa delle superfici e HUD, prestazioni, proiettori, controller fisici e partita con 1/6 partecipanti. La conferma del comando GPU introdotta nel codice non equivale a conferma della presentazione su un proiettore fisico.

Nessun commit o push. Rimossi solo gli snapshot temporanei di questo ricontrollo dopo averne salvato delta e hash; conservati i worktree con lavoro in corso.
