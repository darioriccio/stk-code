# Missione Scintilla — introduzione italiana 2D

Tre clip locali definitive, una per C=1, una per C=2 e una per C=3–5. Ogni clip contiene sei pannelli originali, testi italiani impressi nell'immagine e transizioni morbide sullo stesso fondo chiaro. La comprensione non richiede audio. Non contiene musica, voce sintetica, filmati stock, mesh o personaggi STK importati. Il kart, la capsula, i fari e Scintilla sono illustrazioni procedurali originali; la loro rappresentazione non modifica le hitbox del gioco.

Formato: **1920×1080, H.264 yuv420p, 30/1 fps, 1200 frame, 40 secondi, senza traccia audio**. Tagli ai frame 0/180/360/570/780/990/1200; 180/180/210/210/210/210 frame. I testi restano leggibili anche senza attivare i sottotitoli WebVTT consegnati accanto alle clip. Non esiste un timer host di 40 secondi che simuli la fine: la receipt `played` richiede il completamento osservato dal trasporto video esistente.

La variante C=1 illustra cinque colpi sul medesimo rivale, secondo il profilo solo SC2. C=2 mostra un fascio e il colpo di un altro supporter. C=3–5 mostra un fascio e i colpi di due altri supporter. Il pannello della capsula precisa in tutte le varianti che la stessa persona può aprire e raccogliere. Sono esempi di coordinamento, non nuovi ruoli esclusivi.

L'host seleziona la variante da C della gara, verifica hash, dimensioni e tempi, poi usa la composizione frontale del renderer esistente. Il rettangolo non va stirato su tutta la sfera. La proiezione fisica, il comfort e la leggibilità sul telo richiedono la verifica venue separata.

`manifest.json` è il contratto chiuso `tholus.stk-scintilla-intro/1`. Il suo hash si calcola sui byte effettivi, senza normalizzazione JSON. I riferimenti a video, sottotitoli e pannelli sono relativi a questa directory; ogni file ha SHA-256. Il manifest non contiene il proprio hash.

Rigenerazione offline, dalla radice del repository:

```sh
python3 -B tools/immersive/scintilla/create_mission_intro.py --work-dir '/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc4-intro-authoring'
```

Lo script richiede Pillow, ffmpeg e font installati selezionabili con `--font-bold` e `--font-regular`. La prima edizione usa Arial/Arial Bold installati su macOS; nessun file font è distribuito. ffmpeg e Pillow sono strumenti di authoring, non nuove dipendenze del player o del gioco. Tutti gli output configurabili devono restare sul SSD montato.

## Provenienza e licenza

Titolo: **Missione Scintilla — introduzione italiana 2D v1**. Autori: **Immersive Relay contributors (Codex-assisted)**. Illustrazioni, composizione, testi originali, PNG, clip, WebVTT e generatore sono rilasciati con **[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)**. Conservare titolo, credito, collegamento alla licenza e indicazione delle modifiche quando si redistribuisce materiale modificato. I software di authoring e il font installato mantengono le proprie licenze; non sono inclusi negli asset distribuiti.
