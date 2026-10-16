# SC5 — Regressioni da implementare e verificare

**Matrice di lavoro, non risultati di test.** Il calcolo Python allegato verifica soltanto il modello matematico. Usare core C++11/CTest esistenti, package Swift Testing e runner app isolato; fixture con clock/transport deterministici e percorso reale di orchestration. Non sostituire regressioni funzionali con regex sorgente. Native/GPU/hardware vanno eseguiti soltanto se autorizzati.

| ID | Caso | Risultato richiesto | Livello minimo |
|---|---|---|---|
| R01 | P2…6 su giri nativi | Formula e conteggi del JSON, nessun pilota nella crew | core |
| R02 | C1–2 | Kit universale, nessuna professione obbligatoria | core+UI |
| R03 | Handoff normale, forzato, skip che completa, retry | Turno avanza solo sul vero commit, anomalie registrate | core+session |
| R04 | Perdita tracking/sostituzione device | C e ruoli non collassano, nessuna identità inventata | input+host |
| R05 | Pause/reprepare/maintenance/reset | Quota/crediti conservati stessa gara; reset con nuova race pulito | integration |
| O01 | Tutte le disponibilità C1…5 | Ogni assegnamento iniettivo, bundle completo e ≤C_disponibile | core |
| O02 | C3 offensiva e minaccia nuova | Difesa suggerita come alternativa; nessuna quarta mano | core |
| O03 | C4 e C5 concorrenza | 3+1 e 3+1+1 ammessi soltanto quando fatti eleggibili | core |
| O04 | Team in testa senza rivali frontali | Difesa/recupero/risorse reali oppure respiro; nessun falso rivale | integration |
| O05 | Held reale su compito non suggerito | Resta occupazione, nessun doppio task né cancel artificiale | core+input |
| O06 | Ruolo affinità contro attesa | Affinità non monopolizza; lemma simmetrico round-robin verificato | core |
| O07 | Target scade fra schedule e frame/commit | Indicazione rimossa, nessun effetto dal suggerimento | integration |
| O08 | Validazioni negative e nuovo hit stesso batch | Invalidazione sticky sopravvive, no continuità inventata | input |
| O09 | 7 bundle, k=5, F1000 e attese saturate | Bounded, no overflow int32, determinismo tra ordini input | core |
| C01 | Apertura+combo vs solo ultimo colpo | Crediti distinti dai commit reali, nessuna doppia attribuzione | core+adapter |
| C02 | Full inventory, falso contrasto, effetto rifiutato | Nessun credito; motivazione coerente | core+adapter |
| C03 | Riordino/retry/stato letto ripetutamente | Contatori non incrementano due volte | core+HTTP |
| C04 | Cambio persona su stesso sourceId | Crediti seguono participantId; dato vecchio scartato | integration |
| H01 | Operatore→cambio→rebind→challenge→prepare→Ready | Stadi distinti, stesso producer e grant, gara ripresa | app+native |
| H02 | Stesso flusso da telefono/auto-confirm/skip/API | Nessun bypass gate; native cambio non attende challenge futuro | app+native |
| H03 | Mappa proposta dopo pilota successivo | Biiezione, sorgente entrante passa a uscente, altre invariate | core+host |
| H04 | Fisica durante handoff/rebind/countdown | Tutti i kart congelati, tick missione/cavo non avanzano | native |
| H05 | Challenge con held precedente o press accodato | Nessuna conferma; servono stato neutro noto e nuovo gesto | pump+host |
| H06 | Challenge di altra sorgente/persona/turno/epoch | Rifiuto; nessun gesto riutilizzato | host+native |
| H07 | Stato perso, timeout30s, retry, risposta HTTP persa | Rimane held; retry idempotente; niente restart produttore | app+native |
| H08 | Completamento GPU del vecchio driver | Nessun effetto sul nuovo turno né consenso Party | input+native |
| H09 | Route/maintenance/rebind dopo challenge | Prova invalidata se cambia autorità, recupero visibile | app+native |
| H10 | Stessa mappa retained, mappa cambiata gesture | Retained solo con continuità dimostrata, neutralità per tutti | host+native |
| H11 | Driver→supporter e ritorno due cicli completi | Ruoli, credito, quota e assegnazioni corretti | native+app |
| P01 | Party off, input diretto sul nodo | Nessun effetto/consenso e nessun tiro world attraverso HUD | input+core |
| P02 | C1 hold0,75; C2 due persone | Uno/due voti reali; F60,61,120,144,1000 | core |
| P03 | Due sorgenti stessa persona / stesso voto replay | Mai quorum doppio | core+input |
| P04 | Primo voto rilasciato o perso prima del secondo | Quorum invalido; nuovo hit non lo ripristina | core+input |
| P05 | Tick prima, esatto e dopo deadline3s | Solo prima valido; scadenza precede commit | core |
| P06 | Reset input, rebind, reconnect, 200retry | Una quota per turno, non per dispositivo/epoch | core+integration |
| P07 | E=R+15, un'unità Q sotto, successo/rifiuto fisico | Costo esatto solo successo; riserva mai violata | core+adapter |
| P08 | Normal boost e Party maturi stesso tick | Vince normal, un addebito, Party release-required | adapter |
| P09 | Minaccia/impatti/finale/handoff nello stesso tick | Niente nuovo Party, niente effetti tardivi | native |
| P10 | Freno già premuto e freno durante burst | Veto prima; dopo decelerazione senza floor/extra force; percorso PlayerController reale | Bullet/native |
| P11 | Acceleratore+freno, slope, velocità quasi zero | Ordine PA_BRAKE/PA_ACCEL reale, clamp e numeri finiti | Bullet/native |
| P12 | Pad zipper prima/durante/dopo, nitro/skid/SC3boost | No stacking ammesso, nessuna cancellazione bonus altrui | native |
| P13 | Δv fisica dopo Bullet, normale e angolare | Una sola variazione tangenziale ≤1; caso t4/n3 non diventa +2; cap totale; SC3 conservata | Bullet/native |
| P14 | Zona nonqualificata, confine, uscita durante voto | Rifiuto visibile senza addebito; nessuna ammissione da offline JSON | core+native |
| P15 | Rescue/teleport, direzione inversa, hash pista diverso | Zona invalida e consenso cancellato | native |
| P16 | Usato→cambio reale→nuovo turno | Quota nuova solo sul commit; credito vecchio preservato | core+native |
| U01 | NodoParty bordi, rapporto2:1, domemaster, dimensioni | Stessa maschera CPU/shader, niente bleed world | numeric+GPU |
| U02 | WorldOffense invariata durante nuovo allarme | Nessuna invalidazione dovuta a threatId fuori contesto | input |
| U03 | Old frame layout2 e currentlayout3, o altro turno | Rifiuto corretto senza revocare grant sano | input |
| U04 | Costo15, 1/2consensi, deadline, release, Used, veto | Feedback distinto e leggibile, accessibilità, EN/IT | UI+projection |
| U05 | Vecchio descriptorv2, traceparserv3 discordante | Qualifica respinta; schema/fingerprintclosed | C+++Python |
| U06 | GPU90ms, gap disponibilità, prefetch futuro | Restano respinti come correzione SC4 | C+++Python+native |
| U07 | Introstandard completata poi selezioneParty | Necessaria variante corretta; skip/errore richiedono nuovo renderACK | app+GPU |
| A01 | UInt64 oltre2^53, max, overflow; unknownfield/bodyoversize | Nessun arrotondamento o bypass | parser+Swift |
| A02 | Requestretry sameUUID e body differente | Idempotenza esatta/conflitto, bounded64 | core+HTTP |
| A03 | IntentSC5 in volo, Ready operatore/telefono/API | Requisiti atomici, nessuna gara parzialmente preparata | integration |
| A04 | Cancel Task ma native ha completato | Riconcilia stato, no rollback presunto, no costo duplicato | host |
| A05 | Newhost/oldnative e viceversa | Capability esplicite, nessun downgrade silenzioso | HTTP+host |
| F01 | Quattro esiti naturali SC4 | Invariati con ruoli e Party | native+DTO |
| F02 | Partyenabled mai usato / usato | Entrambi bucketParty, contatore diverso | core+report |
| F03 | Finish stesso tick inputGPU o quorum | Envelope congelato prima degli effetti tardivi | native |
| F04 | Forcefinish/abort/reset/results→Pronto | not_completed o nuova race corretti, vecchio report immutabile | app+native |
| F05 | JSONpersistito↔GET↔package↔app | Stesse identità, profilo, quota e crediti | integration |
| V01 | Vanilla flagoff e crewlegacy | Comportamenti preesistenti, nessun nuovo effetto/endpoint efficace | build+native |
| V02 | SC0–SC4 e tre correzioni recenti | Regressioni e gate conservati | suitesexisting |

I casi nativi senza hardware possono usare input/transport controllati e rendering locale autorizzato, ma devono essere marcati sintetici. Gesto fisico, identità del portatore, leggibilità cupola, comfort e divertimento restano prove fisiche/umane distinte. SC6 non viene eseguita da questa matrice.
