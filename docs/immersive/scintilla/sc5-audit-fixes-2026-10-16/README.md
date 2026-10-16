# SC5 — Correzione dei cinque difetti dell’audit

Data: 16 ottobre 2026. Ambito: codice e controlli software senza compilazione; SC6 non avviata.

Le cinque correzioni sono state affrontate nell’ordine sotto, ciascuna con rilettura dei chiamanti/consumatori e revisione indipendente dei sorgenti prima del passaggio successivo. Il controllo incrociato finale non ha individuato altri difetti concreti nei percorsi modificati. Questo esito non costituisce una verifica nativa o fisica.

| Punto | Correzione | Regressione e ricontrollo |
|---|---|---|
| 1 — cambio pilota | L’host conserva le associazioni durante lo stadio A; la sospensione parte sul commit del pilota effettivo o sul nuovo turno nativo. Lo snapshot handoff viene aggiornato prima di propagare la sospensione. Il gate nativo dello stadio B resta chiuso prima della fisica. | Nuova regressione del RuntimeController per cambio operatore/automatico; conservazione del grant e sospensione dopo commit. Due invocazioni registrate, non eseguite. |
| 2 — tracking | Una risposta Crew non terminale con associazioni non verificate e grant ancora esistente attiva il recupero anche nello stesso turno. Il profilo ricevuto viene pubblicato prima di sospendere; il flag impedisce ripetizioni a ogni poll. La UI usa il percorso held già presente: pausa tecnica, rebind, possesso, preparazione, Ready esplicito. | Regressione con verifica precedente presente/assente e polling ripetuto; due invocazioni registrate, non eseguite. Preparazione in corso, risultato terminale e stadio A restano protetti. |
| 3 — offensiva | Azione operativa distinta dalla competenza: apri, mantieni, attendi breccia, spara, contributo registrato. Un solo apritore anche con due Universali; il proprietario reale SC2 occupa il suo slot e mantiene il fascio. Fase/owner/contributi invalidano subito il suggerimento. | Due nuovi casi C++ registrati per C1–C5, cambio volontario di apritore, breccia, contributore già contato e fatti contraddittori; DTO Swift e negoziazione capability aggiornati. Nessuna esecuzione nativa. |
| 4 — avviso difensivo | L’avviso è strutturato con persona, minaccia e scadenza; raggiunge frame, shader e snapshot host. Prevale sui suggerimenti facoltativi e indica il settore, rimuovendo solo il colore della persona dal vecchio bersaglio. Non genera rilasci o altre azioni. | Caso C++ registrato per rilascio, cambio/rimozione minaccia e hold; DTO rifiuta piloti, duplicati, dati non attivi o malformati. Verifica nativa pendente. |
| 5 — reticolo Party | In SC5 i pannelli legacy sono sotto l’HUD tradotto; i reticoli, i puntini identificativi e l’anello held vengono disegnati sopra. Il feedback spaziale esterno all’HUD sopravvive. Il risultato terminale e il percorso SC1–SC4 conservano i propri confini. | Tre nuove regressioni Python aritmetiche/sorgente: composizione su cinque dimensioni e cinque colori, pannello legacy, confini terminali/legacy. Controesempio respinto con lo shader originario. Suite render registrata in CTest. GPU non eseguita. |

## Contratto aggiornato dei suggerimenti

La capability SC5 aggiunge **`guidanceRevision: "2"`**, obbligatoria e tipizzata come stringa. Il nuovo host richiede questa revisione; i vecchi decoder chiusi rifiutano il campo aggiuntivo, evitando l’attivazione SC5 con un payload dei suggerimenti incompatibile. SC4 resta una selezione distinta. Contract generale, revisione regole gameplay, profili, layout v3 e identità wide restano quelli del piano.

Ogni elemento `suggestions` aggiunge il campo obbligatorio nullable `offenseAction`: per `taskKind="offense"` ammette `open_shield`, `maintain_shield`, `wait_for_breach`, `fire`, `contribution_complete`; per gli altri task è `null`.

Lo snapshot Crew aggiunge il campo obbligatorio `releaseAdvice`, array bounded a cinque elementi, vuoto fuori dalla gara attiva e nel risultato terminale. Ogni elemento ha esattamente `participantId` (supporter corrente), `threatId` e `expiresAtTick` (identità uint64 decimali canoniche, scadenza esclusiva). Può affiancare un suggerimento della stessa persona perché rappresenta l’alternativa di interromperlo, non un secondo compito simultaneo. Non è un comando input.

Queste estensioni completano il [contratto del piano](../sc5-plan-2026-10-16/CONTRATTO_SC5.md); il [resoconto di implementazione precedente](../sc5-implementation-2026-10-16/README.md) rimane evidenza storica antecedente alle correzioni.

## Verifiche effettivamente eseguite

**126 controlli sorgente, asset e numerici passati**, in dieci suite Python: proiezione, Crew physics/wire/render/report, SC4 HUD/API/finish/lifecycle/presentation. Dettagli e output integrali in [verifiche.json](verifiche.json). Controllati inoltre diff, AST Python del runner, registrazione dei 31 casi Crew C++ e delle nuove invocazioni app Swift. Nessun test è stato eliminato o disabilitato.

Le tre nuove regressioni C++, quattro invocazioni app e nove scenari package Swift sono scritti/registrati o scoperti dal package, **non compilati né eseguiti**. Build, C++/Swift, app, GLSL/GPU, Bullet, ciclo completo e hardware non eseguiti. La qualifica reale del layout v3 e delle zone Party resta aperta; nessun dato sintetico è stato promosso a prova operativa.

[Manifest delle modifiche](source-manifest.json) e [patch rispetto alla baseline WIP](correzioni.patch) distinguono questo incarico dal lavoro precedente. Le correzioni sono preparate in due worktree sul SSD e riportate nei checkout originali solo dopo controllo degli hash, senza reset, stash, staging, commit o push.

I due worktree temporanei di questo incarico sono stati rimossi dopo verifica dei contenuti; le tre versioni iniziali di file modificati nel frattempo da altre chat sono conservate soltanto come evidenza nel task sul SSD. I file concorrenti correnti e gli altri worktree non sono stati modificati. Dettagli in [cleanup.json](cleanup.json).
