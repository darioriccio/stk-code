# SuperTuxKart Immersive Relay

## Ambito e autonomia

Completa la richiesta corrente con la soluzione più semplice e verifiche
proporzionate. Rispetta sola lettura e fasi autorizzate: roadmap, brief e checklist
non autorizzano nuove funzionalità o il completamento dell'intero MVP.
Procedi con assunzioni ragionevoli per scelte reversibili; chiedi input solo se
manca una decisione, un accesso o un'autorizzazione indispensabile.

Controlla lo stato Git prima delle modifiche e preserva il lavoro altrui. Se il
checkout contiene modifiche estranee, usa un worktree isolato. Le istruzioni
esplicite dell'utente prevalgono sulle linee guida delle skill; attivale solo se
pertinenti. Se una skill impone una pausa, cita file e regola che la richiedono.

## Vincoli permanenti

- Mantieni il fork sottile, con hook minimi e moduli separati; preserva vanilla.
- Separa gameplay, camera/proiezione e trasporto video; isola le API di piattaforma.
- Usa flag CMake/runtime e convenzioni esistenti, verificando nomi e codice reali.
- Swift/TholusOS gestisce sessioni venue e telefoni; STK gestisce gameplay,
  rendering e API locale autenticata. Valida input, duplicati e versioni obsolete.
- Non bloccare il render thread con rete/I/O. Mantieni reset e recupero operabili.
- Genera l'output immersivo dalla scena 3D, con camera stabilizzata e configurabile.
- Conserva licenze, crediti e provenienza degli asset. La revisione legale per
  distribuzione commerciale riguarda quella fase, non le modifiche locali.

## Documentazione da consultare

Leggi solo ciò che serve al compito. I percorsi seguenti sono relativi al repo.

- **Integrazione attuale:** `docs/immersive_relay_tholustos_integration.md` è il
  contratto STK/TholusOS congelato; `docs/immersive_relay_swift_integration_contract.md`
  ne approfondisce i dettagli. In caso di divergenza prevale il contratto TholusOS
  per la superficie congelata; verifica il codice e segnala eventuali discrepanze.
- **Gameplay/API:** `docs/immersive_relay_phase2_api_contract.md`, documento Phase 3
  per il Crew Event Director e Phase 5 per il ciclo arcade, secondo l'area toccata.
- **Difficoltà adattiva:** `docs/immersive_relay_adaptive_difficulty.md` descrive
  l'implementazione; il corrispondente `_brief.md` conserva la proposta originale.
- **Rendering/output:** documenti Phase 4 e Phase 4.6. Il contratto TholusOS distingue
  il preset `fulldome_safe` dal target 2048; non promuovere un target senza evidenza.
- **Build/rebase:** `README.md`, `INSTALL.md`, `CMakeLists.txt` e
  `docs/immersive_relay_upstream_sync.md`, quando pertinenti.
- **Prodotto:** `docs/supertuxkart_immersive_relay_master_spec.md` contiene obiettivi
  e roadmap, non certifica lo stato implementato. `docs/immersive/repo_audit.md`
  conserva l'audit: aggiornalo solo se cambia un risultato pertinente.

Le limitazioni dei documenti di fase valgono per quella fase. Non interpretarle
come divieti permanenti per un nuovo lavoro esplicitamente richiesto.

## Verifica e consegna

Scegli i controlli in base al comportamento modificato: test mirati per logica/API,
build vanilla/relay per flag o hook condivisi, receiver e smoke grafico per output.
Usa i comandi pertinenti nei documenti, verificando i percorsi di build disponibili.
Per prove relay avvia con `--immersive-relay`; per il flusso sala giochi aggiungi
`--relay-arcade-ui`. Aprire la sola `.app` non attiva quei flussi.

Le checklist venue/M1 e i playtest umani sono criteri di accettazione del prodotto,
non prerequisiti di ogni patch. Simulazioni e prove su altro hardware non li chiudono.
Per sole modifiche documentali controlla diff, link e coerenza, senza build del gioco.
Dopo controlli riusciti, ripetili o ampliali solo con nuove modifiche o evidenze.

Concludi con risultato, verifiche effettivamente eseguite e limiti rilevanti.
Distingui prova locale, hardware reale e accettazione venue; evita resoconti rituali.
