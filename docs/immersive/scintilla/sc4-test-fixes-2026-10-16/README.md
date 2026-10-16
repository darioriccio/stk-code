# SC4 — prima esecuzione dei test e correzioni, 16 ottobre 2026

**Esito:** test nativi STK 251/251, build Relay completa riuscita, package
`TholusScintillaMission` 10/10, test app TholusOS profilo `scintilla` 205/211
prima dell'ultima correzione. Nessuna prova di gioco, GPU, ottica o venue.

Il lavoro SC0–SC4 non era versionato. È conservato senza modificare i checkout
originali, nei quali una sessione SC5 stava ancora consegnando:

| Repository | Snapshot invariato | Correzioni |
|---|---|---|
| STK | `scintilla/sc0-sc4-wip` (`4d8a8f221`) | `scintilla/sc4-test-fixes` |
| TholusOS | `scintilla/sc0-sc4-wip` (`65a411290`) | `scintilla/sc4-test-fixes` |

## Risultati

| Verifica | Esito |
|---|---|
| Suite standalone `tools/immersive/tests` (C++, Bullet, Python offline) | 246/251 sullo snapshot; **251/251** dopo le correzioni |
| Build Relay completa (`STKRelease`, Syphon, API locale) | riuscita, solo avvisi |
| File toccati da SC4 con flag vanilla | solo controllo di compilazione (`-fsyntax-only`), nessuna build vanilla completa |
| `swift test` di `TholusScintillaMission` | il target di test non compilava; **10/10** dopo la correzione |
| App TholusOS, runner isolato, profilo `scintilla` | il target di test non compilava; poi **205/211** |

Dei 6 fallimenti dell'app, 4 (`invalidBinding`) sono corretti nell'ultimo
commit ma **non rieseguiti**, per non ricompilare TholusOS su una macchina
carica. Gli altri 2 (`ImmersiveSTKIntroMediaTests`) richiedono che il runner
riceva un `TMPDIR` sul SSD esterno: il lancio di questa verifica non lo forniva.

## Correzioni

- **Prodotto, `scintilla_input.cpp`:** le mire si risolvono all'arrivo, come le
  pressioni, con lo stesso gate `prepared && active` del tick. Prima una mira
  successiva sostituiva un miss ancora in coda e il fascio tenuto sopravviveva,
  contro il caso M06 del [piano SC4](../sc4-plan-2026-10-16/PIANO_SC4.md).
- **Test STK:** il caso di manutenzione usava fronti acquisiti prima che lo
  storico tornasse pronto, rifiutati dalla regola già presente in SC3; il caso
  di misura costruiva un JSON con radice stringa e terminava con abort.
- **Test Swift:** chiamate `mutating` dentro `#expect`, `#require` annidati e un
  ticket di manutenzione costruito dalla fixture non preparata.

## Riproduzione e limiti

Worktree, build ed evidenze:
`/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/test-fixes-2026-10-16/`
(`build/tests`, `build/relay`, `evidence/host-scintilla-01` e `-02`, `logs/`).
La build Relay è riutilizzabile in modo incrementale.

Restano aperti: rieseguire i 6 test app con `TMPDIR` sul SSD, build vanilla
completa, avvio del gioco, query GPU, qualifica degli ancoraggi, prove con
puntatori reali e accettazione venue.
