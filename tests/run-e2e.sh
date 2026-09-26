#!/usr/bin/env bash
# E2E tests for mwaeckerlin/fake-smtp: start the server, run the Python client
# in the same network, clean up.
# Usage: bash tests/run-e2e.sh
set -uo pipefail

cd "$(dirname "$0")/.."
COMPOSE="tests/docker-compose.yml"

cleanup() {
    docker compose -f "$COMPOSE" --profile tester down -v --remove-orphans > /dev/null 2>&1 || true
}
trap cleanup EXIT

cleanup
docker compose -f "$COMPOSE" up -d --build fake-smtp > /dev/null 2>&1
EXIT=0
docker compose -f "$COMPOSE" run --rm tester || EXIT=$?
if [[ ${EXIT} -ne 0 ]]; then
    echo "==> fake-smtp log:"
    docker compose -f "$COMPOSE" logs fake-smtp 2>&1
fi
exit ${EXIT}
