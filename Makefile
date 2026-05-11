# ai-embedded-platform — convenience targets
#
# Run `make help` to see all available targets.

.PHONY: help test test-wheels test-projects test-changed test-fast test-parallel audit audit-fix audit-fix-apply audit-save audit-compare audit-report audit-quick audit-deep audit-compile audit-sanitizer audit-coverage coverage coverage-wheel coverage-scan coverage-scan-all check-coverage bootstrap list graph downstream sync-vsf sync-vsf-status clean clean-worktrees check check-full ship inventory metrics metrics-save metrics-compare metrics-quality test-scaffold test-property test-null test-sentinel test-boundary test-contract test-differential test-mutate frama-c handoff new-wheel promote-wheel release arm-size-audit arm-stack-audit find-refs dispatch verify fuzz batch-scaffold memory-audit doc-drift check-readme-examples check-complexity check-clones

help:
	@echo "Usage: make <target>"
	@echo ""
	@echo "Testing:"
	@echo "  test              all tests (wheels + projects)"
	@echo "  test-wheels       wheel tests only"
	@echo "  test-projects     project tests only"
	@echo "  test-changed      only tests affected by uncommitted changes"
	@echo "  test-fast         stop on first failure"
	@echo "  test-parallel     run all tests in parallel (fastest)"
	@echo "  test-scaffold     verify new_wheel.sh templates compile"
	@echo "  test-property     property-based tests (all prop_*.c)"
	@echo "  test-null         NULL parameter rejection tests (all test_null_*.c)"
	@echo "  test-sentinel     sentinel boundary tests (all test_sentinel_*.c)"
	@echo "  test-boundary     type-boundary value tests (all test_boundary_*.c)"
	@echo "  test-contract     unit tests with -DCONTRACT_ENABLED (runtime assertions)"
	@echo "  test-differential compare wheel output against reference implementations"
	@echo "  test-mutate       mutation testing (kill rate per wheel)"
	@echo "  frama-c           Frama-C WP formal verification (5 annotated wheels)"
	@echo "  fuzz              LibFuzzer coverage-guided protocol fuzzing"
	@echo "  coverage          tests with gcov coverage report (all wheels)"
	@echo "  coverage-wheel W=x per-wheel coverage with branch detail"
	@echo "  coverage-scan     scan all wheels, show sub-80% (gcov-tool merge)"
	@echo "  coverage-scan-all show all wheels sorted by coverage"
	@echo "  check-coverage    gate: fail if any wheel < 80% (sprint boundary)"
	@echo "  downstream W=x    test downstream consumers of wheel x"
	@echo ""
	@echo "Audit:"
	@echo "  audit             run wheel audit"
	@echo "  audit-fix         auto-fix mechanical findings (dry-run)"
	@echo "  audit-fix-apply   auto-fix and apply"
	@echo "  audit-save        save audit snapshot for trend tracking"
	@echo "  audit-compare     compare latest two audit snapshots"
	@echo "  audit-report      unified report across all audit tools"
	@echo "  audit-deep        full deep audit (pattern grep + sanitizer + all layers)"
	@echo "  audit-quick       deep audit L1+L2 (pattern grep + sanitizer)"
	@echo "  verify            CBMC bounded model checking (verify_*.c wheels)"
	@echo "  audit-compile     compiler warning analysis (all -W flags)"
	@echo "  audit-sanitizer   ASan+UBSan build and run"
	@echo "  audit-coverage    gcov line coverage per wheel"
	@echo "  arm-size-audit    ARM cross-compile size report"
	@echo "  arm-stack-audit   ARM per-function stack usage report"
	@echo "  check-readme-examples  compile C blocks from all wheel README files"
	@echo "  check-complexity  cyclomatic complexity audit (Q14, threshold 15)"
	@echo "  check-clones      detect duplicate code blocks across wheels (Q26)"
	@echo ""
	@echo "Workflow:"
	@echo "  check             test + null/sentinel/boundary + scaffold + audit + coverage gate + doc drift + memory audit (pre-push)"
	@echo "  check-full        check + compile analysis + sanitizer (superset of check)"
	@echo "  ship M=\"msg\"      test + audit + commit + push"
	@echo "  new-wheel         scaffold a new wheel (interactive)"
	@echo "  promote-wheel     promote wheel across layers"
	@echo "  release           create version tag + changelog"
	@echo "  find-refs W=x     find all references to wheel x"
	@echo "  dispatch          dispatch Claude agent in isolated worktree"
	@echo "  batch-scaffold    scaffold + commit multiple wheels at once"
	@echo "                    SPECS=\"name:layer[:hal[:addr]] ...\" (space-separated)"
	@echo "  memory-audit      check memory/MEMORY.md consistency"
	@echo "  doc-drift         check and fix doc/code drift"
	@echo ""
	@echo "Session:"
	@echo "  bootstrap         print session context"
	@echo "  handoff           generate session state for handoff"
	@echo "  list              list all discoverable test targets"
	@echo "  graph             wheel dependency graph (Mermaid)"
	@echo "  inventory         update CLAUDE.md wheel counts"
	@echo ""
	@echo "Metrics:"
	@echo "  metrics           show platform metrics"
	@echo "  metrics-save      save metrics snapshot"
	@echo "  metrics-compare   compare with last snapshot"
	@echo "  metrics-quality   quality-focused metrics"
	@echo ""
	@echo "Maintenance:"
	@echo "  sync-vsf          sync vsf/ with upstream"
	@echo "  sync-vsf-status   show vsf version status"
	@echo "  clean             remove all build directories"
	@echo "  clean-worktrees   remove all worktree-* branches and directories"

test:
	bash tooling/test/test_all.sh

test-wheels:
	bash tooling/test/test_all.sh --wheel

test-projects:
	bash tooling/test/test_all.sh --project

test-changed:
	bash tooling/test/test_all.sh --changed

test-fast:
	bash tooling/test/test_all.sh --wheel --fail-fast

test-parallel:
	bash tooling/test/test_all.sh --parallel

audit:
	python3 tooling/audit/audit_wheels.py

audit-fix:
	python3 tooling/audit/audit_fix.py

audit-fix-apply:
	python3 tooling/audit/audit_fix.py --apply

audit-save:
	python3 tooling/audit/audit_wheels.py --fast --quiet --save

audit-compare:
	python3 tooling/audit/audit_wheels.py --compare

audit-report:
	bash tooling/audit/audit_report.sh

coverage:
	bash tooling/test/test_all.sh --wheel --coverage

coverage-wheel:
	@test -n "$(W)" || { echo "Usage: make coverage-wheel W=<wheel-name>"; exit 1; }
	bash tooling/test/coverage_wheel.sh "$(W)"

coverage-scan:
	bash tooling/test/coverage_scan.sh

coverage-scan-all:
	bash tooling/test/coverage_scan.sh --all

check-coverage:
	bash tooling/test/coverage_scan.sh --gate

bootstrap:
	bash tooling/session/bootstrap.sh

list:
	bash tooling/test/test_all.sh --list

graph:
	python3 tooling/audit/audit_wheels.py --graph

downstream:
	@test -n "$(W)" || { echo "Usage: make downstream W=<wheel-name>"; exit 1; }
	bash tooling/test/test_downstream.sh "$(W)"

sync-vsf:
	bash tooling/ci/sync_vsf.sh

sync-vsf-status:
	bash tooling/ci/sync_vsf.sh --status

clean:
	@echo "Removing build_* directories under wheels/ and projects/..."
	@find wheels/ -type d -name "build_*" -exec rm -rf {} + 2>/dev/null || true
	@find projects/ -type d -name "build_*" -exec rm -rf {} + 2>/dev/null || true
	@find _eval/ -type d -name "build" -exec rm -rf {} + 2>/dev/null || true
	@echo "Done."

clean-worktrees:
	bash tooling/session/dispatch_agent.sh --cleanup

check:
	bash tooling/test/test_all.sh --parallel && $(MAKE) test-null && $(MAKE) test-sentinel && $(MAKE) test-boundary && $(MAKE) test-scaffold && python3 tooling/audit/audit_wheels.py --fast --quiet && bash tooling/test/coverage_scan.sh --gate --jobs 4 && bash tooling/audit/check_readme_examples.sh && python3 tooling/audit/doc_drift_check.py --fix && python3 tooling/audit/memory_audit.py

check-full:
	bash tooling/test/test_all.sh --parallel && $(MAKE) test-null && $(MAKE) test-sentinel && $(MAKE) test-boundary && $(MAKE) test-scaffold && python3 tooling/audit/audit_wheels.py --fast --quiet && bash tooling/test/coverage_scan.sh --gate --jobs 4 && python3 tooling/audit/audit_compile.py --changed --quiet && python3 tooling/audit/audit_sanitizer.py --changed --quiet && python3 tooling/audit/doc_drift_check.py --fix && python3 tooling/audit/memory_audit.py

ship:
	@test -n "$(M)" || { echo "Usage: make ship M=\"commit message\""; exit 1; }
	bash tooling/test/test_all.sh
	python3 tooling/audit/audit_wheels.py --quiet
	git add -u
	@printf '%s\n' '$(subst ','\'',$(M))' | git commit -F -
	git push

inventory:
	bash tooling/ci/update_wheel_inventory.sh --apply

metrics:
	bash tooling/metrics/platform_metrics.sh

metrics-save:
	bash tooling/metrics/platform_metrics.sh --save

metrics-compare:
	bash tooling/metrics/platform_metrics.sh --compare

metrics-quality:
	bash tooling/metrics/platform_metrics.sh --quality

handoff:
	bash tooling/session/handoff.sh

test-scaffold:
	bash tooling/test/test_scaffold.sh

test-property:
	bash tooling/test/run_generated_tests.sh "prop_*.c" "property" --extra-cflags="-Wextra"

test-null:
	bash tooling/test/run_generated_tests.sh "test_null_*.c" "NULL parameter"

test-sentinel:
	bash tooling/test/run_generated_tests.sh "test_sentinel_*.c" "sentinel boundary"

test-boundary:
	bash tooling/test/run_generated_tests.sh "test_boundary_*.c" "type-boundary"

test-contract:
	bash tooling/test/run_generated_tests.sh "test_*.c" "contract" --define=CONTRACT_ENABLED --filter-contract --use-main-test

test-differential:
	bash tooling/test/test_differential.sh

test-mutate:
	bash tooling/test/test_mutation.sh

verify:
	@echo "CBMC verification (requires cbmc in PATH)..."
	@if ! command -v cbmc >/dev/null 2>&1; then echo "cbmc not installed -- skipping"; exit 0; fi
	@fail=0; total=0; \
	for f in $$(find wheels/ -name 'verify_*.c' 2>/dev/null); do \
		dir=$$(dirname "$$f"); wheel_dir=$$(dirname "$$dir"); name=$$(basename "$$wheel_dir"); \
		total=$$((total + 1)); \
		echo "  verifying $$name..."; \
		result=$$(cbmc "$$f" "$$wheel_dir/$$name.c" -I "$$wheel_dir" -I "tooling/harness" --unwind 51 --bounds-check --pointer-check 2>&1 | tail -1); \
		echo "    $$result"; \
		echo "$$result" | grep -q "SUCCESSFUL" || fail=$$((fail + 1)); \
	done; \
	echo ""; echo "$$total wheels, $$fail failures"; \
	[ $$fail -eq 0 ]

frama-c:
	@echo "Frama-C WP verification (requires frama-c in PATH or opam)..."
	@if command -v frama-c >/dev/null 2>&1; then \
		bash tooling/audit/run_frama_c.sh; \
	elif [ -x "$$HOME/.opam/default/bin/frama-c" ]; then \
		export PATH="$$HOME/.opam/default/bin:$$PATH"; \
		bash tooling/audit/run_frama_c.sh; \
	else \
		echo "frama-c not found — install via opam or add to PATH"; exit 1; \
	fi

fuzz:
	bash tooling/test/run_libfuzzer.sh

new-wheel:
	bash tooling/wheel/new_wheel.sh

promote-wheel:
	bash tooling/wheel/wheel_promote.sh

release:
	bash tooling/wheel/release.sh

audit-deep:
	bash tooling/audit/audit_deep.sh

memory-audit:
	python3 tooling/audit/memory_audit.py

doc-drift:
	python3 tooling/audit/doc_drift_check.py --fix

check-clones:
	python3 tooling/audit/check_clones.py

audit-quick:
	bash tooling/audit/audit_deep.sh --quick

audit-compile:
	python3 tooling/audit/audit_compile.py

audit-sanitizer:
	python3 tooling/audit/audit_sanitizer.py

audit-coverage:
	python3 tooling/audit/audit_coverage.py

arm-size-audit:
	bash tooling/audit/arm_size_audit.sh

arm-stack-audit:
	bash tooling/audit/arm_stack_audit.sh

check-readme-examples:
	bash tooling/audit/check_readme_examples.sh

check-complexity:
	python3 tooling/audit/check_complexity.py

check-clones:
	python3 tooling/audit/check_clones.py

find-refs:
	@test -n "$(W)" || { echo "Usage: make find-refs W=<wheel-name>"; exit 1; }
	bash tooling/wheel/find_all_refs.sh "$(W)"

dispatch:
	bash tooling/session/dispatch_agent.sh $(ARGS)

batch-scaffold:
	@test -n "$(SPECS)" || { echo "Usage: make batch-scaffold SPECS=\"w5500:drivers:spi aht20:drivers:i2c:0x38 soft_spi:primitives\""; echo "Format: name:layer[:hal[:addr]]"; exit 1; }
	@count=0; created=""; \
	for spec in $(SPECS); do \
		name=$$(echo "$$spec" | cut -d: -f1); \
		layer=$$(echo "$$spec" | cut -d: -f2); \
		hal=$$(echo "$$spec" | cut -d: -f3); \
		addr=$$(echo "$$spec" | cut -d: -f4); \
		args="--layer $$layer --name $$name"; \
		[ -n "$$hal" ] && args="$$args --hal $$hal"; \
		[ -n "$$addr" ] && args="$$args --addr $$addr"; \
		if [ -d "wheels/$$layer/$$name" ]; then \
			echo "  SKIP  $$name (already exists)"; \
		else \
			bash tooling/wheel/new_wheel.sh $$args > /dev/null 2>&1 && \
			echo "  OK    $$name ($$layer)" && \
			created="$$created wheels/$$layer/$$name/" && \
			count=$$((count + 1)); \
		fi; \
	done; \
	if [ $$count -gt 0 ]; then \
		git add $$created && \
		msg="wheel: scaffold $$count wheels for batch build"; \
		[ $$count -eq 1 ] && msg="wheel: scaffold 1 wheel for batch build"; \
		git commit -m "$$msg" && \
		echo "" && echo "$$count scaffolds committed. Ready for agent dispatch."; \
	else \
		echo "No new scaffolds created."; \
	fi
