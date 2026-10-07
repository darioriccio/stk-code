# Evoluzione successiva: piste realmente interattive

Questa proposta non è implementata dalla MVP. I comandi attuali producono bonus,
impulsi, difesa e rallentamenti arcade; non aprono geometrie o scorciatoie.
Gli identificatori legacy rimangono compatibili, mentre i testi mostrati ai
partecipanti descrivono gli effetti realmente disponibili.

Per introdurre cancelli, rampe mobili e scorciatoie serviranno:

- Asset e geometrie di pista dedicati, collisioni sincronizzate e percorso AI
  aggiornabile; nessun varco soltanto visivo o rampa senza collisione coerente.
- Stato autorevole nel producer, modificato sul ciclo di gioco e replicato
  nell'API; reset di ogni oggetto a nuova gara e comportamento definito durante
  pausa, handoff, perdita dei telefoni e replay.
- Segnali in cupola leggibili prima del punto di decisione: percorso base sempre
  percorribile e recupero sicuro se la crew non completa la combinazione.
- Cooperazione completabile con uno–cinque telefoni crew: più ruoli sullo stesso
  telefono, finestre compatibili con l'input reale e nessun obbligo sul pilota.
- Collaudo delle collisioni, AI, camera immersiva e latenza; prove con persone
  e proiezione sulle piste modificate prima di offrirle nel catalogo consumer.

Una prima estensione proporzionata può usare un solo cancello opzionale su una
pista dedicata. Il percorso ordinario rimane la via di recupero. Solo dopo le
prove fisiche si estenderanno rampe e scorciatoie ad altre piste.
