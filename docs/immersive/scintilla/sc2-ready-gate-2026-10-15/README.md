# SC2 — Gate di Pronto, 15 ottobre 2026

Correzione del punto 1 dell'audit SC2. Perimetro: preparazione iniziale e
ammissione di Pronto/start/countdown/resume. Gli altri tre rilievi dell'audit
restano aperti. SC3 non avviata.

## Comportamento e contratto additivo

STK distingue l'intento **SC2 richiesta** dalla policy già attivata e dal grant
input attualmente pronto. L'intento da solo non attiva powerup, scudi, combo o
conseguenze fisiche. Finché SC2 è richiesta, una gara non preparata non può
entrare nel countdown tramite Pronto, tastiera, SDL o i comandi operatore.

- `GET /immersive/v1/scintilla/state` aggiunge `required: boolean`. È vero se
  l'operatore ha richiesto SC2 oppure se la policy è già attiva. Gli altri campi
  del contratto `tholus.stk-scintilla-combat/1` restano disponibili.
  `intentRevision` è una stringa decimale e cambia a ogni POST intent riuscita.
- `POST /immersive/v1/scintilla/intent` ammette esattamente `contract`,
  `expectedRaceEpoch`, `expectedIntentRevision` (stringhe dal GET) e `required`
  (booleano JSON). Restituisce
  lo stato completo. Route autenticata sul dispatcher gioco; nessun grant SC1
  è necessario per dichiarare l'intento.
- Le due identità expected vengono confrontate prima di mutare l'intento:
  una richiesta ritardata nella stessa gara, anche dopo un timeout del client,
  non può sovrascrivere una scelta più recente già accettata. La revisione
  riparte da zero solo insieme al cambio di raceEpoch.
- Il mondo deve essere Relay e held. Richiedere SC2 esige modalità locale
  supportata e crew legacy disabilitata. Tornare alla diagnostica è possibile
  in hold se la policy non è stata attivata; permette anche di uscire da una
  richiesta iniziale fallita dopo un cambio di configurazione.
- Payload invalido: 400 `invalid_combat_intent`; epoch cambiato: 409
  `combat_identity_changed`; contesto incompatibile: 409
  `combat_intent_requires_held_local_world`; tentativo di togliere una policy
  già attiva: 409 `combat_requires_race_reset`. La revisione non viene riusata
  a overflow: 409 `combat_intent_exhausted`, senza mutare l'intento.
- L'intento sopravvive a reset/nuovo mondo nel processo. La policy, gli effetti,
  le finestre e il grant seguono i rispettivi reset esistenti. Ogni nuovo mondo
  richiesto SC2 deve essere preparato di nuovo. Il processo standalone parte
  con diagnostica libera, senza intento SC2.
- Pronto da Risultati, con SC2 richiesta, resetta e restituisce l'evento
  `scintilla_combat_reset_requires_prepare` senza inviare il countdown.
  Il reset operatore resta utilizzabile per recuperare una preparazione fallita.
- La scorciatoia legacy `/session/start`, che sostituisce configurazione e
  roster avviando immediatamente, viene rifiutata con 409
  `scintilla_combat_requires_operator_start` quando SC2 è richiesta. Si usano i
  comandi operatore sul mondo preparato. La diagnostica legacy è preservata.

L'host sincronizza la scelta prima di preparare SC2 e prima di riaprire il gate
del video durante la transizione. Pronto del telefono segue gli stessi controlli
autoritativi degli avvii operatore, comprese identità e cambi durante gli await.
La UI mostra la scelta in conferma fino alla risposta del produttore. Un Pronto
già eseguito prima che la chiusura raggiunga STK non può essere annullato
retroattivamente: la successiva richiesta nel contesto non held viene rifiutata
e l'host conserva il blocco, richiedendo il recupero/reset del mondo.
La prontezza video stabile rimane distinta dalla disponibilità del tracking:
perdere il fascio in una gara attiva non introduce una pausa globale aggiuntiva.
L'esenzione esistente per la conferma di handoff rimane circoscritta alla
rotazione; questa correzione non completa la nuova assegnazione SI5/SC5.

## Verifiche e limiti

I test C++ aggiunti coprono intento prima del grant, preparazione fallita,
readiness corrente, reset che conserva l'intento, impossibilità di togliere una
policy attiva, parsing rigoroso e FIFO intent/Pronto sul dispatcher. Sono test
scritti e revisionati, senza compilazione o esecuzione nativa.

Le nuove fixture Swift coprono 29 scenari: 18 HTTP/contratto e 11 del controller
effettivo con dipendenze HTTP/proprietà del processo simulate, senza inventare
readiness di input o hardware. Includono selezione pendente, failure, scoperta
del profilo, reset da Risultati, cambio fase durante await, preparazione del
produttore differita, compatibilità diagnostica e risposte obsolete. Sono
sorgenti di test scritti e revisionati; nessun test Swift è stato eseguito.

`python3 -B tools/immersive/tests/check_scintilla_combat.py` comprende otto
controlli sorgente/matematici, inclusa la tabella di verità del predicato reale
di avvio e il collegamento del reset da Risultati. Non esegue C++, Swift, shader,
HTTP reale, fisica o interazioni del gioco.

Il guard di localizzazione host ha esaminato 1648 sorgenti Swift con zero nuove
segnalazioni. La sola nuova chiave del catalogo ha traduzioni en/it e i valori
precedenti sono conservati. Il controllo whitespace è passato sui due repository.

Build, configure, typecheck, test nativi, avvio app/gioco e hardware sono esclusi
da questa consegna. Le evidenze precedenti del [checkpoint SC2](../sc2-2026-10-15/README.md)
restano storiche; il relativo manifest rappresenta la consegna precedente alla
correzione, non i sorgenti aggiornati qui.
