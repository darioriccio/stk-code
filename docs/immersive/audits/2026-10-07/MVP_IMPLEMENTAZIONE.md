# MVP SuperTuxKart / TholusOS — 7 ottobre 2026

> Documento storico della fase iniziale. Per la successiva implementazione del piano, senza build, vedere [il resoconto aggiornato](MVP_COMPLETA_IMPLEMENTAZIONE_2026-10-07.md).

## Stato della consegna

Le modifiche sono integrate nei repository principali di SuperTuxKart e SocialTableController. Il lavoro preesistente è stato preservato, comprese le modifiche indipendenti alla distribuzione dei giochi in `GamesWorkspaceStore.swift`. Non sono stati eseguiti push, installazioni o aggiornamenti della versione upstream.

La verifica finale è sospesa su richiesta dell'utente per il carico del Mac. I processi di build avviati per questo incarico sono terminati o sono stati interrotti. Non è attestato il funzionamento completo della MVP.

## Funzionalità implementate

- Minimo un pilota, con telefoni opzionali. Un telefono può partecipare anche alla gara con un solo pilota e usare carica/attivazione del turbo.
- Avvio del producer locale da TholusOS, con gestione del solo processo avviato dall'integrazione e percorso del launcher configurabile.
- Catalogo delle piste effettivamente installate: Snow Peak, Around the Lighthouse e Oliver’s Math Class, disponibili quando presenti negli asset del producer.
- Configurazione che carica una gara reale con un giocatore alla tastiera e avversari AI; verifica del percorso effettivamente caricato prima di procedere.
- Sequenza preparazione, briefing, countdown e gara; pausa, risultati e reset della gara reale per rigiocare.
- Comandi che modificano il mondo eseguiti sul ciclo principale del gioco. Correzione della conclusione naturale all'ultimo giro e della gestione delle camere immersive durante il cambio pista.
- Launcher dedicato con Syphon e configurazione conservativa del rendering; identificazione del producer Syphon come `SuperTuxKart`.

## Verifiche effettuate e limiti

| Verifica | Esito |
| --- | --- |
| Revisione dei percorsi del codice, sintassi degli script e controllo whitespace delle modifiche | Completata |
| Build STK con API e Syphon | Completata prima della sospensione |
| Build STK incrementale dopo la correzione del turbo per pilota singolo | Completata prima della sospensione |
| Configurazione CMake senza integrazione | Completata; build non avviata |
| Compilazione del ricevitore diagnostico Syphon | Completata; ricezione dei frame non verificata |
| Primo controllo del producer reale | Caricamento della pista e publisher osservati; controllo interrotto dall'assenza di azioni telefono utilizzabili nel caso singolo |
| Correzione di quel caso singolo | Implementata e ricompilata; controllo runtime non ripetuto |
| Build/test TholusOS | Interrotti su richiesta dell'utente prima dei target app/test; compilazione finale e test non attestati |

I test di regressione e gli strumenti di verifica sono presenti nei sorgenti. Restano da eseguire, quando la fase sarà riaperta: compilazione e test pertinenti di TholusOS, ripetizione del controllo API sulle tre piste dopo la correzione del turbo, ricezione Syphon e verifica del flusso completo nell'app. La proiezione fisica richiede una verifica separata.

## Percorsi utili

- Audit iniziale: `audit-2026-10-07/AUDIT.md`.
- Launcher: `Apri Immersive Relay TholusOS.command`, collegato a `stk-code/tools/immersive/RunRelayTholusOS.command`.
- Script di build dedicato: `stk-code/tools/immersive/BuildRelayMVP.sh`.
- Controllo del producer: `stk-code/tools/immersive/ProbeRelayMVP.py` e `audit-2026-10-07/mvp_validation.py`.
- Contratto consumer: `SocialTableController/Modules/GameShowControl/ImmersiveSuperTuxKartIntegration.md` nel progetto Social Table App.
- Evidenze STK: `audit-2026-10-07/mvp-stk-build.log`, `mvp-stk-rebuild.log` e `evidence/`.
- Evidenze TholusOS: `/Users/dario/Dev/worktrees/tholus-stk-mvp-20261007/output/stk-mvp-20261007/`.

I worktree isolati e gli output dedicati sono conservati per riprendere la verifica senza ricostruire la baseline. Nessuna cache condivisa è stata rimossa.
