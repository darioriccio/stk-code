# Immersive Relay MVP — sorgenti del 7 ottobre 2026

La consegna comprende la staffetta cooperativa da 1–6 partecipanti, un pilota alla volta, telefoni facoltativi, selezione di piste installate, controlli SDL/tastiera e integrazione TholusOS. L'uscita Syphon può essere domemaster o equirettangolare nativa 2:1; TholusOS adatta geometria, vista frontale UV e HUD alle superfici calibrate.

Questi documenti conservano la cronologia e le verifiche di ciascuna fase. L'audit iniziale e le build storiche non qualificano il sorgente finale. I riferimenti a log, binari e percorsi locali descrivono evidenze conservate sul Mac; quei materiali non sono inclusi in questa pubblicazione.

- [Audit iniziale](AUDIT.md)
- [Prima implementazione](MVP_IMPLEMENTAZIONE.md)
- [Implementazione completa del piano MVP](MVP_COMPLETA_IMPLEMENTAZIONE_2026-10-07.md)
- [Implementazione del piano superfici](SURFACES_IMPLEMENTAZIONE_2026-10-07.md)
- [Ricontrollo e correzioni del codice](SURFACES_RICONTROLLO_2026-10-07.md)
- [Manifest dei sorgenti pubblicati](PUBLICATION_SOURCE_MANIFEST.json)

Stato della revisione finale: controlli di sorgente e matematica della proiezione completati; test Swift e probe live predisposti, non eseguiti. Build, compilazione degli shader, ricezione Syphon reale, prestazioni, proiezione fisica e partita con controller/telefoni reali restano da verificare. Il volante/pedaliera non è stato scelto. I rilievi preesistenti sulle localizzazioni Mac restano aperti.

Il ricontrollo ha corretto casi enum Syphon inesistenti, rimappatura UV impropria, conferma GPU mancante, recupero dopo preparazione fallita, operazioni asincrone obsolete e valori camera fuori dall'intervallo float. Per pubblicare è stata inoltre aggiornata l'aspettativa del test del catalogo TholusOS che escludeva l'equirettangolare.

La pubblicazione su main è autorizzata; i commit saltano la CI per rispettare il divieto di build. Nessuna build o app è stata avviata in questa fase. Le modifiche indipendenti preesistenti sono conservate fuori dai commit.
