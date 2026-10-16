# SC4 — correzioni del riesame del 16 ottobre 2026

Tre difetti corretti nei sorgenti, in ordine, con revisione del precedente prima del successivo. Nessuna build, compilazione, esecuzione Swift/C++, app, gioco o GPU. SC5 non avviata. Le verifiche Python offline e dei contratti sorgente non dimostrano il funzionamento nativo o fisico.

## 1. Ritorno dall’introduzione sulla stessa route

La conferma di rendering ora identifica ogni tentativo con generation, preparationID e cycle UUID. Skip, annullamento, errore prima del player e riprova richiedono una nuova conferma. Il renderer conserva mesh/risorse della route ma conferma il nuovo ciclo solo dal completamento Metal pertinente. Callback obsolete e successi tardivi di un ciclo già fallito non riaprono il gate.

Revisione: percorso controller → MediaBus → frame STK → renderer → completamento → gate, entrambe le composizioni video, cancellazione e riprova. Aggiunte regressioni Swift e controlli sorgente; i test Swift sono registrati ma non eseguiti.

## 2. Disponibilità GPU coerente con le capsule

Ogni trace registra la disponibilità CPU effettiva readbackUs/readbackTick e il crossing activationUs. Una prova catturata a c e letta a r è utile nell’intervallo chiuso [r, c + 50000 µs]. Per una catena continua, r_i ≤ c_(i−1) + 50000. La durata utile parte dal primo readback, senza accreditare il tempo atteso sulla GPU; l’attivazione richiede un prefetch già disponibile e fresco al crossing. Native verifier, validatore Python e runtime condividono la stessa condizione di disponibilità. Le vecchie trace senza i campi obbligatori richiedono una nuova acquisizione.

Controesempi: a 30 Hz, 20 ms di latenza lasciano vuoti fra prove, mentre 90 ms superano completamente il limite di freschezza. Un risultato osservato dopo il crossing non prova retroattivamente la disponibilità. Il cavo mantiene il contratto geometrico precedente di almeno 3 s; il timeout query di 250 ms resta un limite di risorse.

Revisione: 31 test Python del validatore e 10 del collector; oracle matematico indipendente con 20000 storici casuali; regressioni native aggiunte, registrate e revisionate, non compilate né eseguite. Nessuna trace reale né descriptor qualificato è stato prodotto.

## 3. Upgrade SC3 → SC4 e preflight recuperabile

Lo snapshot espone la qualifica soltanto se il fingerprint corrente corrisponde alla cache, senza rileggere file dal GET. La selezione SC4 controlla la cache e precede l’intent con un preflight esplicito quando necessario. Con un consumer già presente, una transazione autenticata begin/resume sospende l’input nativo mentre SI5 continua a rinnovare il grant e a drenare gli eventi. La scansione non scade artificialmente il consumer. Il ticket lega produttore, gara, sessione, route ed epoch; i retry sono idempotenti e mantengono sequenze e anti-replay.

Resume richiede un rinnovo SI5 reale e tre clock aggiornati. La ripresa ricostruisce le prove video e richiede rebind logico e normale preparazione, mantenendo Pronto chiuso. Errori recuperabili e annullamento dell’intent tentano il ripristino della stessa autorità fisica disarmata; non confermano un intent obsoleto. La qualifica mancante offre una riprova esplicita, senza scansioni pesanti ripetute dal polling. Revoca fisica o cambio di route/produttore restano soggetti all’isolamento esistente.

Le API e la procedura sono descritte in [SC4_API.md](../sc4-implementation-2026-10-16/SC4_API.md) e [OPERATORE.md](../sc4-implementation-2026-10-16/OPERATORE.md).

## Verifica e limiti

I controlli eseguiti sono elencati in [VERIFICHE.json](VERIFICHE.json). I nuovi casi Swift/C++ sono codice di regressione consegnato, non risultati nativi. Restano da autorizzare build e test nativi, ritorni intro reali, upgrade da SC3 preparata, errori HTTP simulati nel processo reale e acquisizioni GPU qualificanti; successivamente le prove SI5/ottiche e venue.

Il lavoro originale non pertinente è conservato. Le modifiche sono state preparate nei worktree isolati esistenti e trasferite per delta con controllo SHA-256 contro modifiche concorrenti. Il manifest della precedente implementazione resta una fotografia storica e non è stato riscritto.

Reset, abort e termine forzato attendono la conclusione della verifica in corso prima di cambiare l’autorità della gara. Se la manutenzione non è stata recuperata, il comando resta bloccato e il grant SI5 non viene chiuso. Un nuovo cambio di modalità non può interrompere una manutenzione in corso.
