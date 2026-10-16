# SC3 — Tracciabilità dei casi del piano

I casi nativi sotto elencati sono **scritti e registrati, non eseguiti**. I controlli Python verificano matematica o struttura dei sorgenti; non sostituiscono le prove runtime della colonna finale. I nomi brevi dei casi C++ appartengono ai registri in `tools/immersive/tests/`; quelli Swift alle suite del checkpoint host.

| Piano | Regressione o evidenza sorgente | Qualifica successiva |
|---|---|---|
| S01 | `warningPresentationBoundary`, `completeDefense`; verifica high≤now e warningTicks | Ritardo ottico reale3s/2,5s |
| S02 | `defenseProtocolClosedAndEpochChecked`, `timeoutAndVideoGap`, consumer storico SC1 | Cambi stream/layout/route durante GPU lenta |
| S03 | Filtri `defenseGeometry`/`opportunity`, verifica istanze prima del commit | AI ritirata, rescue/stagger e ostacolo in una vera gara |
| S04 | `sectorsAndLayout`; oracle angolare | Orientamento kart/camera su pista inclinata |
| S05 | `releaseAtDeadlineWins`, `insufficientHold`, `newEpochNeverInheritsContinuity` | Input e proof GPU ai bordi della deadline |
| S06 | `invalidProofLatches`, `defenseToolExcludesOffensiveBeamAndLatchesContextLoss`, `admissionOrderAndStaleSerial` | Tracking intermittente con due dispositivi |
| S07 | `ownerDoesNotMultiply`, `releaseBeforeOtherAdmission`, `admissionOrderAndStaleSerial` | Due utenti/contesa su nodi reali |
| S08 | `standbyPreservesReserveAndLatches`, `boostCostCooldownAndRearm`; precedenti sessioni lunghe SC1 | Hold prolungati e latenza reale |
| S09 | `exactBoostReserveBoundary`; scanner fixed point/clamp e `profileBudgets` | Verifica confini con runtime nativo e trace risorse |
| S10 | `fixedPointSegmentedStandby`, `profileBudgets`; oracle60/120/144Hz | Esecuzione nativa ai timestep ammessi e gap fisico |
| S11 | `sectorsAndLayout`; oracle forward/inverse, layout CPU→uniformi | Retro nei due output, resize e mapping venue |
| S12 | `defenseHUDNeverFallsThroughToWorld`, `defenseRejectsAmbiguousHistoricalLayout` | Bordo HUD/retro nella proiezione effettiva |
| S13 | Scanner depth preesistente, regioni commissioning e LOS producer | Occludenti reali davanti alla sorgente rear |
| S14 | `ballistics`; oracle radici double | Esecuzione dei casi C++ con FP della piattaforma |
| S15 | Bullet `translatedTarget`, `movingTarget`, `actualCompoundOffset`, `rotatingOffset` | Cast Bullet nativi e kart reali in movimento |
| S16 | Bullet `obstacleWinsAndOrderIndependent`, `separatedObstacleIsNotAnUncertainContact` | Static mesh/dinamici/primo contatto nella scena |
| S17 | `impactIdempotency`, `timeoutAndVideoGap`; cleanup trace su cambio fase | Callback/contact duplicati e frame contact lento |
| S18 | `preserveAcrossReprepare`, `boostCostCooldownAndRearm`; sospensione core/adapter | Pause/resume in ogni fase con receiver |
| S19 | Scanner di tutti i gate server; `ImmersiveSuperTuxKartIntegrationTests` | Ready da UI, operatore e SDL senza preparazione |
| S20 | Adattatore impatto/boost e accessor fade letti; oracle tangenziale | Kart suolo/aria/rescue/zipper, camera e velocità effettive |
| S21 | Gate condizionali, layout SC2 precedente conservato; regressioni precedenti intatte | Vanilla, sola SC2 e telefoni legacy nel binario |
| S22 | `preserveAcrossReprepare`, `defenseParticipantRebindIsAtomicAndIdempotent`, `defenseParticipantRebindRejectsPhysicalChanges` | Fine/reset con contact pendente e cambi crew/set fisico |
| S23 | Cache GL condivisa limitata, lease e timeout; regressioni readback SC1 conservate | GPU lenta, storico insufficiente e budget reale |
| S24 | `explicitVisibilityLossCancelsPendingLaunch`, verifica negativo nelle fasi pendenti | Sorgente rear che esce/si occlude durante lock e volo |

Fixture aggiuntive: `twoAttackersRespectCooldownAndCrewRearm` usa due AI identificate in minacce consecutive; `defenseTwoSourcesMovingFramesPreserveIndependentContexts` usa due sorgenti, camera/target mobili e contesti difesa/offesa distinti. I test Bullet usano corpi traslati e compound ruotati con offset. L'integrazione reale fra queste componenti, il driver GL e il receiver rimane da eseguire; nessuna fixture sintetica viene descritta come prova fisica.
