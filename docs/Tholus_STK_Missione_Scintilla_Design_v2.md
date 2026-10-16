# SuperTuxKart Immersive Relay — Missione Scintilla
## Design v2: corsa competitiva di equipaggio, scudi, carico e controller spaziali

**Versione:** 2.0 — 13 ottobre 2026  
**Destinatari:** Dario, designer e agenti SuperTuxKart/TholusOS.  
**Target:** adolescenti e ragazzi più grandi; non progettare il profilo principale per bambini piccoli.  
**Stato:** specifica di design proposta, aggiornata sulle indicazioni dell’utente. Non sono state implementate patch, eseguite nuove build o effettuate prove hardware in questa revisione.  
**Precedenza:** sostituisce lo studio `Tholus_STK_Missione_Scintilla_Studio_Design_v1.md` per il design di questa modalità. Non sostituisce né modifica automaticamente il piano comune Spatial Input SI0–SI9.

> **Obiettivo:** un solo kart umano, un pilota al volante e un equipaggio attivo con puntatori. La squadra deve arrivare prima con il carico richiesto a bordo. La crew apre gli scudi avversari, colpisce in modo coordinato, difende il proprio mezzo, recupera componenti e prepara energia. La camera resta solidale alla corsa del kart umano, mai al bersaglio scelto da un cannoniere.
>
> **Vincolo economico:** nessuna ricostruzione della pista, nuovi bivi, ponti mobili, cancelli fisici, traffico ridisegnato o cinematiche 3D obbligatorie. Riutilizzare il motore, la guida, i tracciati, gli effetti pertinenti e la staffetta esistenti. Aggiungere logica di missione, bersagli ed effetti leggeri, non un secondo gioco sovrapposto.

---

## 0. Decisioni e differenze rispetto alla v1

| Argomento | Decisione vigente |
|---|---|
| Condizione principale | **Primo posto + carico completo**. Un buon punteggio di supporto non sostituisce il primo posto. |
| Avversari colpiti | Sobbalzo/arresto breve, obiettivo iniziale circa 1 s di animazione, poi ripartenza. Nessuna eliminazione. |
| Due partecipanti totali | Un pilota + un puntatore: servono **5 colpi validi** sul medesimo avversario. Nessun secondo operatore virtuale. |
| Tre partecipanti totali | Un pilota + due puntatori: A mantiene aperto lo scudo, B colpisce durante l’apertura. |
| Quattro o più partecipanti totali | Un pilota + almeno tre puntatori: A mantiene aperto lo scudo, **B e C, due persone diverse da A e fra loro**, colpiscono durante la stessa finestra. |
| Camera | Segue sempre il kart umano secondo il profilo immersivo; non ruota verso il bersaglio, non si arresta per facilitare la mira. Restano le pause e il cambio pilota sicuro. |
| Difesa | Gli avversari attaccano anche il kart umano. Esistono preavviso, attacco leggibile, contromisura e conseguenza concreta. |
| Ruoli | Specializzazioni leggere, non cinque serrature che impediscono di giocare a chi non ha il ruolo giusto. |
| Pista | Rimane invariata nella geometria percorribile, nelle collisioni e nella navigazione AI. |
| Storytelling | Briefing con immagini AI, voce, musica ed effetti; leggero movimento in compositing. Cinematiche 3D rinviate. |
| Fari | Progressione narrativa visibile e leggera; non una scusa per introdurre infrastrutture fisiche o un terzo minigioco obbligatorio. |
| Sabotaggio interno | Variante party esplicita e limitata, integrabile con effetti esistenti; niente traditore obbligatorio o libertà di bloccare il gruppo. |
| Cambio pilota | Conservato a ogni giro, con il percorso di sicurezza esistente. |
| Tecnologia input | Consumer C++ dello Spatial Input comune di TholusOS. Nessun driver/pairing Lighthouse duplicato. |

**Numeri e soglie:** i conteggi 5 / apertura+1 / apertura+2 persone derivano dall’indicazione dell’utente. Le altre durate e grandezze numeriche di seguito sono ipotesi di bilanciamento da provare, non prestazioni del sistema né valori già approvati dopo un playtest.

## 1. Evidenze lette e limiti dello studio

### 1.1 Baseline

Repository `darioriccio/supertuxkart-immersive`, branch `main` ricontrollato e risolto a **`96d38a4654a3a7e41556b26c564197bff3595932`**. Lo studio precedente e il piano Spatial Input v2 sono stati riletti. I riferimenti [R1–R7] alla fine del documento puntano a sorgenti o documenti del fork, non a una versione upstream ipotizzata.

Questa revisione non è un audit dell’intero STK, del suo bilanciamento o del runtime installato. Prima delle patch l’agente deve rileggere il checkout effettivo e preservare lavoro successivo.

### 1.2 Riuso verificabile

- `CrewActionHandler` ha già agganci a energia, boost, shield e rallentamenti. Il suo attacco attuale può però scegliere un rivale di ripiego: non riusare quel fallback nel puntamento geometrico. [R1]
- `ExplosionAnimation` contiene il salto/rotazione da colpo, controllo di invulnerabilità e shield. Legge la durata dalle proprietà del kart; il suo ciclo ha effetti su attachment, fisica e camere. Non è una funzione innocua da chiamare con un nuovo numero a caso. [R2]
- `Attachment` mantiene un tipo attivo e uno stato proprio: non aggiornare la bubblegum ogni frame per simulare il nuovo shield, rischiando di sovrascrivere altro. [R3]
- `RelayRaceMode::enforceHandoffFreeze()` immobilizza tutti i kart durante il cambio, non solo l’umano; `update()` ha già un ramo dedicato che non esegue il normale aggiornamento di gara durante la pausa/handoff. Riutilizzarlo, estendendo la sospensione alle nuove meccaniche. [R4]
- Il percorso di proiezione documentato cattura sei direzioni ma pubblica come output operativo il **fisheye/domemaster front-facing 180°**. Il supporto equirettangolare non va dichiarato già disponibile per STK. [R5]
- L’API attuale riceve azioni semantiche della crew; non dimostra da sola mira continua o corrispondenza fra colpo e frame proiettato. [R6]
- Il direttore crew attuale ha finestre e ruoli in attesa: una modalità con puntatori non può limitarsi a sostituire i vecchi bottoni con immagini. [R7]

### 1.3 Cosa insegna BattleVirus, senza copiarlo

BattleKart BattleVirus è cooperativo: si attacca un virus e si possono curare i compagni infettati. Il produttore descrive anche bonus per attacchi coordinati ravvicinati. Qui ci interessano **attacco coordinato e soccorso**, non il virus, l’immobilizzazione ripetuta o l’intero formato di gioco. [W1]

---

## 2. Identità della modalità

### 2.1 Una gara prima di tutto

Il driver sceglie traiettoria, frenata, accelerazione e opportunità di sorpasso. La crew non guida al suo posto, non teletrasporta avversari e non cambia segretamente l’ordine d’arrivo.

La vittoria completa richiede:

```text
raceFinished && officialTeamRacePosition == 1 && cargoCompleteAtFinish
```

Per la prima missione si propone `cargoCompleteAtFinish = 3 moduli validi nei 3 alloggiamenti`.

I fari sono una conseguenza narrativa e una traccia del viaggio; nel primo MVP non costituiscono un’ulteriore condizione nascosta di vittoria. Il punteggio crew e i riconoscimenti migliorano il risultato secondario, non lo sostituiscono.

| Esito reale | Presentazione |
|---|---|
| Primi e carico completo | Vittoria completa. |
| Primi, carico incompleto | Gara vinta, consegna incompleta. Nessun falso trionfo di missione. |
| Non primi, carico completo | Consegna salva, gara persa. |
| Non primi, carico incompleto | Tentativo fallito con contributi reali mostrati. |

“Devono vincere” è l’obiettivo dei giocatori, non un mandato al software per garantire il primo posto. Il livello iniziale deve essere affrontabile, ma perdere deve rimanere possibile e comprensibile.

### 2.2 Scintilla per un pubblico più grande

Scintilla diventa una voce radio sveglia e ironica, non un personaggio infantile che commenta ogni gesto. È l’assistente del convoglio: segnala un lock, conferma una cattura, prende in giro con leggerezza un turbo mal sincronizzato. Non assegna colpe personali per errori o guasti.

Premessa proposta: la rete dei fari è fuori uso e il nucleo del kart deve arrivare al punto di consegna prima dei rivali. Il circuito fornisce moduli di stabilizzazione; i fari confermano la riattivazione durante la corsa. I rivali cercano di guadagnare terreno, non di uccidere o distruggere definitivamente il veicolo.

Mantenere la geografia e il look leggibile della pista scelta. Un’illustrazione introduttiva può mostrare il villaggio in blackout fuori dalla pista, senza promettere che il livello di gara venga trasformato in una nuova ambientazione notturna.

### 2.3 L’interdipendenza cercata

```text
Il pilota porta il bersaglio in una posizione utile.
A tiene aperto lo shield.
B/C colpiscono.
Il rivale sobbalza e perde velocità.
Il pilota sfrutta la finestra per passare.
Un altro membro difende o recupera il carico durante l’azione.
```

Il merito va alla sequenza, non esclusivamente all’ultimo grilletto.

## 3. Comandi del puntatore

Per adolescenti conviene separare due intenti, senza aggiungere menu:

| Controllo logico | Azione |
|---|---|
| `primary` | Spara; tenuto può produrre fuoco a cadenza limitata. |
| `tool` mantenuto | Fascio contestuale: apri scudo avversario, recupera componente, rinforza un settore del nostro scudo. |
| Rilascio `tool` | Interrompe l’azione continua. |

L’associazione fisica del comando a grilletto/grip/pulsante compatibile avviene tramite il contratto Spatial Input e i profili della sala. Non hardcodare un bottone VIVE in STK. Il profilo concreto del VIVE 2018 va verificato sull’hardware.

Il mirino deve mostrare **prima** dell’azione che cosa farà `tool`, con icone diverse per breccia, calamita e difesa. La priorità di target fra HUD e mondo è dichiarata: una zona interattiva di shield non deve catturare invisibilmente la mira destinata a un nemico.

Un dispositivo può mantenere un solo fascio. Mentre mantiene lo shield nemico non può contare anche come tiratore della medesima combo. Niente bimanualità obbligatoria, lanci del controller o gesti ampi richiesti.

Gli input press/hold/release sono distinti. Una riacquisizione del tracking o una riconnessione non deve essere interpretata come un nuovo press.

## 4. Attacco coordinato: regola definitiva del design

### 4.1 Tre profili, conteggi inequivocabili

`P = numero di partecipanti dichiarati, compreso il pilota`.  
`C = P − 1 = numero di membri attivi della crew con puntatore`.

| P | C | Attacco valido allo stesso rivale |
|---:|---:|---|
| 2 | 1 | **Cinque colpi** dello stesso membro, entro una sequenza limitata. Lo scudo mostra cinque segmenti che cedono; al quinto il rivale viene colpito. Nessuna apertura mantenuta contemporaneamente richiesta. |
| 3 | 2 | A apre e mantiene lo shield; B deve mettere a segno almeno **un colpo** nella finestra aperta. |
| 4–6 iniziali | 3–5 | A apre e mantiene lo shield; almeno **due membri distinti B e C**, diversi da A, devono colpire nella stessa finestra di coordinazione. |

Con 5 controller non chiedere automaticamente quattro tiratori. Il minimo offensivo rimane tre mani: le altre possono difendere, raccogliere e preparare il motore.

Il conteggio è di persone/slot autorizzati nella sessione, non di pacchetti, click o seriali USB. Due pressioni della stessa persona non sostituiscono due tiratori. Un nuovo indice del runtime non crea un nuovo partecipante.

La soglia viene stabilita alla preparazione e cambia solo con un’esplicita riconfigurazione prevista del roster. Non abbassarla automaticamente quando un controller viene coperto: sarebbe un exploit e confonderebbe guasto e scelta di difficoltà.

### 4.2 Come si apre lo scudo

Il membro A indica il kart avversario e tiene `tool`. Un anello/progresso aderente allo shield comunica l’aggancio. Dopo una breve acquisizione compare la breccia: non è necessario modellare un meccanismo o una porticina per ogni kart.

L’aggancio dipende da:

- target realmente visibile nel canale in cui si mira;
- distanza/area apparente ammesse;
- posa fresca e valida;
- mantenimento del target da parte di A;
- assenza di altre animazioni o stati incompatibili del rivale.

Una tolleranza angolare esplicita può rendere stabile il fascio rispetto al tremore, ma non lo rende capace di seguire da solo un’auto uscita dall’inquadratura. Se si vuole un aggancio assistito, il suo volume deve essere visibile.

La camera continua a seguire il kart umano. Le curve, la distanza e i cambi di assetto rendono il mantenimento una vera abilità, senza introdurre una telecamera indipendente del cannoniere.

### 4.3 Finestra e colpi distinti

Valori di partenza da provare, non requisiti hardware:

| Parametro | Valore iniziale proposto |
|---|---:|
| Tempo per acquisire la breccia | 0,6 s |
| Durata massima di un’apertura mantenuta | 3,5 s |
| Finestra fra il primo e il secondo tiratore (C ≥ 3) | 1,25 s |
| Cadenza massima di fuoco | 4 impulsi/s |
| Distanza temporale massima fra colpi della catena solo | 2,5 s |
| Durata totale massima della catena solo | 8 s |
| Animazione del sobbalzo | Obiettivo 1 s, da misurare con l’effettiva ripartenza |
| Protezione dalla stessa classe di attacco dopo il recupero | 4 s iniziali |

B e C non devono premere nello stesso millisecondo. Il kart avversario mostra due piccoli segni distinti mentre riceve i colpi; l’ultimo completa la combo.

I colpi avvengono durante la breccia. Premere sullo shield chiuso dà un feedback di rimbalzo, ma non accumula in segreto contributi per la prossima apertura.

Se A rilascia, perde realmente il target o il tracking, la finestra si chiude. Un cambio di apritore apre un’altra transazione: niente accumulo ambiguo di contributi da attacchi diversi. I normali errori di mira non tolgono punti già conquistati dalla squadra.

Nel profilo solo, i colpi mancati non azzerano immediatamente tutta la sequenza. Il progresso decade alla scadenza dichiarata o alla chiusura dell’opportunità; cambiare bersaglio non trasferisce i segmenti a un’altra auto.

### 4.4 Stati di attacco per rivale

```text
shielded → acquiring(opener) → breached(window, opener, contributors)
         → stagger → recovery → shielded
```

Il percorso solo passa da `shielded` a una catena personale di cinque segmenti e poi a `stagger`; non finge l’esistenza di un apritore.

Chiave logica di una combo: sessione + race generation + istanza del kart bersaglio + attackWindowId. Per ogni contributo conservare participantId e sourceEventId. I retry dell’input non producono nuove tacche.

Non applicare due sobbalzi per due eventi arrivati nello stesso tick. La transizione a `stagger` chiude la finestra e pubblica una sola conseguenza autoritativa.

### 4.5 Il colpo non elimina

Il rivale sobbalza e perde velocità; poi rientra nella gara. Non viene distrutto, espulso, respawnato arbitrariamente davanti o cancellato dal risultato.

L’errore da evitare è il blocco a catena: cinque persone che concentrano il fuoco non devono tenere un avversario fermo fino alla fine. Il tempo di recupero è leggibile con un effetto diverso, e gli input non possono prolungare un’animazione già iniziata.

Il tempo dell’animazione e la perdita effettiva di tempo in gara non coincidono: ripartire da fermo aggiunge un costo. Misurare il sorpasso risultante prima di confermare “un secondo” come effetto bilanciato.

## 5. Difesa attiva: gli altri attaccano davvero noi

### 5.1 Una minaccia con una sorgente reale

Per la prima versione basta un solo archetipo:

```text
un rivale realmente vicino e ammesso dal profilo
→ indicatore di aggancio, direzione e segnale sonoro
→ tentativo di breccia sul nostro shield
→ impulso/proiettile visibile
→ blocco/intercetto oppure colpo sul kart e sul carico
```

Non creare colpi invisibili attribuiti a un’auto che non esiste, è fuori portata o dietro un muro. Per gli attacchi al di fuori del campo proiettato è necessario un canale di allarme/difesa esplicito, descritto nella sezione 9.

Le AI restano pilotate dal comportamento STK esistente. Un piccolo direttore Relay può scegliere quando una AI reale ha opportunità di attacco: non occorre implementare cinque personaggi AI di equipaggio su ogni avversario. La reciprocità riguarda shield, preavviso, vulnerabilità e conseguenze; non pretende che ogni AI simuli cinque esseri umani.

### 5.2 Come difende la crew

Quando il lock nemico si avvicina, appare il **settore dello scudo minacciato** (fronte/sinistra/destra/retro), rappresentato con un arco leggibile o un nodo proiettato.

Un membro indica quel settore e mantiene `tool`: rinforza lo shield e contrasta l’apertura. Se resta sul settore nel momento giusto, l’attacco viene respinto. Se lo abbandona troppo presto, il lock può completarsi e il colpo entra.

Un altro membro può intercettare l’impulso dopo il lancio, se il profilo e la visibilità lo consentono. Un tiro mancato non diventa automaticamente un blocco del settore.

**Difesa non significa tenere un bottone premuto per tutta la gara.** La minaccia ha un inizio e una fine. Dopo averla respinta, il difensore ha di nuovo ragioni per attaccare o recuperare oggetti.

### 5.3 Perché lo scudo non diventa invulnerabilità permanente

- Il comando rinforza un settore, non ogni direzione contemporaneamente.
- Una barra di energia di servizio alimenta shield e boost; il carico di missione non viene speso.
- Il consumo/riscaldamento del modulo è condiviso: alternare controller non azzera il limite.
- Il consumo principale avviene mentre si contrasta un attacco, più un costo di mantenimento, non attraverso una manutenzione continua che prosciuga tutto anche senza minacce.
- Un periodo di protezione post-colpo impedisce la raffica che toglie il controllo al pilota.

Per C=1 proporre minacce serializzate e ben anticipate: la stessa mano non deve aprire un nemico e difendere simultaneamente come requisito minimo per non perdere. Per C=2 o C=3 la scelta di abbandonare un’offensiva per difendere può essere interessante; un attacco volontario può costare un’occasione, non obbligare a una sconfitta inevitabile.

### 5.4 Camera e colpo subito

La conseguenza meccanica deve essere coerente e chiara anche per il kart umano. La camera conserva l’orizzonte e non copia il rollio spettacolare dell’animazione come se fosse la testa dello spettatore. Rimane la camera del kart, non passa a una cinepresa esterna per l’esplosione.

Qualunque reset globale di camera del percorso `ExplosionAnimation` va controllato: la stabilizzazione immersiva non deve perdersi quando viene colpito un avversario o il kart umano. [R2]

## 6. Carico, raccolta e fari senza rifare la pista

### 6.1 Il carico: tre alloggiamenti, niente inventario complesso

Il nucleo è un piccolo elemento sul kart; la prima missione prevede tre alloggiamenti per moduli intercambiabili. La squadra deve arrivare prima con tutti e tre pieni.

Per ridurre le variabili non aggiungere contemporaneamente salute del veicolo, percentuale integrità, sigilli, peso, tre valute e un inventario. Per il primo profilo bastano:

- tre icone/stati del carico;
- una riserva di energia di servizio;
- le informazioni locali degli scudi.

I moduli di missione non sono la stessa risorsa della nitro o dello shield. Boost e difesa non possono consumare l’ultimo oggetto indispensabile al traguardo.

### 6.2 Recupero in corsa

Posizionare poche opportunità curate tramite dati del tracciato: capsule che riutilizzano un modello esistente o una sagoma 2D/2.5D nitida con un guscio visivo.

- Il primo membro può rompere il guscio con `primary`.
- Chiunque può agganciare il modulo con `tool` e mantenerlo per una breve durata.
- Con C=1 le due azioni si fanno in sequenza; non richiedere due identità anche per ogni raccolta.
- Con più membri, più capsule/opportunità si distribuiscono nello stesso tratto, entro il budget visivo.

La buona guida prolunga l’opportunità o la rende più facile. Non obbligare il pilota a frenare a ogni cassa; gli ancoraggi e le finestre devono essere provati alla velocità effettiva del tratto.

Sono oggetti Relay con identità propria, non gli stessi pickup STK a cui si aggiunge un secondo premio. Anche quando si riusa la mesh di un pickup, la transazione è unica e il collider/ownership devono essere inequivocabili.

### 6.3 Proteggere significa non perdere il lavoro fatto

Un attacco nemico completato può sganciare **al massimo un modulo** dal carico, una sola volta per impatto. Prima di perderlo definitivamente, il modulo rimane per una breve finestra visibile sul cavo magnetico del kart: qualcuno può recuperarlo.

Proposta iniziale: 2,5–3 s di recupero, con posizione e icona chiare e senza cambiare la camera. Niente frammenti microscopici da cercare sul bordo del telo.

Se fallisce, quell’alloggiamento torna vuoto. Una successiva opportunità di raccolta può riempirlo: niente obbligo di fare retromarcia fino al punto in cui è caduto. Le opportunità sostitutive devono apparire in un ancoraggio successivo ammissibile e registrato, non essere regalate al carico di nascosto.

Un carico completo non rende inutile la difesa: ora va portato fino alla fine. Un carico vuoto non aumenta l’effetto di stordimento e non può perdere oggetti negativi. A fine gara resta lo stato reale, anche se un ultimo errore ha fatto perdere la vittoria di missione.

### 6.4 Fari: versione leggera consigliata

I fari sono tre **stazioni di conferma della rete**, non nuove costruzioni percorribili. Si possono rappresentare con una sagoma, un segnale o un elemento di scenario esistente a cui aggiungere emissione e un fascio luminoso economico.

Per l’MVP consigliato:

1. Il primo recupero/conferma di ciascuno dei tre alloggiamenti abilita una tappa della rete.
2. Quando la squadra attraversa il successivo punto di conferma previsto dallo script, il nucleo invia automaticamente il segnale e il relativo faro si accende.
3. La sua accensione resta registrata per la sessione e visibile ai passaggi successivi.
4. Il segnale non consuma il modulo. Se il modulo viene poi perso, la rete già attivata non si spegne: resta però incompleta la consegna finale finché l’alloggiamento non viene riempito.

L’automatismo è **una conseguenza del componente guadagnato**, non una vittoria automatica. Evita di imporre un ulteriore compito obbligatorio mentre attacco e difesa occupano tutte le mani.

In un incremento successivo, una trasmissione volontaria con `tool` per circa un secondo può dare energia o punti bonus, ma non deve chiudere un percorso o rendere impossibile la missione se viene mancata. Il profilo v2 base usa la conferma automatica; non lasciare entrambe le versioni attive senza distinzione.

### 6.5 Tracciato immutato: cosa è ammesso

Ammessi: oggetti non collidenti, sprite/billboard, glow, anelli sugli avversari, indicatori, nodi del carico, piccole particelle, audio, script di opportunità legati al giro/progresso, un elemento leggero sul kart.

Esclusi dalla prima versione: modifiche a mesh/collisioni della pista, navmesh/grafo di guida, scorciatoie nuove, cancelli fisici, ponti, rampe mosse, sequenze con droni autonomi complessi, distruttibilità del mondo, AI di guida nuova.

La pista può avere una mappa di ancoraggi Relay separata dal file grafico originale. Le coordinate e l’occlusione vanno provate, ma non richiedono la ricostruzione del tracciato.

## 7. Specializzazioni leggere e utilità continua

### 7.1 Cinque competenze, non cinque obblighi permanenti

Tutti i membri hanno colpo e fascio. Con C=5 le identità consigliate sono:

| Competenza | Momento caratteristico | Cosa fa quando quel momento manca |
|---|---|---|
| **Hacker** | Acquisisce/apre lo shield e chiama il bersaglio. | Raccoglie, difende, contribuisce a una combo aperta da altri. |
| **Cannoniere** | Completa i colpi e intercetta proiettili. | Recupera componenti o assume la difesa. |
| **Difensore** | Individua minacce laterali/posteriori e protegge il carico. | Attacca e raccoglie quando il pericolo è passato. |
| **Ingegnere** | Gestisce occasioni di energia/boost e recupero del carico. | Fa anche il secondo tiratore o apre una breccia. |
| **Ricognitore-recuperatore** | Individua capsule, segnala rivali, sceglie priorità di raccolta. | Spara e può mantenere uno shield. Non è un navigatore che aspetta un bivio. |

Il ricognitore non richiede nuove piste o informazioni segrete su un telefono. La specializzazione iniziale può essere una responsabilità comunicata e un piccolo riconoscimento, senza un sistema di abilità complesso.

Piccoli bonus eventuali (tempi di acquisizione o consumi) si introducono solo dopo il test della versione universale. Non modificano il requisito dei cinque colpi o delle identità distinte. Un solo cannoniere non può diventare “due persone” grazie a un upgrade.

Con C=1–2 non mostrare cinque professioni da gestire in parallelo: il kit è universale e la collaborazione è richiesta soltanto quando è possibile. Le identità di ruolo possono ruotare al cambio pilota, non continuamente durante la mira.

### 7.2 Il direttore deve creare decisioni, non turni di stand-by

Il nuovo profilo non deve aspettare che il singolo incontro principale finisca prima di offrire attività al resto della crew. Distinguere:

- opportunità di base: recupero e scelta dell’avversario;
- pressione: minacce vere e finestre di sorpasso;
- momenti di squadra: combo, consegna, recupero urgente;
- brevi intervalli di respiro in cui si prepara la mossa seguente.

Non riempire ogni secondo di obblighi. La misura utile è “c’è una scelta utile disponibile?”, non “sta premendo il grilletto?”.

Le simultaneità dipendono dal numero di mani. Con C=3 l’attacco completo occupa tutta la crew: una minaccia sopraggiunta deve permettere di interrompere l’offensiva per difendere. Non dichiarare contemporaneamente obbligatori attacco a tre, difesa e recupero di due oggetti.

Se il team è in testa e non ci sono rivali davanti, dare importanza a difesa posteriore, recupero e gestione delle risorse. Non generare falsi avversari davanti al kart per tenere occupati i cannoniere.

### 7.3 Contrastare il monopolio senza penalizzare chi è bravo

Identificare i contributi utili, non soltanto i last hit. Offrire bersagli/opportunità distinte e suggerimenti visivi di priorità. Non rendere tutte le attività esclusive di un colore.

Chi apre la breccia riceve merito anche se non infligge il colpo finale. Chi difende riceve credito soltanto se contrasta una minaccia reale. Chi raccoglie a inventario pieno non deve farmare punti di missione infiniti.

L’unicità richiesta per la combo limita il monopolio offensivo in modo naturale: una persona molto brava ha comunque bisogno degli altri.

## 8. Sabotaggio del pilota: conservarlo, ma come variante leggibile

Il desiderio di scherzi e discussioni è coerente con il target. Non serve però una modalità “traditore” che annulli la missione.

### 8.1 Primo scherzo consigliato: turbo forzato della crew

Nella variante **Party**, nota a tutti prima della partita, la crew può attivare un **turbo non richiesto dal pilota** usando un nodo dedicato del veicolo/HUD. È un vero effetto sul mezzo, non solo un clacson: può essere una mossa utile o una scelta mal sincronizzata che genera la reazione del guidatore.

Limiti iniziali proposti:

- una sola attivazione di squadra per turno del pilota;
- consumo della riserva di servizio, mai del carico;
- attivazione deliberata, distinta da un normale colpo o fascio;
- con almeno due supporter, conferma di due membri; con uno vale il singolo comando del profilo Party;
- impulso breve e limitato, con parametri separati dal boost principale;
- pilota in grado di contrastarlo con freno/comando previsto;
- indisponibile durante pausa, handoff, recovery, intro, finale e zone del tracciato marcate come inadatte dal profilo;
- niente override dello sterzo, visuale ruotata, schermo oscurato o blocchi prolungati.

Usare l’effetto boost esistente e gli ancoraggi/profilo del tracciato, non nuove geometrie. Il limite è sul turno di squadra, non per dispositivo: cinque controller non danno cinque scherzi consecutivi.

Il dato Party deve comparire nel rapporto e nel bucket del risultato: non confrontarlo indistintamente con una gara cooperativa fissa. Default del prototipo tecnico: off, così il test della mira non viene confuso da questo effetto. Può essere on nel profilo Party autorizzato.

### 8.2 La frizione sociale migliore nasce anche dalle scelte normali

Il pilota chiede di colpire un rivale; la crew preferisce salvare il carico. L’ingegnere conserva energia invece di dare boost. Due persone inseguono la stessa apertura e lasciano scoperto il retro. Sono discussioni prodotte dalla missione, senza introdurre comportamenti che impediscono agli altri di giocare.

Non trasformare uno scherzo in una penalità individuale pubblica. Tutta la squadra ne vive l’esito.

## 9. Dietro il kart e dietro lo spettatore: il limite da risolvere esplicitamente

### 9.1 Non sono la stessa cosa

Una cupola circonda fisicamente gli utenti, ma il profilo STK esaminato proietta un contenuto front-facing 180°. Le sei facce sono catturate; non significa che tutta la sfera virtuale sia già visibile nel mapping pubblicato. [R5]

Non piazzare un bersaglio dietro la camera STK e dichiarare che i ragazzi possono colpirlo voltandosi: va prima dimostrato che quel bersaglio è veramente proiettato lì.

### 9.2 Scelta economica per il primo incremento: retrovisore di missione

Mantenere la camera principale invariata. Ricampionare la direzione posteriore dalle facce già catturate per mostrare un **retrovisore/radar di missione**, presentato come superficie secondaria del veicolo, in una zona leggibile dell’output. Quando la calibrazione della cupola lo consente, collocarlo in un settore fisico laterale/posteriore dedicato alla crew; altrimenti usare un settore effettivamente visibile e dichiarare quel profilo.

Non è una seconda camera che insegue il bersaglio: è una vista posteriore dello stesso kart, con orientamento relativo fisso. Non si crea un’altra simulazione e non serve per principio un secondo rendering completo a sei facce. Il costo di ricampionamento/compositing va comunque misurato.

La prima versione può usare il retrovisore solo per leggere il nemico e il suo lock e un nodo di **difesa posteriore** per contrastarlo. Se si vuole sparare anche dentro il retrovisore, il suo mapping e il picking diventano parte esplicita del contratto: non basta un’immagine incollata sopra il video.

Un semplice allarme di direzione permette di verificare la difesa prima di implementare il retrovisore, ma **non** chiude il requisito di combattimento posteriore visibile. Segnare separatamente `rear_alert` e `rear_targetable_view` nelle capacità/prove.

### 9.3 Regole del picking nella vista secondaria

Il raggio della mano colpisce un punto del telo; il mapping dell’output stabilisce se quel punto appartiene al mondo principale, a un comando di shield o al retrovisore. Nel retrovisore si applicano rettangolo/forma, crop e orientamento della vista secondaria, inclusa un’eventuale riflessione dichiarata.

Ogni vista dispone di `viewId`, revisione del layout, relazione col frame e trasformazione verso la scena STK. Non usare la matrice della vista frontale su un pixel del retrovisore. Occorre applicare occlusione e visibilità anche lì: non si colpisce attraverso una montagna perché un’icona ha segnalato il nemico.

Un futuro output equirettangolare completo può eliminare la necessità del retrovisore come compromesso, ma è una qualifica di proiezione distinta. Non deve essere un prerequisito nascosto per il primo test con due controller.

## 10. Esempio di 40 secondi con sei partecipanti

P = 6, C = 5. Il gruppo sta portando due moduli su tre e ha un po’ di energia.

**0–8 s:** il pilota entra in un tratto veloce. Hacker e cannoniere scelgono un rivale vicino; il ricognitore segnala una capsula. Il difensore vede un avversario in avvicinamento nel retrovisore. L’ingegnere ha una scelta fra contribuire al secondo colpo e recuperare energia.

**8–15 s:** l’hacker mantiene la breccia. Cannoniere e ingegnere colpiscono entro la finestra: il rivale sobbalza. Il pilota passa. Il ricognitore apre/aggancia il terzo modulo. Il difensore contrasta il lock posteriore e impedisce di perdere quello già caricato.

**15–23 s:** il kart entra in curva; non si impone un’altra combo a tre. Il recupero viene completato, il nucleo ha tre alloggiamenti pieni. Il pilota chiede energia per il rettilineo successivo; chi ha appena aperto lo shield può ora occuparsi della riserva.

**23–32 s:** la squadra attraversa il punto di conferma della rete. Il faro si accende perché il modulo è stato guadagnato. Scintilla conferma in una frase. I rivali sono adesso alle spalle: cambia la priorità, non bisogna continuare a cercare bersagli inesistenti davanti.

**32–40 s:** un attacco non viene contrastato del tutto. Il kart subisce il piccolo stop e un modulo resta sul cavo magnetico. Un membro lo recupera mentre gli altri proteggono; il pilota riprende la linea. Il carico torna completo, ma il distacco guadagnato si è ridotto.

Questa sequenza è illustrativa: non deve ripetersi identica a orari fissi e non dimostra che cinque compiti simultanei siano già bilanciati. Serve a verificare che ogni azione abbia una conseguenza leggibile nella stessa gara.

## 11. Regia artistica e media: immagini ora, 3D dopo

### 11.1 Direzione visiva

Conservare kart e tracciato STK; aggiungere un linguaggio da competizione tecnologica colorata, ironica e leggibile. Scudi con pochi segmenti netti, brecce evidenti, fasci sottili, impulsi brevi, nucleo riconoscibile. Evitare piccoli testi, contatori sovrapposti alle curve, confetti a pieno campo o cinque laser opachi che coprono il pilota.

Colore + forma/numero identificano sempre i membri. Una breccia non deve essere riconoscibile solo dalla differenza rosso/verde. Tutti i reticoli restano coerenti con l’identità di sala/sessione; la loro opacità può calare quando non c’è un’azione utile in corso, senza nasconderli durante la mira.

Nessun nuovo motore WebGL2/Chroma dentro STK. Il gioco usa il proprio renderer C++; il riferimento Chroma riguarda l’input comune, non una libreria grafica da innestare nel kart.

### 11.2 Intro proposta: circa 40 secondi

Un video montato da immagini AI, audio e lievi movimenti; le durate sono una prima scansione editoriale.

| Shot | Durata | Immagine/azione | Funzione |
|---|---:|---|---|
| 1 | 6 s | Villaggio/rete di fari spenti, ambiente coerente con il tema. | Mostrare il problema. |
| 2 | 6 s | Nucleo e tre alloggiamenti del carico. | Rendere concreto ciò che va consegnato. |
| 3 | 7 s | Kart umano in gara, rivali protetti dagli scudi. | Chiarire che resta una competizione. |
| 4 | 7 s | Illustrazione con fascio che apre lo shield e contributi della crew. | Anticipare il gesto di squadra. |
| 5 | 7 s | Attacco posteriore, guardia e modulo recuperato. | Mostrare reciprocità e difesa. |
| 6 | 7 s | Linea di partenza e obiettivo sintetico. | Chiudere: primi, con il carico. |

40 s sono una proposta, non una durata obbligatoria. Montare a ritmo, non allungare gli shot per arrivare al numero.

Confezionare immagini 2D adatte al formato dell’uscita del briefing, voce, musica e SFX. Separare il media di presentazione dal runtime di gara. Il launcher/TholusOS orchestra il passaggio con i controlli già esistenti, senza pretendere che STK includa un editor video.

I movimenti possono essere pan/zoom moderati, parallax di pochi livelli, luci e particelle. Non spacciare questi pannelli per un nuovo ambiente 3D navigabile. In cupola usare una composizione leggibile con contesto visivo nel resto della superficie, senza stirare scritte o personaggi attraverso tutta la sfera.

### 11.3 Testo di voce proposto

> «La rete dei fari è fuori uso. Noi abbiamo il nucleo, ma mancano tre moduli per stabilizzarlo. Gli altri equipaggi proveranno a fermarci. Pilota, tienici in gara. Crew: aprite gli scudi, coordinate i colpi e proteggete il carico. Il primo posto da solo non basta: dobbiamo arrivarci con tutto a bordo. Io sono Scintilla. E no, non posso guidare al posto vostro.»

Il testo va adattato e misurato sull’audio reale. Per il tutorial successivo usare una frase specifica per C=1, C=2 o C≥3; non mostrare il requisito di tre mani quando ci sono soltanto due partecipanti totali.

### 11.4 Runtime audio

Una voce per eventi importanti, con priorità: pericolo immediato → perdita del carico → cambio pilota → missione → commento. Evitare sovrapposizioni e tagliare la battuta secondaria se arriva una minaccia.

Una raccolta iniziale di cue può coprire: aggancio in corso, breccia aperta, manca un secondo tiratore, combo completata, attacco dal retro, shield rinforzato, modulo agganciato, modulo sganciato, recupero riuscito, faro confermato, preparazione cambio, ripartenza, carico incompleto al traguardo, vittoria completa.

Non chiamare LLM o TTS in tempo reale per ogni evento. Media prodotti e verificati prima della sessione, localizzati e disponibili offline.

### 11.5 Sostituire le immagini con cinematiche 3D in futuro

Il contratto del briefing/finale deve riferirsi a cue narrativi e condizioni del risultato, non al tipo di produzione. Uno shot può essere sostituito da video 3D mantenendo identità, durata prevista e punto di transizione. Non ricodificare il gioco perché è cambiata un’immagine introduttiva.

## 12. Cambio pilota: conservato e narrativamente integrato

La staffetta avviene a ogni giro come nella modalità attuale. Le politiche di giri non si modificano in questa proposta. La tabella storica indica 6 giri per 2–3 partecipanti, 8 per 4, 10 per 5 e 12 per 6; va riletta nel checkout corrente prima della patch. [R7, R8]

Al cambio:

1. Si annuncia in anticipo il prossimo pilota.
2. Il percorso sicuro esistente congela la gara; non solo il kart umano. [R4]
3. Si chiudono lock/combo/tiro e si sospendono i timer di minacce, recuperi e missione.
4. Si cambia il binding partecipante→ruolo/controller nella sessione, senza rifare pairing o calibrazione della sala.
5. Chi lascia il volante riceve il dispositivo del prossimo pilota o quello previsto dal roster; si verifica l’identità con un gesto semplice.
6. Si richiede rilascio dei comandi prima del riarmo e conferma nel percorso esistente.
7. Si riparte con lo stesso carico, fari e stato persistente della missione; niente attacchi arretrati dalla rete.

Non inserire un puzzle obbligatorio nel pit-stop né premiare la fretta con cui i partecipanti si scambiano posto. Si può mostrare il quadro della missione o una battuta, ma le pause rimangono un’esigenza di rotazione.

Per le nuove classifiche, usare il tempo di gara effettivo, non il tempo reale impiegato dal gruppo a passarsi un controller. Se si misurano le durate commerciali dell’esperienza, registrarle separatamente.

## 13. Rigiocabilità e difficoltà

### 13.1 Non basta randomizzare il colore

Usare un catalogo piccolo di incontri ben fatti: recupero, breccia/sorpasso, attacco alle spalle, intercetto e carico sganciato. Variarne ordine, sovrapposizione, posizione fra gli ancoraggi ammessi e priorità. L’esito di un’azione influenza ciò che serve dopo.

Scegliere un seed di sessione riproducibile. Le condizioni non devono produrre composizioni impossibili o usare per forza tutta la libreria in ogni giro.

Il primo giro insegna le azioni, la parte centrale combina ruoli, la parte finale mette alla prova il carico e il distacco. La gara non si interrompe per un boss separato obbligatorio.

### 13.2 La difficoltà della crew e quella della guida non sono la stessa variabile

Misurare acquisizioni perse, minacce respinte, carico e opportunità non utilizzate separatamente dal distacco e dagli errori del pilota. Non punire con bersagli impossibili una crew competente solo perché il pilota è bravo.

Il direttore di difficoltà attuale offre già segnali di gara, ma la selezione di opportunità per i controller è un incremento da fare. Non introdurre una seconda AI generativa né una serie di boost nascosti che garantiscano il primo posto. [R9]

Profili proposti: `crew_standard` e `crew_party`, più parametri di difficoltà dichiarati. Una classifica competitiva usa condizioni comparabili; l’eventuale assistenza dinamica è tracciata.

### 13.3 Il criterio di rigiocabilità è osservabile

Dopo più partite chiedere se vogliono rifare il ruolo di equipaggio e provare una strategia diversa. Non basta che sparino molto.

Obiettivi qualitativi: il pilota ha bisogno della crew, la crew ha bisogno della traiettoria, il difensore produce momenti memorabili, il passaggio al volante non è l’unica ricompensa e il gioco rimane leggibile a tutti.

Non promettere “non si stancheranno mai”. Registrare i momenti di noia, monopolio, sovraccarico e confusione e correggere il design.

---

## 14. Architettura tecnica minima

### 14.1 Un consumer dello Spatial Input, non una nuova infrastruttura

```text
Controller / tracking
  → Spatial Input comune di TholusOS
  → adapter host STK e client C++ del contratto comune
  → input Relay: press, held action, release, cancel, timestamp e raggio
  → targeting nel frame pertinente
  → regole di missione/attacco/difesa nello stesso thread autoritativo STK
  → effetti di gara esistenti o wrapper Relay circoscritti
  → renderer STK e output attuale
```

Stanza → Accessori continua a possedere discovery, identità, pairing, calibrazione e diagnostica. STK non deve accedere a USB/SteamVR. I telefoni non sono prerequisiti della modalità nuova; le vecchie API possono restare per compatibilità, ma non registrare i controller come telefoni finti.

Il contratto comune deve fornire dati fisici e lifecycle. STK possiede bersagli, regole di tiro, risorse, miss, punti e missione. Nessun `targetId` o “colpo riuscito” proveniente dal controller è considerato autorevole.

### 14.2 Raggio fisico e camera in movimento

Per ciascun evento:

1. Usare origine e direzione della mano nel frame della sala, con calibrazioni e tempi validi.
2. Intersecare la superficie fisica ammessa.
3. Risalire al pixel/canale dell’output proiettato, usando il profilo fisheye effettivo e l’eventuale regione di retrovisore.
4. Ricavare il raggio della camera STK corrispondente a quel pixel nel frame di gioco pertinente.
5. Risolvere bersaglio e occlusione sullo stato storico pertinente.
6. Applicare soltanto ora l’effetto allo stato autoritativo corrente, se istanza, finestra e regole restano valide.

Il kart virtuale e la cupola reale non usano necessariamente la stessa scala. Non lanciare il raggio della mano direttamente sui kart virtuali. Non applicare due volte roll/pitch della camera o la rotazione del dome.

Il punto dello schema che costa più lavoro non è il bottone “spara”: è la corrispondenza **mano → pixel proiettato → camera e target in movimento**.

### 14.3 Prova del frame presentato

L’attuale `state_version_seen` delle azioni phone non è da solo una prova del frame realmente proiettato. Syphon consegna immagini, non certifica da solo il momento in cui il pubblico le vede. [R6]

Riutilizzare il contratto di presentazione della suite dove compatibile, ma qualificare il producer STK: frame identity, camera/capture transforms, stream generation e associazione all’uscita. Non assumere che l’integrazione di Sagitta qualifichi automaticamente questo renderer.

I nuovi oggetti hanno identità d’istanza e trasformazioni storiche limitate; la pista statica consente di riutilizzare i controlli di occlusione senza riavvolgere tutta la simulazione fisica. Il metodo preciso di picking va scelto sul renderer reale: volumi/primitive di bersaglio visibili e coerenti con i proxy sono una prima strada; un pass ID completo a ogni frame per ogni giocatore non è obbligatorio né gratuito.

Se l’incertezza include più frame plausibili, definire una policy conservativa coerente con la suite. Non scegliere automaticamente il frame che produce il colpo desiderato e non ripiegare sulla posa corrente dopo il grilletto.

Anche un fascio continuo deve possedere campioni validi durante il mantenimento. L’ultimo press non dà il diritto di mantenere lo shield aperto per minuti senza pose o release.

### 14.4 Moduli sufficienti

Evitare una nuova collezione di microservizi. Dentro `src/modes/immersive_relay/` sono sufficienti responsabilità chiaramente separate:

| Responsabilità nuova | Contenuto |
|---|---|
| Input/targeting spaziale | Client del contratto, mapping della vista e target storici, cancel e held actions. |
| Regole Relay Crew | Shield per kart, finestre/cooperazione, risorse, minacce e carico. Può essere un runtime con sottocomponenti, non cinque processi. |
| Presentazione Crew | Anelli, fasci, capsule, nodi, informazioni del carico e retrovisore. |
| Script missione | Ancoraggi, opportunità, milestone, media e preset; dati versionati e validati. |

`CrewEventDirector`, `CrewRoleManager`, `CrewActionHandler` e `RelaySessionController` sono punti di partenza da estendere. I nomi delle nuove classi sono da scegliere nel checkout: questo documento non inventa API esistenti.

### 14.5 Shield nuovo e powerup vanilla: non sovrapporre due sistemi incompatibili

L’attuale bubblegum è un attachment con durata e interazioni native; non equivale allo scudo cooperativo mantenuto di questa proposta. [R1–R3]

**Scelta raccomandata per il primo profilo di missione:** scudi e impulsi Relay hanno un piccolo stato dedicato, condiviso nella regola fra kart umano e avversari. Riutilizzare risorse grafiche pertinenti dello shield, non la sua attivazione ripetuta ogni frame.

Per evitare che due sistemi di danno producano risultati incomprensibili, il primo profilo controllato deve usare un pool di armi/powerup compatibile. Gli attacchi offensivi che non attraversano la policy Relay vengono esclusi **solo da questo profilo**, o mantenuti soltanto dopo un hook di compatibilità verificato. Documentare la lista effettiva nel checkpoint, non modificare il gioco vanilla globale.

Questo è un compromesso esplicito: si limita inizialmente la combinatoria degli oggetti nativi, non si riscrive la guida o l’AI. La scelta non significa disabilitare le collisioni, i salti o il rischio di uscire di pista. La compatibilità con altri powerup potrà essere aggiunta uno alla volta.

Una nuova minaccia può usare un modello di proiettile esistente e una traiettoria semplice. Non deve obbligatoriamente ereditare ogni effetto o ricerca del bersaglio di un vecchio powerup. Il termine “aperto” dello shield ha una sola autorità; gli effetti grafici non decidono il danno.

### 14.6 Riutilizzare il sobbalzo senza fare danni al motore

`ExplosionAnimation` è una base concreta da valutare. Il codice letto:

- rifiuta/gestisce invulnerabilità e shield;
- sceglie la durata dalle proprietà;
- modifica fisica e attachment;
- ha un percorso di camera durante il cleanup. [R2]

L’agente deve isolare una policy Relay di durata/intensità/recupero: eventuale overload o wrapper deve conservare i default vanilla. Non modificare globalmente `getExplosionDuration()` per tutti i giochi; non usare un flag `direct_hit=false` come trucco per dimezzare l’effetto.

Prima di applicare lo stagger verificare che il kart non sia già in un’animazione incompatibile o eliminato. Non assegnare invulnerabilità prima della chiamata nativa se quella chiamata la interpreta come motivo per rifiutare il colpo. Il nuovo carico non deve stare nello slot `Attachment` che l’esplosione ripulisce.

Il fallback `slowRival()` corrente produce uno schiacciamento/rallentamento, non esattamente il salto richiesto. È utile per un test meccanico dichiarato, ma non è la consegna grafica finale di questo design. [R1]

### 14.7 Thread, tempi e transazioni

I messaggi entrano in una coda limitata. Mutazioni di kart, inventario, shield e score avvengono nell’aggiornamento autoritativo; il thread HTTP non modifica direttamente corpi fisici o scene node.

Separare clock monotono di trasporto dal tempo di simulazione. I timer di opening, combo, proiettili, carico sganciato e rigenerazione non avanzano mentre la gara è in pausa/handoff. Le scadenze di connessione continuano a funzionare come servizio.

Per il fuoco automatico mantenuto, ogni impulso generato dal runtime deve avere un’identità unica e una posa/tempo coerenti con l’istante del tiro. Non considerare tutti gli impulsi successivi come lo stesso campione congelato del primo press.

Un contributo storico può identificare un target visto in passato; non può far rinascere un target già scomparso, riaprire una finestra conclusa, ripristinare vecchio carico o assegnare punti due volte.

### 14.8 Perdita del tracking e dimensione della squadra

Una breve perdita elimina l’attività del solo controller: niente shield mantenuto da una mano invisibile, niente spari dalla vecchia posa. Gli altri continuano secondo la policy comune.

La soglia offensiva non viene silenziosamente abbassata. Se il dispositivo non torna, prevedere la riconfigurazione autorizzata alla pausa/handoff o il profilo di recupero già supportato; non inventare un partecipante automatico che completi i colpi.

Il primo prototipo con due controller valida **P=3**. Il profilo P=2 si prova con un solo supporter. P≥4 va collaudato con almeno tre controller reali prima di dichiarare dimostrata la cooperazione a tre mani.

## 15. Performance e vincoli di produzione

Non associare più giocatori a più rendering completi del mondo. Una simulazione e una cattura, overlay ed eventi dei controller sullo stesso stato.

Contenere il numero contemporaneo di target, particelle trasparenti e fasci; registrare costi CPU/GPU della cattura a sei facce. La risoluzione 1536 domemaster documentata è il preset provvisorio del fork, non un obiettivo da alzare senza benchmark. [R5, R8]

Il primo test di prestazioni deve includere camera in movimento, due controller, ombre/proiezione reali, effetti di shield e un retrovisore attivo se lo si dichiara disponibile. Un test su target statico non sostituisce questo carico.

La disponibilità delle immagini AI riduce la produzione del briefing, non elimina la necessità di realizzare buoni feedback di gioco. Preparare un set piccolo e coerente: shield, breccia, impulso, fascio, modulo, nucleo/alloggiamenti, faro-segnale e icone. Riutilizzare risorse e licenze della build e mantenere un inventario dei nuovi media.

## 16. Sequenza di implementazione, una fase alla volta

Le sigle **SC0–SC6** sono un incremento di Missione Scintilla, non rinominano SI0–SI9 o le fasi pregresse STK. L’agente legge il documento completo ma implementa solo la fase autorizzata.

| Fase | Consegna | Criterio di chiusura |
|---|---|---|
| **SC0 — Baseline e profilo** | Mappa del checkout, capacità di output/input, pool dei powerup e parametri del profilo. | Nessuna ipotesi critica su frame, retro o hardware lasciata come funzione già pronta. |
| **SC1 — Mira in corsa** | Due reticoli via Spatial Input, targeting di kart e oggetti durante curve, frame e cancel. | Si colpisce ciò che era proiettato, senza fallback al primo rivale; verifica software e limiti hardware distinti. |
| **SC2 — Breccia e combo** | I tre profili 5 / A+B / A+B+C, scudo, feedback e stagger circoscritto. | Test delle identità, cadenze, scadenze, immunità, main camera e regressioni vanilla. |
| **SC3 — Ritorsione e difesa** | Un attacco AI telegraphed, shield di settore, energia, alert e vista retro prevista dal profilo. | Non si è invulnerabili tenendo un bottone; nessun attacco invisibile inevitabile; capacità rear dichiarata e provata. |
| **SC4 — Carico e missione** | Tre slot, raccolta, sgancio/recupero, conferme dei fari, esiti e prima intro 2D. | Primi + carico completo verificato al traguardo; niente modifiche alla pista o cinematiche 3D obbligatorie. |
| **SC5 — Ruoli, rotazione, Party** | Specializzazioni leggere, opportunità concorrenti, handoff completo e turbo Party separato. | Nessun ruolo condannato al stand-by; risorse/stati conservati correttamente fra i turni. |
| **SC6 — Playtest e bilanciamento** | Prove con adolescenti, sessioni ripetute, tre profili di partecipanti e rapporto prestazioni. | Consegna delle correzioni misurate e dei limiti; non basta un video di una combo riuscita. |

Prima verifica pratica raccomandata: una sola pista, P=3, due puntatori. Solo quando mira + apertura + sparo risultano piacevoli aggiungere la difesa; solo dopo introdurre il carico completo. Non aprire subito il lavoro su cinque ruoli esclusivi e molte piste.

## 17. Matrice minima di verifica

| ID | Prova | Esito richiesto |
|---|---|---|
| A01 | P=2, quattro colpi validi | Nessuno stagger; quattro segmenti corretti. |
| A02 | P=2, quinto colpo valido | Un solo stagger. |
| A03 | P=2, catena scaduta o altro rivale | Nessun trasferimento di progresso. |
| A04 | P=3, B colpisce shield chiuso | Rimbalzo, non contributo conservato. |
| A05 | P=3, A mantiene e B colpisce | Combo valida una volta. |
| A06 | P≥4, B colpisce due volte | Non vale come B+C. |
| A07 | P≥4, A prova a fare anche il tiratore | Non conta fra i due non-apritori. |
| A08 | P≥4, B e C su rivali diversi | Nessuna combo fusa. |
| A09 | P≥4, B+C fuori finestra o dopo rilascio A | Nessuno stagger. |
| A10 | Retry/riordine/reconnect | Nessun giocatore o colpo duplicato. |
| A11 | Target già in recovery | Nessun prolungamento o farming. |
| A12 | Controller perso durante apertura | Breccia chiusa secondo la policy, non tenuta dalla posa vecchia. |
| G01 | Curva mentre si spara | Target/frame corretti, non solo prova da fermo. |
| G02 | Kart occluso o fuori canale | Non colpibile per fallback semantico. |
| G03 | Pixel del retrovisore | Mapping posteriore corretto e distinto da quello principale. |
| G04 | Carro umano/avversario colpito | La camera immersiva resta nel profilo previsto. |
| D01 | Rivale attacca da dietro | Preavviso e sorgente reali, difesa comprensibile. |
| D02 | Difesa mantenuta senza fine | Risorse/limiti impediscono invulnerabilità permanente. |
| D03 | Due attacchi simultanei sullo stesso kart | Una sola conseguenza nel gate di invulnerabilità/stagger previsto. |
| D04 | Crew da una sola mano | Nessuna simultaneità obbligatoria impossibile. |
| C01 | Guscio e modulo raccolto da due sorgenti | Una sola unità nel carico. |
| C02 | Attacco superato, carico pieno | Al massimo un modulo sganciato per impatto. |
| C03 | Recupero dentro/fuori finestra | Ripristino o perdita reale senza duplicati. |
| C04 | Passare due volte sullo stesso ancoraggio/in retromarcia | Nessun farming delle ricompense. |
| C05 | Tutti gli esiti d’arrivo | Risultati coerenti con posizione e carico congelati al traguardo. |
| C06 | Faro già acceso, modulo perso | Faro persistente; inventario distinto e consegna ancora da verificare. |
| H01 | Pausa/handoff durante breccia o carico sganciato | Timer di gara sospesi e comandi cancellati in modo coerente. |
| H02 | Cambio con tutti gli AI in movimento | Nessun vantaggio AI durante lo scambio fisico. |
| H03 | Controller passa all’ex pilota | Identità di sessione aggiornata, calibrazione fisica invariata. |
| P01 | Profilo Party disabilitato | Nessuna possibilità di turbo forzato. |
| P02 | Party usato, cambio controller | Il limite di squadra non si azzera per il dispositivo. |
| R01 | Build/profilo vanilla | Nessun cambiamento di fisica, camera, powerup o race loop. |
| R02 | Vecchia modalità a telefoni | Compatibilità dichiarata, nessun nuovo falso telefono necessario. |
| R03 | Più partite e reset | Nessuna finestra, icona faro, lock o modulo rimasto dalla partita precedente. |

### Misure del playtest

Registrare tempo di acquisizione, successi delle combo, false sensazioni di miss, latenza percepita, numero di perdite di tracking, occasioni utili per ogni partecipante, tempi di stand-by, energia e moduli persi/ripresi, cambi pilota, esiti della gara e desiderio di rigiocare i ruoli.

Separare errore del dispositivo, mapping/proiezione, regola incomprensibile e abilità della persona. Aumentare le hitbox non deve essere il modo di nascondere un mapping sbagliato.

La fase finale include prove con crew competente/pilota debole e viceversa, una crew interamente offensiva, una interamente difensiva, un membro molto più rapido degli altri e momenti di testa/coda della gara. Nessuna di queste strategie deve spegnere il gioco degli altri o produrre successi automatici.

## 18. Istruzione di consegna all’agente

> Leggi questo design v2, il piano Spatial Input comune e il checkout corrente di SuperTuxKart Immersive Relay. Le priorità sono: gara da vincere con il carico; camera della nostra auto; cinque colpi per il supporter solo; apertura mantenuta più un tiratore con due supporter; apertura più due tiratori distinti con almeno tre supporter; difesa reciproca; cambio pilota invariato.
>
> Non ricostruire la pista, non aggiungere bivi/cancelli/ponti, non aprire un progetto Blender o un nuovo motore grafico. I media iniziali sono immagini AI montate con voce, musica, SFX e leggero movimento. Il gioco resta C++/STK; Spatial Input e Accessori appartengono a TholusOS.
>
> Implementa solo la fase SC autorizzata. Prima di mutare effetti nativi, controlla `ExplosionAnimation`, attachment, camera e pool dei powerup. Non chiamare il primo rivale come fallback di un colpo. Non fingere che la vista virtuale posteriore sia già coperta dal domemaster front-facing. Distingui output, retrovisore, telemetria e prova fisica.
>
> Consegna test ed evidenze della singola fase, parametri modificati e limiti aperti. Un mock non certifica la mira in cupola. Non modificare i requisiti di partecipazione o garantire il primo posto con AI/risorse nascoste per far passare una demo.

## 19. Fonti e riferimenti

I file del progetto sono riferiti al commit letto. Le specifiche di design proposte non sono asserzioni delle fonti.

- **[R1]** `CrewActionHandler`: boost, shield, selezione/rallentamento dei rivali. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/src/modes/immersive_relay/crew_action_handler.cpp
- **[R2]** `ExplosionAnimation`: create, durata, invulnerabilità, attachment, fisica/camere. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/src/karts/explosion_animation.cpp
- **[R3]** `Attachment`, stato e cleanup. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/src/items/attachment.cpp
- **[R4]** `RelayRaceMode`, freeze e update durante cambio pilota. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/src/modes/immersive_relay/relay_race_mode.cpp
- **[R5]** Proiezione e copertura del fork. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/docs/immersive_relay_phase46_projection_generalization.md
- **[R6]** Contratto di integrazione TholusOS/STK. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/docs/immersive_relay_tholustos_integration.md
- **[R7]** Crew director e rotazione/giri. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/docs/immersive_relay_phase3_crew_event_director.md — sorgente: https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/src/modes/immersive_relay/crew_event_director.cpp
- **[R8]** Stabilizzazione arcade, preset provvisorio e policy di cambio. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/docs/immersive_relay_phase5_arcade_stabilization.md
- **[R9]** Direttore della difficoltà. https://github.com/darioriccio/supertuxkart-immersive/blob/96d38a4654a3a7e41556b26c564197bff3595932/stk-code/docs/immersive_relay_adaptive_difficulty.md
- **[W1]** BattleKart, BattleVirus, descrizione del produttore. https://www.battlekart.com/en/battlevirus/
- **[L1]** Studio precedente: `Tholus_STK_Missione_Scintilla_Studio_Design_v1.md`, allegato in questa conversazione.
- **[L2]** Piano comune: `TholusOS_Spatial_Input_Piano_Implementazione_v2.md`, allegato in questa conversazione. Rimane il riferimento per hardware, calibrazioni, lease, Accessori e protocolli condivisi.
- **[U1]** Indicazioni dell’utente in questa revisione: target adolescenti, conteggi offensivi, camera, no pista, difesa, media 2D, missione, vittoria con carico e mantenimento del cambio pilota.

**Criterio finale:** la squadra vuole vincere la gara e sa che il vantaggio viene da aperture, colpi, difese e recuperi coordinati. La crew non è un passatempo laterale; il pilota non è un passeggero del suo supporto. Il nuovo divertimento deriva da regole e feedback mirati sopra la base STK, non dalla ricostruzione del gioco.
