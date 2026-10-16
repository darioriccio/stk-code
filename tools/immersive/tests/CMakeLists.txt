cmake_minimum_required(VERSION 3.12...4.0)

# This directory can be configured alone without STK assets, graphics or network
# dependencies. Root builds reach it only through the existing explicit opt-in.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ScintillaInputRegressions LANGUAGES CXX)
    option(ENABLE_RELAY_TEST_TOOLS "Build Immersive Relay core regression tooling." OFF)
endif()
if(NOT ENABLE_RELAY_TEST_TOOLS)
    return()
endif()

enable_testing()
get_filename_component(STK_REGRESSION_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_executable(scintilla_input_regression
    scintilla_input_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/spatial/scintilla_input.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_crew.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_combat.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_mission.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_defense.cpp")
target_include_directories(scintilla_input_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_input_regression PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/scintilla_input_cases.inc")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/scintilla_input_cases.inc" SCINTILLA_CASE_LINES
    REGEX "^SCINTILLA_CASE\\([A-Za-z0-9_]+\\)$")
if(NOT SCINTILLA_CASE_LINES)
    message(FATAL_ERROR "Scintilla regression case registry is empty.")
endif()
foreach(SCINTILLA_CASE_LINE IN LISTS SCINTILLA_CASE_LINES)
    string(REGEX REPLACE "^SCINTILLA_CASE\\(([A-Za-z0-9_]+)\\)$" "\\1"
        SCINTILLA_CASE_NAME "${SCINTILLA_CASE_LINE}")
    add_test(NAME "scintilla.${SCINTILLA_CASE_NAME}"
        COMMAND scintilla_input_regression --case "${SCINTILLA_CASE_NAME}")
    set_tests_properties("scintilla.${SCINTILLA_CASE_NAME}" PROPERTIES LABELS "relay;scintilla;core" TIMEOUT 120)
endforeach()

# The production transport is POSIX-only; Windows retains its existing API stub.
if(UNIX)
    add_executable(relay_http_transport_regression relay_http_transport_regression.cpp
        "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/remote/relay_http_transport.cpp")
    target_include_directories(relay_http_transport_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
    target_compile_definitions(relay_http_transport_regression PRIVATE ENABLE_RELAY_LOCAL_API)
    set_target_properties(relay_http_transport_regression PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/relay_http_transport_cases.inc")
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/relay_http_transport_cases.inc" RELAY_HTTP_CASE_LINES
        REGEX "^HTTP_CASE\\([A-Za-z0-9_]+\\)$")
    foreach(RELAY_HTTP_CASE_LINE IN LISTS RELAY_HTTP_CASE_LINES)
        string(REGEX REPLACE "^HTTP_CASE\\(([A-Za-z0-9_]+)\\)$" "\\1"
            RELAY_HTTP_CASE_NAME "${RELAY_HTTP_CASE_LINE}")
        add_test(NAME "relay_http.${RELAY_HTTP_CASE_NAME}"
            COMMAND relay_http_transport_regression --case "${RELAY_HTTP_CASE_NAME}")
        set_tests_properties("relay_http.${RELAY_HTTP_CASE_NAME}" PROPERTIES LABELS "relay;http;transport" TIMEOUT 30)
    endforeach()
endif()

if(UNIX)
    find_package(Threads REQUIRED)
    add_executable(relay_game_dispatcher_regression relay_game_dispatcher_regression.cpp
        "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/remote/relay_game_dispatcher.cpp")
    target_include_directories(relay_game_dispatcher_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
    target_compile_definitions(relay_game_dispatcher_regression PRIVATE ENABLE_RELAY_LOCAL_API)
    target_link_libraries(relay_game_dispatcher_regression PRIVATE Threads::Threads)
    set_target_properties(relay_game_dispatcher_regression PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/relay_game_dispatcher_cases.inc")
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/relay_game_dispatcher_cases.inc" RELAY_DISPATCHER_CASE_LINES
        REGEX "^DISPATCHER_CASE\\([A-Za-z0-9_]+\\)$")
    foreach(RELAY_DISPATCHER_CASE_LINE IN LISTS RELAY_DISPATCHER_CASE_LINES)
        string(REGEX REPLACE "^DISPATCHER_CASE\\(([A-Za-z0-9_]+)\\)$" "\\1"
            RELAY_DISPATCHER_CASE_NAME "${RELAY_DISPATCHER_CASE_LINE}")
        add_test(NAME "relay_dispatcher.${RELAY_DISPATCHER_CASE_NAME}"
            COMMAND relay_game_dispatcher_regression --case "${RELAY_DISPATCHER_CASE_NAME}")
        set_tests_properties("relay_dispatcher.${RELAY_DISPATCHER_CASE_NAME}" PROPERTIES LABELS "relay;http;dispatcher" TIMEOUT 30)
    endforeach()
endif()

add_executable(scintilla_combat_regression scintilla_combat_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_combat.cpp")
target_include_directories(scintilla_combat_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_combat_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/scintilla_combat_cases.inc")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/scintilla_combat_cases.inc" COMBAT_CASE_LINES REGEX "^COMBAT_CASE")
foreach(COMBAT_CASE_LINE IN LISTS COMBAT_CASE_LINES)
    string(REGEX REPLACE "^COMBAT_CASE\\(([A-Za-z0-9_]+)\\)$" "\\1" COMBAT_CASE_NAME "${COMBAT_CASE_LINE}")
    add_test(NAME "scintilla.combat.${COMBAT_CASE_NAME}" COMMAND scintilla_combat_regression --case "${COMBAT_CASE_NAME}")
    set_tests_properties("scintilla.combat.${COMBAT_CASE_NAME}" PROPERTIES LABELS "relay;scintilla;combat" TIMEOUT 120)
endforeach()

add_executable(scintilla_defense_regression scintilla_defense_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_defense.cpp")
target_include_directories(scintilla_defense_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_defense_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/scintilla_defense_cases.inc")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/scintilla_defense_cases.inc" DEFENSE_CASE_LINES REGEX "^DEFENSE_CASE")
foreach(DEFENSE_CASE_LINE IN LISTS DEFENSE_CASE_LINES)
    string(REGEX REPLACE "^DEFENSE_CASE\\(([A-Za-z0-9_]+)\\)$" "\\1" DEFENSE_CASE_NAME "${DEFENSE_CASE_LINE}")
    add_test(NAME "scintilla.defense.${DEFENSE_CASE_NAME}" COMMAND scintilla_defense_regression --case "${DEFENSE_CASE_NAME}")
    set_tests_properties("scintilla.defense.${DEFENSE_CASE_NAME}" PROPERTIES LABELS "relay;scintilla;defense" TIMEOUT 120)
endforeach()

add_executable(scintilla_mission_regression scintilla_mission_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_mission.cpp")
target_include_directories(scintilla_mission_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_mission_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/scintilla_mission_cases.inc")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/scintilla_mission_cases.inc" MISSION_CASE_LINES
    REGEX "^MISSION_CASE\\([A-Za-z0-9_]+\\)$")
if(NOT MISSION_CASE_LINES)
    message(FATAL_ERROR "Scintilla mission regression case registry is empty.")
endif()
foreach(MISSION_CASE_LINE IN LISTS MISSION_CASE_LINES)
    string(REGEX REPLACE "^MISSION_CASE\\(([A-Za-z0-9_]+)\\)$" "\\1" MISSION_CASE_NAME "${MISSION_CASE_LINE}")
    add_test(NAME "scintilla.mission.${MISSION_CASE_NAME}" COMMAND scintilla_mission_regression --case "${MISSION_CASE_NAME}")
    set_tests_properties("scintilla.mission.${MISSION_CASE_NAME}" PROPERTIES LABELS "relay;scintilla;mission" TIMEOUT 120)
endforeach()

add_executable(scintilla_mission_anchors_regression scintilla_mission_anchors_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_mission_anchors.cpp")
target_include_directories(scintilla_mission_anchors_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_mission_anchors_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/scintilla_mission_anchors_cases.inc")
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/scintilla_mission_anchors_cases.inc" ANCHORS_CASE_LINES REGEX "^ANCHORS_CASE\\([A-Za-z0-9_]+\\)$")
if(NOT ANCHORS_CASE_LINES)
    message(FATAL_ERROR "Scintilla anchor regression case registry is empty.")
endif()
foreach(ANCHORS_CASE_LINE IN LISTS ANCHORS_CASE_LINES)
    string(REGEX REPLACE "^ANCHORS_CASE\\(([A-Za-z0-9_]+)\\)$" "\\1" ANCHORS_CASE_NAME "${ANCHORS_CASE_LINE}")
    add_test(NAME "scintilla.mission.anchors.${ANCHORS_CASE_NAME}" COMMAND scintilla_mission_anchors_regression --case "${ANCHORS_CASE_NAME}")
    set_tests_properties("scintilla.mission.anchors.${ANCHORS_CASE_NAME}" PROPERTIES LABELS "relay;scintilla;mission;anchors" TIMEOUT 120)
endforeach()

# Actual bundled Bullet regression; no STK renderer/assets required.
if(NOT TARGET bulletcollision)
    if(APPLE AND NOT ARCHFLAGS)
        set(ARCHFLAGS "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    add_subdirectory("${STK_REGRESSION_ROOT}/lib/bullet" "${CMAKE_CURRENT_BINARY_DIR}/scintilla-bullet" EXCLUDE_FROM_ALL)
endif()
add_executable(scintilla_defense_collision_regression scintilla_defense_collision_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_defense_collision.cpp")
target_include_directories(scintilla_defense_collision_regression PRIVATE "${STK_REGRESSION_ROOT}/src" "${STK_REGRESSION_ROOT}/lib/bullet/src")
target_link_libraries(scintilla_defense_collision_regression PRIVATE bulletcollision bulletmath)
set_target_properties(scintilla_defense_collision_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(COLLISION_CASE translatedTarget movingTarget actualCompoundOffset rotatingOffset obstacleWinsAndOrderIndependent separatedObstacleIsNotAnUncertainContact socketRelocationRejectsIntermediateObstacle socketRelocationRejectsEndpointOverlap socketRelocationRespectsCollisionExclusions socketRelocationRejectsInvalidSegment)
    add_test(NAME "scintilla.defense.collision.${COLLISION_CASE}" COMMAND scintilla_defense_collision_regression "${COLLISION_CASE}")
    set_tests_properties("scintilla.defense.collision.${COLLISION_CASE}" PROPERTIES LABELS "relay;scintilla;defense;bullet" TIMEOUT 120)
endforeach()

# SC4's real closed parser and bounded retry/intro receipts, without the engine.
add_executable(scintilla_mission_protocol_regression scintilla_mission_protocol_regression.cpp)
target_include_directories(scintilla_mission_protocol_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_mission_protocol_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(PROTOCOL_CASE crewIntroRequiresProfileRulesLayoutAndSixPanels canonicalIdentitiesPreserveWideIntegers requestsAreClosedAndOperationTyped recentRequestRetriesPrecedeIdentityAndNeverExhaustSession everyBindingDimensionParticipatesInAdmission introReceiptIsImmutableAcrossRebindAndRejectsOtherRace introManifestRejectsTimingTraversalAndIntegerWrap)
    add_test(NAME "scintilla.mission.protocol.${PROTOCOL_CASE}" COMMAND scintilla_mission_protocol_regression --case "${PROTOCOL_CASE}")
    set_tests_properties("scintilla.mission.protocol.${PROTOCOL_CASE}" PROPERTIES LABELS "relay;scintilla;mission;protocol" TIMEOUT 120)
endforeach()

# SC4 maximin native-window regressions; source-written, no GL/engine dependency.
add_executable(scintilla_mission_measurement_regression scintilla_mission_measurement_regression.cpp "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_mission.cpp")
target_include_directories(scintilla_mission_measurement_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_mission_measurement_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(MEASUREMENT_CASE crewHeaderRequiresDistinctClosedV3Contract capsuleWindowRequiresStrictlyMoreThanTwoSeconds tetherWindowAcceptsExactThreeSeconds shortFastSpikeCannotRaiseASustainedEnvelope laterSustainedSpeedCanRaiseItsOwnBound gapsAndClockNonAdvancementCannotAccumulateTime clockAndIdentityDomainsStaySeparate fullFootprintExcludesReservedHUDAndProjectionEdges resetDoesNotFabricateANewQualifiedWindow availabilityIntervalsHaveInclusiveExpiryAndRequireReadback capsuleDurationStartsAtReadbackButTetherRetainsCaptureContract actualEvidenceRejectsSlowAndIndividuallyFreshButGappedQueries actualEvidenceRejectsLegacyAndImpossibleAvailabilityFields)
    add_test(NAME "scintilla.mission.measurement.${MEASUREMENT_CASE}" COMMAND scintilla_mission_measurement_regression --case "${MEASUREMENT_CASE}")
    set_tests_properties("scintilla.mission.measurement.${MEASUREMENT_CASE}" PROPERTIES LABELS "relay;scintilla;mission;measurement" TIMEOUT 120)
endforeach()
find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
if(Python3_Interpreter_FOUND)
    add_test(NAME scintilla.mission.qualification.offline COMMAND "${Python3_EXECUTABLE}" -B "${STK_REGRESSION_ROOT}/tools/immersive/scintilla/test_mission_qualification.py")
    add_test(NAME scintilla.mission.collector.offline COMMAND "${Python3_EXECUTABLE}" -B "${STK_REGRESSION_ROOT}/tools/immersive/scintilla/test_collect_mission_measurement.py")
    set_tests_properties(scintilla.mission.qualification.offline scintilla.mission.collector.offline PROPERTIES LABELS "relay;scintilla;mission;offline" TIMEOUT 120)
endif()

# SC5 closed identities and bounded exact retry replies, without engine or GL.
add_executable(scintilla_crew_protocol_regression scintilla_crew_protocol_regression.cpp)
target_include_directories(scintilla_crew_protocol_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_crew_protocol_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(CREW_PROTOCOL_CASE canonicalUInt64AndEveryAuthorityDimension closedOperationAndProfileSchemas onlyIntentAcceptsIncompleteAndLegacyAuthority bindingProofsAreClosedBoundedAndBijective exactReceiptSurvivesAuthorityChange)
    add_test(NAME "scintilla.crew.protocol.${CREW_PROTOCOL_CASE}" COMMAND scintilla_crew_protocol_regression --case "${CREW_PROTOCOL_CASE}")
    set_tests_properties("scintilla.crew.protocol.${CREW_PROTOCOL_CASE}" PROPERTIES LABELS "relay;scintilla;crew;protocol" TIMEOUT 120)
endforeach()

add_executable(scintilla_crew_regression scintilla_crew_regression.cpp "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_crew.cpp")
target_include_directories(scintilla_crew_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_crew_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(CREW_CASE urgentReleaseAdviceDisappearsImmediatelyAfterReleaseThreatChangeOrHold offenseActionsFollowTheActualOwnerAcrossShieldPhasesAndUniversalCrews contradictoryOffenseOwnershipFactsCannotProduceInstructions rolesMatchEveryNativeRaceAndFairnessCalculation partyDisabledAndPreprepareSnapshotsRemainCoherent partyHoldQuorumAndPhysicalAcknowledgementAtEveryFrequency partyPendingProofUsesOriginalAdmissionAndRejectsNegativeRepair partyExpiryIsExclusiveAndZeroCannotReopenAnOldHold partyQuorumUsesOriginalAdmissionAcrossAsyncCompletionAndIdTies partyAuthorityReleaseVetoAndQuotaSurviveReprepare partyGateReasonsDoNotSpendQuotaOrAllowLatentPending partyRejectsDuplicatePeopleOldAuthorityAndBootstrapAcrossRebind advisoryTechnicalHoldAlignsTickAndReadyAllowsPreviousTickPress allAvailabilitySubsetsAreBoundedInjectiveAndComplete defenseAndRecoveryRespectRealHandCapacity heldOccupationIsPreservedAndInterruptionAdviceIsSeparate invalidationBypassesStabilityAndExpiredTargetsDisappear waitingOutranksAffinityWithoutRoleMonopoly commitCreditsAreDistinctBoundedAndPersistForPeople opportunityAccountingSeparatesReadinessPauseAndNoTarget malformedFactsFailClosedAndNextValidTickRecovers maximumFrequencyWaitAndCountersNeverWrap heldPreviewHasRolesButCannotClaimPrepared previewProfileChoiceAndMissedFullRotationRemainHeld disablingNeverPreparedPreviewClearsRolesAndKeepsIntentRevision observedTurnLocksEvenNeverPreparedPreviewAgainstDowngrade nativeTurnCommitIsTheOnlyAdvancement sameRaceReprepareSuspendAndResetKeepCorrectAuthority frozenRosterAndProfileCannotBeRewrittenByRetry invalidInitialPreparationDoesNotSeedRace competenceModuloRemainsExactAtUInt64Maximum)
    add_test(NAME "scintilla.crew.${CREW_CASE}" COMMAND scintilla_crew_regression --case "${CREW_CASE}")
    set_tests_properties("scintilla.crew.${CREW_CASE}" PROPERTIES LABELS "relay;scintilla;crew" TIMEOUT 120)
endforeach()


# Real SC5 controller/rotation/stage transitions. Rendering, logs, asset lookup
# and adaptive-difficulty execution are isolated test boundaries, not gameplay.
add_executable(scintilla_crew_controller_regression
    scintilla_crew_controller_regression.cpp scintilla_crew_controller_boundaries.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/relay_session_controller.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/remote/relay_state_publisher.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_crew.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_mission.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/relay_pilot_rotation.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/arcade_session_state_machine.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/crew_role_manager.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/crew_event_director.cpp")
target_include_directories(scintilla_crew_controller_regression PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/crew_controller_stubs" "${STK_REGRESSION_ROOT}/src")
target_compile_definitions(scintilla_crew_controller_regression PRIVATE ENABLE_IMMERSIVE_RELAY)
if(UNIX)
    target_link_libraries(scintilla_crew_controller_regression PRIVATE Threads::Threads)
endif()
set_target_properties(scintilla_crew_controller_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(CREW_CONTROLLER_CASE initialGateBlocksAllStartsAndAdministrativeTurn confirmedStageACommitsIntoHeldStageB automaticStageAAlsoClosesBeforePhysics pausedStageAAllowsPhoneConfirmationButHealthyPresentationIsRequired skipCreatesOneAdministrativeTurnAndCannotFarmStageB preparedStageBWaitsForEveryExplicitReadyResumeIngress resetsPreserveRequirementAndCloseOldAdmission gameplayResumeRequiresStageBAndLegacyRemainsIndependent naturalCrewReportUsesExactMissionTupleAndSharedPublisherEnvelope artificialCrewReportUsesCopiedObservationAndCannotAcquireLateCredits administrativeNativeHandoffMakesNaturalCrewReportNonComparable resetKeepsBothImmutableSessionArchivesAndTheSamePublishedEnvelope newRacePreviewAbortHasCompleteCrewEnvelopeAndMissingRosterCannotBecomeLegacy committedHandoffThenAbortBeforeEngineObservationUsesNativeAuthorityOnly)
    add_test(NAME "scintilla.crew.controller.${CREW_CONTROLLER_CASE}" COMMAND scintilla_crew_controller_regression --case "${CREW_CONTROLLER_CASE}")
    set_tests_properties("scintilla.crew.controller.${CREW_CONTROLLER_CASE}" PROPERTIES LABELS "relay;scintilla;crew;handoff" TIMEOUT 120 ENVIRONMENT "SCINTILLA_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}")
endforeach()

add_executable(scintilla_crew_zones_regression scintilla_crew_zones_regression.cpp "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_crew_zones.cpp")
target_include_directories(scintilla_crew_zones_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_crew_zones_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(CREW_ZONE_CASE authoringNeverQualifiesAndLoadingIsAtomic exactNativeTrackNodesAndDirectionAreRequired qualificationBindsEveryZoneBuildAndEvidenceHash remainingDistanceIsCheckedAtTheLastConsent rescueTeleportBackwardAndClockGapsBreakContinuity sameTickRecheckCannotAcceptMovedKartOrFinalSegment nativeEvidenceRequiresClosedProvenanceAndReturnsBoundedProofHashes nativeEvidenceRejectsSyntheticStaleFailedAndIncompleteObservations)
    add_test(NAME "scintilla.crew.zones.${CREW_ZONE_CASE}" COMMAND scintilla_crew_zones_regression --case "${CREW_ZONE_CASE}")
    set_tests_properties("scintilla.crew.zones.${CREW_ZONE_CASE}" PROPERTIES LABELS "relay;scintilla;crew;zones" TIMEOUT 120)
endforeach()

add_executable(scintilla_crew_boost_math_regression scintilla_crew_boost_math_regression.cpp)
target_include_directories(scintilla_crew_boost_math_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_crew_boost_math_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(CREW_MATH_CASE partyTangentImpulsePreservesNormalAndMatchesCounterexample partyTotalCapUsesCurrentSlowdownAndRejectsNoUsefulImpulse partyFloatAndDoubleConversionsRespectBoundedImpulse partyMalformedNumbersAndNonunitNormalsFailBeforeWriting partyNumericalToleranceRejectsOutOfScaleInputs partyDurationsUseFloorAndNativeInt16BoundsAtEveryFrequency)
    add_test(NAME "scintilla.crew.math.${CREW_MATH_CASE}" COMMAND scintilla_crew_boost_math_regression --case "${CREW_MATH_CASE}")
    set_tests_properties("scintilla.crew.math.${CREW_MATH_CASE}" PROPERTIES LABELS "relay;scintilla;crew;math" TIMEOUT 120)
endforeach()
if(Python3_Interpreter_FOUND)
    add_test(NAME scintilla.crew.physics_source_contract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_LIST_DIR}/check_scintilla_crew_physics_source.py")
    set_tests_properties(scintilla.crew.physics_source_contract PROPERTIES LABELS "relay;scintilla;crew;source" TIMEOUT 120)
endif()

# SC5 immutable report semantics and bounded exact JSON, using production cores.
add_executable(scintilla_crew_result_regression scintilla_crew_result_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_crew.cpp")
target_include_directories(scintilla_crew_result_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_crew_result_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(CREW_RESULT_CASE naturalSnapshotPreservesEveryParticipantAndProfileBucket reportParametersMatchExactIntegerRulesAtEveryFPS identityMismatchAndMalformedObservationCannotFreeze frozenRecordOutlivesLateCreditsPauseResetAndProfileChange artificialAdminTestSaturationCannotBeComparable exclusiveArchiveNeverOverwritesAndKeysBoundNames)
    add_test(NAME "scintilla.crew.result.${CREW_RESULT_CASE}" COMMAND scintilla_crew_result_regression --case "${CREW_RESULT_CASE}")
    set_tests_properties("scintilla.crew.result.${CREW_RESULT_CASE}" PROPERTIES LABELS "relay;scintilla;crew;result" TIMEOUT 120 ENVIRONMENT "SCINTILLA_TEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
if(Python3_Interpreter_FOUND)
    add_test(NAME scintilla.crew.report_source_contract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_LIST_DIR}/check_scintilla_crew_report_source.py")
    set_tests_properties(scintilla.crew.report_source_contract PROPERTIES LABELS "relay;scintilla;crew;source" TIMEOUT 120)
    add_test(NAME scintilla.crew.zones.offline COMMAND "${Python3_EXECUTABLE}" -B "${STK_REGRESSION_ROOT}/tools/immersive/scintilla/test_party_zones.py")
    set_tests_properties(scintilla.crew.zones.offline PROPERTIES LABELS "relay;scintilla;crew;zones;offline;authoring" TIMEOUT 120)
endif()

add_executable(scintilla_crew_services_regression scintilla_crew_services_regression.cpp
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_crew.cpp"
    "${STK_REGRESSION_ROOT}/src/modes/immersive_relay/scintilla_defense.cpp")
target_include_directories(scintilla_crew_services_regression PRIVATE "${STK_REGRESSION_ROOT}/src")
set_target_properties(scintilla_crew_services_regression PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
foreach(CREW_SERVICE_CASE normalWinsAtTheProductionCommitPointAndCannotChargeTwice partySuccessChargesOnceAndCreditsOnlyTheCommittedQuorum rejectedPhysicsHasNeitherChargeCooldownQuotaNorLateRetry lateHoldTerminalAndSameTickThreatWinBeforeOptionalPhysics)
    add_test(NAME "scintilla.crew.services.${CREW_SERVICE_CASE}" COMMAND scintilla_crew_services_regression --case "${CREW_SERVICE_CASE}")
    set_tests_properties("scintilla.crew.services.${CREW_SERVICE_CASE}" PROPERTIES LABELS "relay;scintilla;crew;adapter" TIMEOUT 120)
endforeach()

if(Python3_Interpreter_FOUND)
    add_test(NAME scintilla.crew.intro.offline COMMAND "${Python3_EXECUTABLE}" -B "${STK_REGRESSION_ROOT}/tools/immersive/scintilla/test_crew_intro.py")
    set_tests_properties(scintilla.crew.intro.offline PROPERTIES LABELS "relay;scintilla;crew;offline;authoring" TIMEOUT 120)
endif()

if(Python3_Interpreter_FOUND)
    add_test(NAME scintilla.crew.wire_source_contract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_LIST_DIR}/check_scintilla_crew_wire_source.py")
    set_tests_properties(scintilla.crew.wire_source_contract PROPERTIES LABELS "relay;scintilla;crew;source" TIMEOUT 120)
    add_test(NAME scintilla.crew.render_source_contract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_LIST_DIR}/check_scintilla_crew_render_source.py")
    set_tests_properties(scintilla.crew.render_source_contract PROPERTIES LABELS "relay;scintilla;crew;source;numeric" TIMEOUT 120)
endif()
