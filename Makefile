# ai-embedded-platform — convenience targets
#
# Usage:
#   make test           # all tests (wheels + projects)
#   make test-wheels    # wheel tests only
#   make test-changed   # only tests affected by uncommitted changes
#   make audit          # run wheel audit (14 checks)
#   make coverage       # tests with gcov coverage report
#   make bootstrap      # print session context
#   make list           # list all discoverable test targets

.PHONY: test test-wheels test-projects test-changed audit coverage bootstrap list

test:
	bash tooling/scripts/test_all.sh

test-wheels:
	bash tooling/scripts/test_all.sh --wheel

test-projects:
	bash tooling/scripts/test_all.sh --project

test-changed:
	bash tooling/scripts/test_all.sh --changed

audit:
	python tooling/scripts/audit_wheels.py

coverage:
	bash tooling/scripts/test_all.sh --wheel --coverage

bootstrap:
	bash tooling/scripts/session_bootstrap.sh

list:
	bash tooling/scripts/test_all.sh --list
