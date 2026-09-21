#!/bin/bash
#
# Start/stop the Smart_Server stack (Mosquitto + FastAPI) via Docker Compose.
#
#   tools/mqtt/broker.sh start|stop|status|logs|ip|api
#
# The compose file lives in the Smart_Server submodule. The broker listens on
# 1883 (and 9001 for MQTT-over-WebSockets); the API on 8000 with interactive
# docs at /docs.

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPOSE_DIR="$(cd "$HERE/../../Smart_Server" && pwd)"

if [ ! -f "$COMPOSE_DIR/docker-compose.yml" ]; then
    echo "Smart_Server submodule not checked out at $COMPOSE_DIR" >&2
    echo "Run: git submodule update --init Smart_Server" >&2
    exit 1
fi

compose() {
    # BUILDX_BUILDER=default is deliberate.
    #
    # The global buildx default on this machine is a `docker-container` driver
    # builder (used by another project). Those run BuildKit inside its own
    # container with its own network path, and image metadata resolution
    # through it times out against registry-1.docker.io:
    #
    #     failed to solve: ... failed to resolve source metadata for
    #     docker.io/library/python:3.11-slim: net/http: TLS handshake timeout
    #
    # The built-in `default` driver talks to the daemon directly and resolves
    # base images from the local image store, which sidesteps it entirely.
    # Override by exporting BUILDX_BUILDER if you need a different one.
    ( cd "$COMPOSE_DIR" && BUILDX_BUILDER="${BUILDX_BUILDER:-default}" docker compose "$@" )
}

# The address the ESP should be pointed at.
#
# A plain `ip route get 1.1.1.1` is wrong under WSL2 mirrored networking: the
# mirrored adapter installs catch-all routes (0.0.0.0/5 and 8.0.0.0/7 via eth0)
# that swallow most public addresses and report the WSL-internal NAT address
# rather than the LAN address the device can actually reach.
lan_ip() {
    local addr
    addr="$(ip -4 -o addr show scope global 2>/dev/null \
            | awk '{print $4}' | cut -d/ -f1 \
            | grep -E '^192\.168\.' | head -1)"
    if [ -z "$addr" ]; then
        addr="$(ip -4 -o addr show scope global 2>/dev/null \
                | awk '{print $4}' | cut -d/ -f1 \
                | grep -vE '^(10\.255\.|172\.1[6-9]\.|172\.2[0-9]\.|172\.3[01]\.)' \
                | head -1)"
    fi
    echo "${addr:-<no LAN address found>}"
}

prepare() {
    # .env and the bind-mount targets must exist before compose will start.
    [ -f "$COMPOSE_DIR/.env" ] || cp "$COMPOSE_DIR/.env.example" "$COMPOSE_DIR/.env"
    mkdir -p "$COMPOSE_DIR"/{logs,models,ota,reports,mosquitto/data,mosquitto/log}
}

case "${1:-}" in
    start)
        prepare
        if ! command -v docker >/dev/null 2>&1; then
            echo "docker not found" >&2
            exit 1
        fi
        compose up -d --build || exit 1
        echo
        compose ps
        echo
        echo "Node should use:  mqtt_set $(lan_ip)"
        echo "API:              http://$(lan_ip):8000/docs"
        ;;
    stop)     compose down ;;
    restart)  compose down && prepare && compose up -d --build ;;
    status)   compose ps ;;
    logs)     compose logs -f --tail=100 ;;
    ip)       lan_ip ;;
    api)      curl -s "http://$(lan_ip):8000/api/devices" | python3 -m json.tool 2>/dev/null \
                  || echo "API not responding on $(lan_ip):8000" ;;
    *)
        echo "usage: $0 {start|stop|restart|status|logs|ip|api}" >&2
        exit 1
        ;;
esac
