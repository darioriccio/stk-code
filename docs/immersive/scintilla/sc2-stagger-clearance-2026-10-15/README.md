# SC2 — Volume del sobbalzo, 15 ottobre 2026

Correzione del punto 2 dell'audit SC2: il controllo precedente centrava il
volume sul pivot usando le semidimensioni nominali e non comprendeva lo
spostamento del telaio fisico nel corpo composto. La correzione di Pronto
rimane invariata; gli altri rilievi non sono oggetto di questo intervento.
SC3 non avviata. Nessuna build, configure, compilazione, typecheck, avvio o
esecuzione dei test nativi.

## Geometria effettiva e inviluppo

La factory legge `kart->getBody()->getCollisionShape()->getAabb(identity,...)`.
Nel Bullet incluso nel repository, il corpo composto include trasformazioni
dei figli e relativi margini; il suo AABB comprende anche il centro locale
traslato. Il telaio STK è un hull figlio con `getGravityCenterShift()`, non una
scatola simmetrica intorno all'origine. Non ricavare nuovamente questi limiti
da altezza/lunghezza/larghezza nominali.

L'helper puro `ScintillaCombat::StaggerEnvelope` riceve l'AABB locale `l,u`.
Per il pitch intorno a X usa il raggio rispetto all'origine effettiva:

```text
Y = max(abs(l.y), abs(u.y))
Z = max(abs(l.z), abs(u.z))
R = hypot(Y,Z)
k = 0.5
a = min(8 gradi, k*H/R), H = 0.25 m
```

Per ogni angolo `theta` in `[0,a]`, `cos(theta)` appartiene a `[cos(a),1]`
e `sin(theta)` a `[0,sin(a)]`. Il codice racchiude per intervalli le coordinate
`y*cos(theta)-z*sin(theta)` e `z*cos(theta)+y*sin(theta)`. L'intervallo completo
diventa una scatola con centro locale proprio, con 2 cm di riserva esterna.
La posa della query è `bodyTransform * translation(envelopeCenter)`; il centro
del volume e il pivot dell'animazione rimangono distinti. La scatola, orientata
come il corpo iniziale, viene controllata con overlap iniziale e sweep lungo
la normale per H. Racchiude anche il tragitto di ritorno della stessa curva.

La conversione a `btScalar` aggiunge alle semiestensioni l'errore di conversione
del centro e arrotonda verso l'esterno con `nextafter`. Il pitch effettivo viene
arrotondato verso zero, così non supera quello usato per l'inviluppo.
Bounds non finiti/invertiti/degeneri, conversioni non rappresentabili, spazio
occupato o non dimostrabile conservano il fallback già previsto: arresto e
feedback, altezza e pitch nulli, contatore `suppressedStaggerMotion` esistente.

## Appoggio e pitch

Non si ignora più qualsiasi contatto con normale rivolta verso l'alto. Sono
ammessi soltanto contatti finiti sotto/al fondo reale dell'AABB iniziale, con
normale unitaria `n` tale che `dot(n,up)>k`. Un ripiano sopra quel fondo, un
soffitto, una parete o un contatto non valido non passano questa eccezione.
La classificazione è per contatto, senza ignorare in blocco l'oggetto pista.

Per un punto del corpo, lo spostamento dovuto alla rotazione soddisfa
`|dRotation| <= R*theta`. Durante la curva, `theta=a*w` e la risalita è `H*w`.
Quindi lungo una normale di supporto ammessa:

```text
dot(n,dPosition) >= dot(n,up)*H*w - R*a*w
                >= (dot(n,up)-k)*H*w >= 0
```

Questo giustifica il limite più prudente sul pitch: il solo vincolo precedente
`H/R` non bastava per le normali inclinate ammesse dal callback. Durata, curva,
limite massimo di altezza e asse di rotazione del profilo sono conservati.

## Controesempio e verifiche

Fixture ideale: limiti locali `[-0.5,0,-1]` e `[0.5,1,1]`, coerenti con un
telaio 1 m × 2 m traslato di 0.5 m verticalmente.

- Vecchio volume più risalita: sommità `0.909173101 m`, sotto il soffitto a 1.15 m.
- Raggio effettivo `sqrt(2)`; pitch limitato a circa `5.064279278 gradi`.
- Nuovo volume iniziale: sommità `1.108273304 m`; fine sweep `1.358273304 m`.
  Il soffitto a 1.15 m ricade nel tragitto controllato.
- Sommità del corpo al culmine, con questo pitch: circa `1.334369596 m`.
  Il volume verificato è conservativo rispetto al corpo. Una copertura a 1.5 m
  non incontra questo inviluppo nella fixture.

Eseguiti senza compilatori:

```sh
python3 -B tools/immersive/tests/check_scintilla_stagger_clearance.py
python3 -B tools/immersive/tests/check_scintilla_combat.py
git diff --check
```

Passano 6 controlli nuovi e gli 8 controlli sorgente/matematici SC2 aggiornati.
Il nuovo script valuta le espressioni scalari del C++ e le confronta con una
rotazione di riferimento a quaternioni: bounds asimmetrici, traslazioni/pose
del mondo, tutti gli angoli campionati, supporti e conversioni float. È prova
matematica e di collegamento sorgente; non esegue C++, Bullet o il gioco.

Quattro regressioni C++ sono scritte e registrate nel target esistente
`scintilla_combat_regression` (21 casi totali): soffitto, inviluppo su bounds
traslati, distacco dai supporti e input geometrici invalidi. Non compilate né
eseguite. Il ramo vanilla dopo il blocco Relay è identico byte per byte alla
base. Restano da verificare nativamente callback Bullet, factory completa,
fisica, resa del modello e comportamento in gara, entro un incarico che lo
consenta. La query riguarda la scena fisica al momento dell'ammissione.
