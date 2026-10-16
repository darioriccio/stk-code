# SC1 — correzioni software residue

Lavorazione 14–15 ottobre 2026. Limiti: nessuna build, compilazione, esecuzione
di regressioni native, avvio app/gioco o verifica hardware. SC2 non avviata.
I risultati dei checkpoint sono verifiche del sorgente, non qualifica runtime.

## 4 — Rinnovo del clock

Evento e prova ottica vengono confrontati nel clock host continuo comune.
La monotonicità riguarda entrambi i limiti originali della prova; una stima NTP
nuova non modifica i candidati di un colpo già ricevuto. Restano i controlli
nel dominio STK per età, attivazione e cattura, più il divieto di riscrivere
intervalli già sigillati o riaprire prefissi scartati.

Revisionati i casi di offset nelle due direzioni, prova ritardata/ritagliata,
depth in attesa, ambiguità reale, prova arretrata e riscrittura. I test nativi
sono sorgenti revisionati e non eseguiti.

## 5 — Registro anti-replay

La protezione permanente del pacchetto canonico usa sessione, sorgente e
sequenza monotona; rinnovi e riacquisizioni non azzerano la sequenza. Gli UUID
recenti hanno una finestra di un secondo misurata alla ricezione e una quota
di 512 per sorgente, fino a 2560 con cinque sorgenti. Superare la quota disarma
soltanto quella sorgente e restituisce un rifiuto operativo.

Gli UUID non costituiscono più un registro illimitato per l'intera sessione.
Alterare UUID/sequence/time produce un messaggio differente: il contratto
autenticato resta il confine di fiducia. Il principio delle sequenze e delle
finestre limitate è confrontato con [RFC 4303, §3.4.3](https://www.rfc-editor.org/rfc/rfc4303.html#section-3.4.3);
SC1 non implementa il protocollo ESP. Revisionati stress di 6000 azioni,
scadenza, rinnovi, riacquisizione, duplicati tra sorgenti, saturazione isolata e
recupero. Regressioni native non eseguite.

## 6 — Storici e ammissione temporale

Prima del grant SI5, TholusOS raccoglie le ricevute reali della route qualificata e
chiede `GET /immersive/v1/spatial/history`. La risposta autenticata elenca soltanto
metadata pubblicati e depth effettivamente conservate con fence completata, nello
stream/proiezione correnti. Tutti i candidati della finestra operativa devono
esistere in entrambi gli insiemi; nessun FPS o ritardo della pipeline viene
inventato. Una misura ottica valida può quindi risultare incompatibile con le
risorse attuali e viene rifiutata prima del grant, consentendo un nuovo tentativo.

SC1 esporta 65 ms di prova per coprire i 60 ms ammessi dell'evento e il margine
limitato del clock bridge; il default Sagitta resta 150 ms. Il prefisso esportato
non può arretrare rispetto all'ultima prova confermata. L'engine conserva anche i
metadata pre-grant pubblicati e li associa alla sessione ammessa, conservando
camera, tempo di cattura e istanze originali.

Durante la sessione il produttore ricontrolla ogni candidato e la depth reale.
La perdita dello storico è un rifiuto operativo: disarma gli input mantenendo il
grant rinnovabile. Un hint positivo dell'host non può sostituire una prova valida;
il recupero richiede un nuovo rilascio acquisito dopo la riattivazione. I frame
mai pubblicati e le riscritture restano errori di protocollo. Gli ID realmente
ritirati hanno un registro limitato, distinto dai buchi nella numerazione.

La selezione deve chiudersi entro i 60 ms dell'evento. Prima della prima query,
ogni colpo acquisisce copie immutabili e lease di tutti gli otto candidati al
massimo. Il limite totale resta 250 ms per completare le query; le lease restano
nel budget GPU condiviso di 192 MiB, senza attese sul render thread. Un candidato
mancante rifiuta l'intera azione e disarma i nuovi input della sorgente.

Revisionati i casi di history a 400 ms incompatibile con le depth reali, warmup,
budget/fence mancanti, frame ritirati o mai pubblicati, lease e metadata durante
l'evizione, recupero e intervalli con vuoti anche di un microsecondo. Regressioni
native non eseguite. Il controllo localizzazione host non segnala nuove anomalie.

## 7 — Feedback dell'esito

L'esito usa uno stato distinto dalla mira, con UV congelate all'azione. Attesa,
hit, miss e rifiuto hanno simboli differenti: anello tratteggiato, cerchio con
spunta, croce diagonale e rombo con punto esclamativo. Colore e punti conservano
l'identità dei cinque puntatori. Se manca una posizione attendibile, il rifiuto
usa un indicatore di sorgente, senza inventare un punto colpito.

L'esito dura 500 ms dalla risoluzione o dal rifiuto effettivi, con dissolvenza
finale; l'attesa resta entro i 250 ms originali. Aim, rilascio e duplicati di
trasporto non riscrivono l'esito. Una risposta asincrona più vecchia non può
sovrascrivere l'azione più recente. Pausa/cancel/cambio sessione eliminano gli
stati pertinenti; la sola perdita dello storico conserva un hit già confermato
e mostra rifiutata una richiesta pendente annullata. I motivi restano nello
stato diagnostico autenticato. Nessun danno o comportamento SC2 è stato aggiunto.

Revisionati stati, scadenze, duplicati, sorgenti indipendenti e collegamento delle
uniformi ai due shader. Regressioni e shader non compilati/eseguiti; leggibilità
fisica e prestazioni richiedono ancora verifica sul sistema di proiezione.

## 8 — Regressioni e runner

Il target `scintilla_input_regression` è collegato a CMake/CTest con il flag
`ENABLE_RELAY_TEST_TOOLS`, disabilitato per default. Può essere configurato anche
separatamente dal gioco, usando soltanto il core C++ e la libreria standard.
Il registro condiviso espone 33 casi nominati; i controlli `CHECK` restano attivi
con `NDEBUG`. Comandi futuri e limiti sono nel [README del target](../../../../tools/immersive/tests/README.md).
Nessuna configurazione CMake, compilazione o esecuzione nativa è stata avviata.

Le nuove fixture coprono cinque sorgenti e perdita/riacquisizione indipendente,
ray picking con camera storica diversa dalla camera corrente e 4800 colpi primari
in oltre 20 minuti, a circa 3,984 Hz, con rinnovi indipendenti ogni 500 ms. Sono
simulazioni deterministiche, non misure di throughput o latenza reali.

TholusOS usa lo stesso helper HTTP in produzione e nei test con trasporto
iniettato: serializzazione canonica, token, loopback, redirect negati, dimensioni,
clock a quattro timestamp, rinnovi, risposte operative/protocollo e cancellazione.
Due test del controller reale con API simulata verificano pausa/ripresa e pausa
rifiutata. Le regressioni del package SI5 mantengono separata la verifica di
pre-grant retry e del latch di isolamento dopo un grant.

Il profilo `THOLUS_RUNTIME_TEST_PROFILE=scintilla` del runner isolato host seleziona
cinque suite SC1 e soltanto quattro metodi della suite di integrazione: 50 metodi,
almeno 87 casi parametrizzati. Rifiuta casi mancanti, saltati o falliti. Se avviato
in futuro compilerà il test host nativo, senza build dell’artefatto di produzione,
smoke o catture. Per questo **non è stato avviato**. I 28 controlli Python del
runner sono invece passati con temporanei sul SSD esterno.

Revisioni indipendenti su target, fixture, helper, delega e selezione dei test
non hanno rilevato blocker nel sorgente. Le cinque verifiche sorgente/matematiche
`CheckRelayProjectionSource.py` sono passate, inclusi 4000 round trip indipendenti.
Sono verifiche di codice e matematica; GPU, proiezione e hardware restano da
qualificare.

## 9 — Blocco del servizio HTTP ereditato

Il servizio locale usa un pool non bloccante di massimo 16 client con quote di
lettura/scrittura e scadenze assolute: 2 s per gli header, 5 s per l’intera
richiesta, 30 s per il risultato di gioco, 2 s per la risposta e 37 s complessivi.
Un client con header/corpo incompleti o che non legge la risposta non monopolizza
più il worker. I limiti sono 8 KiB header, 64 KiB corpo e 1 MiB risposta.

Il framing controlla soltanto gli header, rifiuta lunghezze negative/ambigue,
duplicati e transfer encoding non supportati, autentica prima di accumulare
ulteriormente il corpo e inoltra soltanto un corpo completo. HTTP/1.1 richiede
Host; resta il preflight OPTIONS senza corpo. Queste scelte sono confrontate con
[RFC 9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3).

Il worker possiede e chiude i socket. L’arresto segnala un wake socket non bloccante;
non chiude da un altro thread descrittori che potrebbero essere riutilizzati.
I comandi mantengono la coda FIFO sul thread del gioco tramite ticket asincroni:
la rete non attende la chiamata sincrona da 30 s. I ticket cancellati liberano la
coda; una route già iniziata conserva gli effetti potenzialmente avvenuti e
richiede un refresh dello stato prima di un eventuale nuovo tentativo.
Il comportamento di readiness è verificato sul testo ufficiale di
[POSIX poll](https://pubs.opengroup.org/onlinepubs/9799919799/functions/poll.html).

Il timeout di poll è limitato a 1 ms con job pendenti, 5 ms altrimenti: non è una
misura né una garanzia della cadenza effettiva dei callback. Il flag API esclude
l’implementazione dal percorso vanilla e Windows conserva lo stub esistente.

Revisionati sette test del trasporto con socketpair e quattro del dispatcher
reale con router registratore: richieste parziali affiancate a clock/update sani,
autenticazione anticipata, EOF, half-close, buffer, limiti, deadline, fairness,
FIFO, cancellazione, backpressure e stop. I target CTest contengono quindi 44
casi complessivi (33 core, 7 trasporto, 4 dispatcher). Tutti i nuovi test sono
sorgenti revisionati; nessuna socketpair o regressione nativa è stata eseguita.

## Stato della consegna

Correzioni software dei punti 4–9 completate e ricontrollate nel sorgente, con
checkpoint separati prima del passaggio al punto successivo. Nessuna build o
qualifica runtime/hardware; i gate SC0 restano aperti e SC2 non è avviata.
La verifica finale comprende controlli Python del runner, matematica della
proiezione, localizzazione (1646 file Swift, zero nuove anomalie), whitespace,
link locali, hash e integrazione mediante elenco esplicito preservando il WIP.

I checkpoint e i limiti delle verifiche sono registrati in [verification.json](verification.json).
