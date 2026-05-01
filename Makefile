# ai-embedded-platform — convenience targets
#
# Run `make help` to see all available targets.

.PHONY: test test-wheels test-projects test-changed test-fast test-parallel audit audit-fix audit-fix-apply coverage bootstrap list graph downstream sync-vsf sync-vsf-status clean check ship inventory metrics metrics-save metrics-compare metrics-quality test-scaffold handoff new-wheel promote-wheel release arm-size-audit find-refs help

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
	@echo "  coverage          tests with gcov coverage report"
	@echo "  downstream W=x    test downstream consumers of wheel x"
	@echo ""
	@echo "Audit:"
	@echo "  audit             run wheel audit"
	@echo "  audit-fix         auto-fix mechanical findings (dry-run)"
	@echo "  audit-fix-apply   auto-fix and apply"
	@echo "  arm-size-audit    ARM cross-compile size report"
	@echo ""
	@echo "Workflow:"
	@echo "  check             test + audit + doc drift (pre-push)"
	@echo "  ship M=\"msg\"      check + commit + push"
	@echo "  new-wheel         scaffold a new wheel (interactive)"
	@echo "  promote-wheel     promote wheel across layers"
	@echo "  release           create version tag + changelog"
	@echo "  find-refs W=x     find all references to wheel x"
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

test:
	bash tooling/scripts/test_all.sh

test-wheels:
	bash tooling/scripts/test_all.sh --wheel

test-projects:
	bash tooling/scripts/test_all.sh --project

test-changed:
	bash tooling/scripts/test_all.sh --changed

test-fast:
	bash tooling/scripts/test_all.sh --wheel --fail-fast

test-parallel:
	bash tooling/scripts/test_all.sh --parallel

audit:
	python3 tooling/scripts/audit_wheels.py

audit-fix:
	python3 tooling/scripts/audit_fix.py

audit-fix-apply:
	python3 tooling/scripts/audit_fix.py --apply

coverage:
	bash tooling/scripts/test_all.sh --wheel --coverage

bootstrap:
	bash tooling/scripts/session_bootstrap.sh

list:
	bash tooling/scripts/test_all.sh --list

graph:
	python3 tooling/scripts/audit_wheels.py --graph

downstream:
	@test -n "$(W)" || { echo "Usage: make downstream W=<wheel-name>"; exit 1; }
	bash tooling/scripts/test_downstream.sh "$(W)"

sync-vsf:
	bash tooling/scripts/sync_vsf.sh

sync-vsf-status:
	bash tooling/scripts/sync_vsf.sh --status

clean:
	@echo "Removing build_* directories under wheels/ and projects/..."
	@find wheels/ -type d -name "build_*" -exec rm -rf {} + 2>/dev/null || true
	@find projects/ -type d -name "build_*" -exec rm -rf {} + 2>/dev/null || true
	@find _eval/ -type d -name "build" -exec rm -rf {} + 2>/dev/null || true
	@echo "Done."

check:
	bash tooling/scripts/test_all.sh && python3 tooling/scripts/audit_wheels.py --quiet && python3 tooling/scripts/doc_drift_check.py

ship:
	@test -n "$(M)" || { echo "Usage: make ship M=\"commit message\""; exit 1; }
	bash tooling/scripts/test_all.sh
	python3 tooling/scripts/audit_wheels.py --quiet
	git add -A
	git commit -m "$(M)"
	git push

inventory:
	bash tooling/scripts/update_wheel_inventory.sh --apply

metrics:
	bash tooling/scripts/platform_metrics.sh

metrics-save:
	bash tooling/scripts/platform_metrics.sh --save

metrics-compare:
	bash tooling/scripts/platform_metrics.sh --compare

metrics-quality:
	bash tooling/scripts/platform_metrics.sh --quality

handoff:
	bash tooling/scripts/session_handoff.sh

test-scaffold:
	bash tooling/scripts/test_scaffold.sh

new-wheel:
	bash tooling/scripts/new_wheel.sh

promote-wheel:
	bash tooling/scripts/wheel_promote.sh

release:
	bash tooling/scripts/release.sh

arm-size-audit:
	bash tooling/scripts/arm_size_audit.sh

find-refs:
	@test -n "$(W)" || { echo "Usage: make find-refs W=<wheel-name>"; exit 1; }
	bash tooling/scripts/find_all_refs.sh "$(W)"
