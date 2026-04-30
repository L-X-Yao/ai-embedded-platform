# ai-embedded-platform — convenience targets
#
# Usage:
#   make test           # all tests (wheels + projects)
#   make test-wheels    # wheel tests only
#   make test-projects  # project tests only
#   make test-changed   # only tests affected by uncommitted changes
#   make test-fast      # stop on first failure (fast iteration)
#   make audit          # run wheel audit (22 checks)
#   make audit-fix      # auto-fix mechanical audit findings (dry-run)
#   make audit-fix-apply  # auto-fix and apply
#   make coverage       # tests with gcov coverage report
#   make bootstrap      # print session context
#   make list           # list all discoverable test targets
#   make graph          # wheel dependency graph (Mermaid)
#   make downstream W=<wheel>  # test downstream consumers of a wheel
#   make sync-vsf              # sync vsf/ with upstream (interactive)
#   make sync-vsf-status       # show current vs upstream vsf version
#   make clean                 # remove all build_* directories
#   make check                 # test + audit in one shot (pre-push)
#   make ship M="msg"          # check + commit + push in one shot
#   make inventory             # update CLAUDE.md wheel counts

.PHONY: test test-wheels test-projects test-changed test-fast audit audit-fix audit-fix-apply coverage bootstrap list graph downstream sync-vsf sync-vsf-status clean check ship inventory metrics metrics-save metrics-compare test-scaffold

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
	bash tooling/scripts/test_all.sh && python3 tooling/scripts/audit_wheels.py --quiet

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

test-scaffold:
	bash tooling/scripts/test_scaffold.sh
