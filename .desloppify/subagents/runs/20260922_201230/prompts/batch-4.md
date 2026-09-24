You are a focused subagent reviewer for a single holistic investigation batch.

Repository root: D:\dev\CYD_SD_Tester
Blind packet: D:\dev\CYD_SD_Tester\.desloppify\review_packet_blind.json
Batch index: 4
Batch name: error_consistency
Batch rationale: error_consistency review

REVIEWER PERSONA: Migrator
Attention bias: Deprecated patterns, half-migrated code, stale shims, and dual-path confusion
Key question: What should have been cleaned up already?

The persona biases where you spend attention, not the scoring rules. Apply the same evidence and confidence thresholds as every other batch.
DIMENSION TO EVALUATE:

## error_consistency
Consistent error strategies, preserved context, predictable failure modes
Look for:
- Mixed error strategies: some functions throw, others return null, others use Result types
- Error context lost at boundaries: catch-and-rethrow without wrapping original
- Inconsistent error types: custom error classes in some modules, bare strings in others
- Silent error swallowing: catches that log but don't propagate or recover
- Missing error handling on I/O boundaries (file, network, parse operations)
Skip:
- Intentional error boundaries at top-level handlers
- Different strategies for different layers (e.g. Result in core, throw in CLI)

YOUR TASK: Read the code for this batch's dimension. Judge how well the codebase serves a developer from that perspective. The dimension rubric above defines what good looks like. Cite specific observations that explain your judgment.

Mechanical scan evidence — navigation aid, not scoring evidence:
The blind packet contains `holistic_context.scan_evidence` with aggregated signals from all mechanical detectors — including complexity hotspots, error hotspots, signal density index, boundary violations, and systemic patterns. Use these as starting points for where to look beyond the seed files.

Previously flagged issues — navigation aid, not scoring evidence:
Check whether open issues still exist. Do not re-report resolved or deferred items.
If several past issues share a root cause, call that out.

  Resolved (10):
    - [fixed] runQuickBenchmark() captures no SD error code on preAllocate()/seekSet() failure, unlike every other I/O failure branch in the same function (note: Extracted the prealloc/seek/write/read/seek/verify sequence in runQuickBenchmark() into a new runFileBenchmarkSequence() helper (benchmark.cpp) using early returns, with recordSdError() called on every failing step (preallocate, each seekSet, and the timed steps which already recorded internally) -- this both closes the missing-error-capture gap and reads more clearly than the prior nested if/else chain. Gave rawReadAtCurrentSpeed() proper uint8_t& errorCode/errorData out-params (benchmark_raw.cpp + benchmark_internal.h) instead of discarding its precise per-LBA recordSdError() result into unused locals; both call sites (runQuickBenchmark in benchmark.cpp, runSpiScalingBenchmark in spi_scale_benchmark.cpp) now receive that error directly instead of redundantly re-deriving a coarser one. Made ui_geometry.cpp's buildScaleSummary() fall back to showing the first attempted point's SD error code when no speed qualified, instead of leaving the SPI-scale summary line blank -- so SpiScalePoint.errorCode/errorData are now genuinely read and displayed, with 2 new Unity tests covering the fallback and empty-result cases.)
    - [fixed] rawReadAtCurrentSpeed() computes a precise per-LBA SD error internally but discards it, forcing both callers to redundantly re-derive a coarser version (note: Extracted the prealloc/seek/write/read/seek/verify sequence in runQuickBenchmark() into a new runFileBenchmarkSequence() helper (benchmark.cpp) using early returns, with recordSdError() called on every failing step (preallocate, each seekSet, and the timed steps which already recorded internally) -- this both closes the missing-error-capture gap and reads more clearly than the prior nested if/else chain. Gave rawReadAtCurrentSpeed() proper uint8_t& errorCode/errorData out-params (benchmark_raw.cpp + benchmark_internal.h) instead of discarding its precise per-LBA recordSdError() result into unused locals; both call sites (runQuickBenchmark in benchmark.cpp, runSpiScalingBenchmark in spi_scale_benchmark.cpp) now receive that error directly instead of redundantly re-deriving a coarser one. Made ui_geometry.cpp's buildScaleSummary() fall back to showing the first attempted point's SD error code when no speed qualified, instead of leaving the SPI-scale summary line blank -- so SpiScalePoint.errorCode/errorData are now genuinely read and displayed, with 2 new Unity tests covering the fallback and empty-result cases.)
    - [fixed] SpiScalePoint.errorCode/errorData are populated by recordSdError() but never read back anywhere, unlike AttemptResult's identical fields (note: Extracted the prealloc/seek/write/read/seek/verify sequence in runQuickBenchmark() into a new runFileBenchmarkSequence() helper (benchmark.cpp) using early returns, with recordSdError() called on every failing step (preallocate, each seekSet, and the timed steps which already recorded internally) -- this both closes the missing-error-capture gap and reads more clearly than the prior nested if/else chain. Gave rawReadAtCurrentSpeed() proper uint8_t& errorCode/errorData out-params (benchmark_raw.cpp + benchmark_internal.h) instead of discarding its precise per-LBA recordSdError() result into unused locals; both call sites (runQuickBenchmark in benchmark.cpp, runSpiScalingBenchmark in spi_scale_benchmark.cpp) now receive that error directly instead of redundantly re-deriving a coarser one. Made ui_geometry.cpp's buildScaleSummary() fall back to showing the first attempted point's SD error code when no speed qualified, instead of leaving the SPI-scale summary line blank -- so SpiScalePoint.errorCode/errorData are now genuinely read and displayed, with 2 new Unity tests covering the fallback and empty-result cases.)
    - [fixed] runQuickBenchmark() captures no SD error code when the raw sector read fails, unlike its open/write/read/verify failure paths (note: Reformatted cardTypeName()/fsName() (sd_card.cpp) and attemptStageName()/resolveVendor() (card_identity.cpp) definitions to one-arg-per-line, matching their own headers and the codebase convention; added a recordSdError() call in runQuickBenchmark() after a raw-read failure, populating result.ioErrorCode/ioErrorData (mirroring spi_scale_benchmark.cpp's existing pattern for the same call); surfaced CardInfo.volumeErrorCode/volumeErrorData via a new 'mount error: XX/XX' line in printCardReport() when the volume fails to mount.)
    - [fixed] CardInfo.volumeErrorCode/volumeErrorData and SpiScalePoint.errorCode/errorData are populated by recordSdError() but never read back anywhere, unlike AttemptResult's identical fields (note: Reformatted cardTypeName()/fsName() (sd_card.cpp) and attemptStageName()/resolveVendor() (card_identity.cpp) definitions to one-arg-per-line, matching their own headers and the codebase convention; added a recordSdError() call in runQuickBenchmark() after a raw-read failure, populating result.ioErrorCode/ioErrorData (mirroring spi_scale_benchmark.cpp's existing pattern for the same call); surfaced CardInfo.volumeErrorCode/volumeErrorData via a new 'mount error: XX/XX' line in printCardReport() when the volume fails to mount.)
    - [fixed] BenchmarkResult captures no SD error code on failure, unlike CardInfo/AttemptResult/SpiScalePoint (note: Documented runQuickBenchmark()'s no-side-effect/default-result contract in benchmark.h; surfaced BenchmarkResult.cleanupOk in printBenchmarkResult and removed dead CardInfo::csdOk (with its test assertion); confirmed identityCheckName()/header() defs already matched their headers (stale finding from before a mid-review fix landed); split makeSdConfig(uint32_t hz) in sd_card.h/.cpp to one-arg-per-line (the remaining compact holdout); added a BenchmarkProgressFn callback threaded through runChunkedIo()/timedFileWrite/timedFileRead/verifyFile/runQuickBenchmark so benchmark.cpp no longer includes ui/display_ui.h, with main.cpp passing uiShowProgress as the callback; added CardInfo-style ioErrorCode/ioErrorData to BenchmarkResult, populated via recordSdError() on any chunk-transfer failure and on sd.open() failure, and printed in printBenchmarkResult(); switched rawReadAtCurrentSpeed() to call the shared recordSdError() with a formatted 'RAW READ LBA %lu' label instead of its own inline Serial.printf; updated CLAUDE.md's Architecture section to describe main.cpp's inlined showDiagnostics() instead of the removed diagnostics.* module; removed the unused RAW_TEST_SECTORS constant from config.h; renamed touch.h/touch.cpp's readTouch() sx/sy params to screenX/screenY matching mapTouchToScreen()'s naming; moved scaleGraphMaxSpeed()/scalePointCoords()/buildScaleSummary() from display_results.cpp into ui_geometry.h/.cpp (now in the native test_build_src_filter) with 9 new Unity tests covering edge cases (single-point graphs, failed/zero-speed points, first-successful-point summary); retyped ChunkIoFn's void* context to a concrete uint32_t* crcAccum since only chunkReadAndCrc ever used it.)
    - [fixed] sd.open() failure in runQuickBenchmark() produces no Serial output at all, unlike every other SD failure branch (note: Documented runQuickBenchmark()'s no-side-effect/default-result contract in benchmark.h; surfaced BenchmarkResult.cleanupOk in printBenchmarkResult and removed dead CardInfo::csdOk (with its test assertion); confirmed identityCheckName()/header() defs already matched their headers (stale finding from before a mid-review fix landed); split makeSdConfig(uint32_t hz) in sd_card.h/.cpp to one-arg-per-line (the remaining compact holdout); added a BenchmarkProgressFn callback threaded through runChunkedIo()/timedFileWrite/timedFileRead/verifyFile/runQuickBenchmark so benchmark.cpp no longer includes ui/display_ui.h, with main.cpp passing uiShowProgress as the callback; added CardInfo-style ioErrorCode/ioErrorData to BenchmarkResult, populated via recordSdError() on any chunk-transfer failure and on sd.open() failure, and printed in printBenchmarkResult(); switched rawReadAtCurrentSpeed() to call the shared recordSdError() with a formatted 'RAW READ LBA %lu' label instead of its own inline Serial.printf; updated CLAUDE.md's Architecture section to describe main.cpp's inlined showDiagnostics() instead of the removed diagnostics.* module; removed the unused RAW_TEST_SECTORS constant from config.h; renamed touch.h/touch.cpp's readTouch() sx/sy params to screenX/screenY matching mapTouchToScreen()'s naming; moved scaleGraphMaxSpeed()/scalePointCoords()/buildScaleSummary() from display_results.cpp into ui_geometry.h/.cpp (now in the native test_build_src_filter) with 9 new Unity tests covering edge cases (single-point graphs, failed/zero-speed points, first-successful-point summary); retyped ChunkIoFn's void* context to a concrete uint32_t* crcAccum since only chunkReadAndCrc ever used it.)
    - [fixed] rawReadAtCurrentSpeed() hand-rolls its own SD error log format instead of using the shared recordSdError() helper (note: Documented runQuickBenchmark()'s no-side-effect/default-result contract in benchmark.h; surfaced BenchmarkResult.cleanupOk in printBenchmarkResult and removed dead CardInfo::csdOk (with its test assertion); confirmed identityCheckName()/header() defs already matched their headers (stale finding from before a mid-review fix landed); split makeSdConfig(uint32_t hz) in sd_card.h/.cpp to one-arg-per-line (the remaining compact holdout); added a BenchmarkProgressFn callback threaded through runChunkedIo()/timedFileWrite/timedFileRead/verifyFile/runQuickBenchmark so benchmark.cpp no longer includes ui/display_ui.h, with main.cpp passing uiShowProgress as the callback; added CardInfo-style ioErrorCode/ioErrorData to BenchmarkResult, populated via recordSdError() on any chunk-transfer failure and on sd.open() failure, and printed in printBenchmarkResult(); switched rawReadAtCurrentSpeed() to call the shared recordSdError() with a formatted 'RAW READ LBA %lu' label instead of its own inline Serial.printf; updated CLAUDE.md's Architecture section to describe main.cpp's inlined showDiagnostics() instead of the removed diagnostics.* module; removed the unused RAW_TEST_SECTORS constant from config.h; renamed touch.h/touch.cpp's readTouch() sx/sy params to screenX/screenY matching mapTouchToScreen()'s naming; moved scaleGraphMaxSpeed()/scalePointCoords()/buildScaleSummary() from display_results.cpp into ui_geometry.h/.cpp (now in the native test_build_src_filter) with 9 new Unity tests covering edge cases (single-point graphs, failed/zero-speed points, first-successful-point summary); retyped ChunkIoFn's void* context to a concrete uint32_t* crcAccum since only chunkReadAndCrc ever used it.)
    - [fixed] sd.volumeBegin() failure captures no error code, unlike every other SD failure path in scanCard() (note: Batch 2/3: moved allocateDmaBuffer() into hardware/sd_card.h/.cpp so sd_card.cpp and benchmark_raw.cpp share one implementation instead of duplicating it; added CardInfo::volumeErrorCode/volumeErrorData, populated via recordSdError() on sd.volumeBegin() failure; moved uiWaitForTap()'s definition into display_ui_internal.cpp next to header()/lineRow(); added a sd.card() null check at the top of rawReadAtCurrentSpeed(); strengthened the scanCard()-restores-state doc comment on runSpiScalingBenchmark() in benchmark.h; extracted runChunkedIo() to remove the timedFileWrite/timedFileRead/verifyFile triplication; split uiShowSpiScaleResult() into scaleGraphMaxSpeed()/drawScaleGraphFrame()/scalePointCoords()/drawScalePoint()/buildScaleSummary() helpers; reformatted touch.cpp and tightened bench_math.cpp's one compact expression to the dominant vertical style; documented runSpiScalingBenchmark()'s hidden rescan on its declaration in benchmark.h; added explicit '../hardware/card_format.h' and '../hardware/card_identity.h' includes to every ui/ and benchmark/ file that calls those functions, instead of relying on sd_card.h's transitive include.)
    - [fixed] DMA buffer allocation failure is handled by two independently-maintained code paths instead of the shared helper (note: Batch 2/3: moved allocateDmaBuffer() into hardware/sd_card.h/.cpp so sd_card.cpp and benchmark_raw.cpp share one implementation instead of duplicating it; added CardInfo::volumeErrorCode/volumeErrorData, populated via recordSdError() on sd.volumeBegin() failure; moved uiWaitForTap()'s definition into display_ui_internal.cpp next to header()/lineRow(); added a sd.card() null check at the top of rawReadAtCurrentSpeed(); strengthened the scanCard()-restores-state doc comment on runSpiScalingBenchmark() in benchmark.h; extracted runChunkedIo() to remove the timedFileWrite/timedFileRead/verifyFile triplication; split uiShowSpiScaleResult() into scaleGraphMaxSpeed()/drawScaleGraphFrame()/scalePointCoords()/drawScalePoint()/buildScaleSummary() helpers; reformatted touch.cpp and tightened bench_math.cpp's one compact expression to the dominant vertical style; documented runSpiScalingBenchmark()'s hidden rescan on its declaration in benchmark.h; added explicit '../hardware/card_format.h' and '../hardware/card_identity.h' includes to every ui/ and benchmark/ file that calls those functions, instead of relying on sd_card.h's transitive include.)

Explore past review issues:
  desloppify show review --no-budget              # all open review issues
  desloppify show review --status deferred         # deferred issues

Phase 1 — Observe:
1. Read the blind packet's `system_prompt` — scoring rules and calibration.
2. Study the dimension rubric (description, look_for, skip).
3. Review the existing characteristics list — which are settled? Which are positive? What needs updating?
4. Explore the codebase freely. Use scan evidence, historical issues, and mechanical findings as navigation aids.
5. Adjudicate mechanical concern signals (confirm/dismiss with fingerprint).
6. Augment the characteristics list via context_updates: positive patterns (positive: true), neutral characteristics, design insights.
7. Collect defects for issues[].
8. Respect scope controls: exclude files/directories marked by `exclude`, `suppress`, or non-production zone overrides.
9. Output a Phase 1 summary: list ALL characteristics for this dimension (existing + new, mark [+] for positive) and all defects collected. This is your consolidated reference for Phase 2.

Phase 2 — Judge (after Phase 1 is complete):
10. Keep issues and scoring scoped to this batch's dimension.
11. Return 0-10 issues for this batch (empty array allowed).
12. For error_consistency, use evidence from `holistic_context.errors.exception_hotspots` — files with concentrated exception handling issues. Investigate whether error handling is designed or accidental. Check for broad catches masking specific failure modes.
13. Complete `dimension_judgment`: write dimension_character (synthesizing characteristics and defects) then score_rationale. Set the score LAST.
14. Output context_updates with your Phase 1 observations. Use `add` with a clear header (5-10 words) and description (1-3 sentences focused on WHY, not WHAT). Positive patterns get `positive: true`. New insights can be `settled: true` when confident. Use `settle` to promote existing unsettled insights. Use `remove` for insights no longer true. Omit context_updates if no changes.
15. Do not edit repository files.
16. Return ONLY valid JSON, no markdown fences.

Scope enums:
- impact_scope: "local" | "module" | "subsystem" | "codebase"
- fix_scope: "single_edit" | "multi_file_refactor" | "architectural_change"

Output schema:
{
  "batch": "error_consistency",
  "batch_index": 4,
  "assessments": {"<dimension>": <0-100 with one decimal place>},
  "dimension_notes": {
    "<dimension>": {
      "evidence": ["specific code observations"],
      "impact_scope": "local|module|subsystem|codebase",
      "fix_scope": "single_edit|multi_file_refactor|architectural_change",
      "confidence": "high|medium|low",
      "issues_preventing_higher_score": "required when score >85.0",
      "sub_axes": {"abstraction_leverage": 0-100, "indirection_cost": 0-100, "interface_honesty": 0-100, "delegation_density": 0-100, "definition_directness": 0-100, "type_discipline": 0-100}  // required for abstraction_fitness when evidence supports it; all one decimal place
    }
  },
  "dimension_judgment": {
    "<dimension>": {
      "dimension_character": "2-3 sentences characterizing the overall nature of this dimension, synthesizing both positive characteristics and defects",
      "score_rationale": "2-3 sentences explaining the score, referencing global anchors"
    }  // required for every assessed dimension; do not omit
  },
  "issues": [{
    "dimension": "<dimension>",
    "identifier": "short_id",
    "summary": "one-line defect summary",
    "related_files": ["relative/path.py"],
    "evidence": ["specific code observation"],
    "suggestion": "concrete fix recommendation",
    "confidence": "high|medium|low",
    "impact_scope": "local|module|subsystem|codebase",
    "fix_scope": "single_edit|multi_file_refactor|architectural_change",
    "root_cause_cluster": "optional_cluster_name_when_supported_by_history",
    "concern_verdict": "confirmed|dismissed  // for concern signals only",
    "concern_fingerprint": "abc123  // required when dismissed; copy from signal fingerprint",
    "reasoning": "why dismissed  // optional, for dismissed only"
  }],
  "retrospective": {
    "root_causes": ["optional: concise root-cause hypotheses"],
    "likely_symptoms": ["optional: identifiers that look symptom-level"],
    "possible_false_positives": ["optional: prior concept keys likely mis-scoped"]
  },
  "context_updates": {
    "<dimension>": {
      "add": [{"header": "short label", "description": "why this is the way it is", "settled": true|false, "positive": true|false}],
      "remove": ["header of insight to remove"],
      "settle": ["header of insight to mark as settled"],
      "unsettle": ["header of insight to unsettle"]
    }  // omit context_updates entirely if no changes
  }
}

// context_updates example:
{
  "naming_quality": {
    "add": [
      {
        "header": "Short utility names in base/file_paths.py",
        "description": "rel(), loc() are deliberately terse \u00e2\u20ac\u201d high-frequency helpers where brevity aids readability at call sites. Full names would add noise without improving clarity.",
        "settled": true,
        "positive": true
      }
    ],
    "settle": [
      "Snake case convention"
    ]
  }
}
