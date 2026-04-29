# ai-embedded-platform — convenience targets
#
# Usage:
#   make test           # all tests (wheels + projects)
#   make test-wheels    # wheel tests only
#   make test-projects  # project tests only
#   make test-changed   # only tests affected by uncommitted changes
#   make test-fast      # stop on first failure (fast iteration)
#   make audit          # run wheel audit (15 checks)
#   make coverage       # tests with gcov coverage report
#   make bootstrap      # print session context
#   make list           # list all discoverable test targets
#   make graph          # wheel dependency graph (Mermaid)
#   make downstream W=<wheel>  # test downstream consumers of a wheel

.PHONY: test test-wheels test-projects test-changed test-fast audit coverage bootstrap list graph downstream

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
	python tooling/scripts/audit_wheels.py

coverage:
	bash tooling/scripts/test_all.sh --wheel --coverage

bootstrap:
	bash tooling/scripts/session_bootstrap.sh

list:
	bash tooling/scripts/test_all.sh --list

graph:
	python tooling/scripts/audit_wheels.py --graph

downstream:
	@test -n "$(W)" || { echo "Usage: make downstream W=<wheel-name>"; exit 1; }
	bash tooling/scripts/test_downstream.sh $(W)
