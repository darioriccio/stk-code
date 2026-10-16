# SC3 — Boost, priorità AI e costo visibile

15 ottobre 2026. Correzioni dei tre finding residui dell'audit, affrontati
nell'ordine 1→2→3 rispetto al [piano SC3](../sc3-plan-2026-10-15/PIANO_SC3.md).
Consegna solo sorgenti; nessuna build/configure/typecheck, esecuzione nativa,
preview, avvio app/producer/helper o hardware. SC4 non avviata.

## Correzioni e ricontrollo

1. **Boost applicato prima della conferma.** L'adattatore gira dopo Bullet:
   `instantSpeedIncrease` da solo impostava un minimo destinato a sparire al
   successivo `Kart::update`. Ora il veicolo nativo applica subito quel minimo
   tramite `btKart::adjustSpeed(minimum, -1)`, prima di restituire successo.
   Il valore minimo mantiene il clamp nativo di MaxSpeed; il massimo disabilitato
   evita il ramo che scalerebbe l'intero vettore. Il ramo minimo conserva la
   componente lungo la normale e la velocità angolare. Le verifiche di idoneità,
   forward motion, assenza di boost incompatibili e riserva precedono l'effetto;
   vettori non finiti o moto tangenziale nullo sono rifiutati prima del comando.
   Energia e cooldown sono confermati soltanto dopo questo commit sincrono.
   Ricontrollati chiamanti, ordine Kart→Bullet→SC3 e implementazione nativa.

2. **AI selezionata per priorità con prove complete.** Prima vengono raccolti i
   candidati geometricamente ammessi e con linea corrente libera. Il selettore
   puro condiviso con i test li ordina per ultimo attacco, distanza e id. La prova
   viene richiesta in quest'ordine: Pending ferma la decisione; soltanto un esito
   negativo permette di passare al successivo. Nessuna attesa bloccante GPU.
   Si conserva una sola query, coalescendola verso il nuovo candidato prioritario
   se cambia l'opportunità corrente. Le verifiche live vengono rifatte a ogni tick;
   prove, lease e timeout restano limitati come SC1/SC3.
   Tre regressioni C++ registrate nel runner esistente coprono ordine permutato,
   candidato vicino pendente, visibilità negativa, storico dell'attaccante,
   spareggio per id, nuova istanza e input non valido. Non compilate/eseguite.

3. **Costo leggibile prima della pressione.** `Profile::BoostCost` è l'unica
   definizione del costo 25: ammissione, addebito, risposta API additiva e uniforme
   GLSL leggono la stessa costante. Il nodo mostra il pittogramma fulmine e “−25”
   persistente; la metà superiore della barra evidenzia la quota che verrebbe
   consumata, con tratteggio distinto dalla riserva. Le cifre restano bianche anche
   a boost indisponibile. Nessuna nuova dipendenza, font o hitbox; il pittogramma
   numerico usa segmenti procedurali nello shader esistente. Verificato l'ingombro
   completo, smoothing incluso, dentro il raggio .65 del nodo, separato dall'anello
   di acquisizione. Entrambe le proiezioni leggono il medesimo shader e alpha THB1
   resta l'ultimo passaggio. `layoutRevision` resta 1: geometria e contesti invariati.

Le quattro correzioni del [checkpoint precedente](../sc3-audit-fixes-2026-10-15/README.md)
restano nel WIP e nei controlli: handoff, socket, SC2 durante allarme e reset/rebind.

## Verifiche e limiti

Eseguiti 47 controlli Python: depth 6, volume SC2 6, combat 10, feedback 6,
difesa 19; tutti superati. Il calcolatore del piano supera 10 gruppi matematici.
`git diff --check` pulito. I tre controlli Python aggiunti verificano percorso
nativo/ordine del boost con oracle numerico, decisione AI e collegamento/ingombro
del costo. Il guard precedente contro `adjustSpeed` resta sull'impatto; il boost
usa intenzionalmente il metodo del veicolo nativo, distinto da `Kart::adjustSpeed`.

Il primo controllo del costo aveva rilevato che il bordo di smoothing delle cifre
oltrepassava l'anello di acquisizione; ridotte le dimensioni interne e ripetuta
la verifica con esito positivo, senza ampliare il nodo o indebolire il limite.

La suite C++ difesa contiene ora 24 casi registrati; consumer 80 e Bullet 10
rimangono presenti. Nessun caso nativo è stato compilato/eseguito. Le nuove prove
di selezione eseguono il selettore di produzione con callback di visibilità
controllata: non sostituiscono una prova GPU. I controlli del boost leggono il
codice nativo e verificano la matematica: non sono un'esecuzione di btKart/Bullet.
La leggibilità delle cifre, soprattutto su output piccoli e in cupola, richiede
la futura qualifica ottica; i bounds matematici non ne certificano la percezione.

[verification.json](verification.json) registra le verifiche consentite e i
limiti. [source-manifest.json](source-manifest.json) attribuisce i file modificati
alla baseline WIP iniziale. Consegna solo del delta; nessun commit/push richiesto.
