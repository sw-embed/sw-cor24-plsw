#!/usr/bin/env bash
#
# vendor-fetch.sh -- materialize vendored toolchain binaries from
# the manifests committed under vendor/<tool>/<version>/version.json.
#
# Two modes:
#
#   ./scripts/vendor-fetch.sh                  -- verify mode (default)
#   ./scripts/vendor-fetch.sh --record <tool>  -- first-time pin a SHA-256
#
# Verify mode is what runs on every fresh clone or after pulling
# new commits. For each tool listed in vendor/active.env, it makes
# sure the binary at vendor/<tool>/<version>/bin/<tool> exists and
# matches the SHA-256 in version.json. If the binary is missing or
# stale, it rebuilds at the recorded commit and re-verifies.
#
# Record mode is for first-time vendoring of a new tool/version,
# before any SHA-256 has been pinned. It rebuilds at the recorded
# commit, then *writes* the resulting SHA-256 into version.json.
#
# Critical guarantee: builds always happen at the commit recorded
# in version.json, never at upstream HEAD. This is what makes
# rollback and controlled adoption possible.
#
# Dependencies: bash 4+, jq, git, sha256sum (or shasum -a 256).
#
# See docs/vendor-plan.md for the broader design.

set -euo pipefail

# --- Locate ourselves and the repo root --------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
VENDOR_DIR="$REPO_ROOT/vendor"
ACTIVE_ENV="$VENDOR_DIR/active.env"

cd "$REPO_ROOT"

# --- Argument parsing --------------------------------------------------------

MODE="verify"
RECORD_TOOL=""
FORCE_RECORD=0

while [ $# -gt 0 ]; do
    case "$1" in
        --record)
            MODE="record"
            RECORD_TOOL="${2:-}"
            if [ -z "$RECORD_TOOL" ]; then
                echo "error: --record requires a tool name (e.g. --record sw-cx24)" >&2
                exit 2
            fi
            shift 2
            ;;
        --force)
            FORCE_RECORD=1
            shift
            ;;
        -h|--help)
            sed -n '2,30p' "$0" | sed 's/^# \?//'
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

# --- Dependencies ------------------------------------------------------------

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: required command not found: $1" >&2
        echo "       install it before running vendor-fetch.sh" >&2
        exit 3
    fi
}

require_cmd jq
require_cmd git

# Pick a sha256 implementation
if command -v sha256sum >/dev/null 2>&1; then
    sha256_of() { sha256sum "$1" | awk '{print $1}'; }
elif command -v shasum >/dev/null 2>&1; then
    sha256_of() { shasum -a 256 "$1" | awk '{print $1}'; }
else
    echo "error: neither sha256sum nor shasum found" >&2
    exit 3
fi

# --- Platform detection (matches version.json platforms.<platform>) ---------

detect_platform() {
    local os arch
    case "$(uname -s)" in
        Darwin)  os="darwin" ;;
        Linux)   os="linux" ;;
        *) echo "unsupported-$(uname -s)" ; return ;;
    esac
    case "$(uname -m)" in
        arm64|aarch64) arch="arm64" ;;
        x86_64|amd64)  arch="x86_64" ;;
        *) echo "unsupported-$(uname -m)" ; return ;;
    esac
    echo "${os}-${arch}"
}

PLATFORM="$(detect_platform)"
echo "vendor-fetch: platform=$PLATFORM mode=$MODE"

# --- Load active.env so we know which versions to fetch ---------------------

if [ ! -f "$ACTIVE_ENV" ]; then
    echo "error: $ACTIVE_ENV not found" >&2
    exit 4
fi

# shellcheck source=/dev/null
. "$ACTIVE_ENV"

# Build the (tool, version) list. The mapping below is the only
# place where tool names live; adding a new tool means adding one
# line here plus a new SW_<TOOL>_VERSION variable in active.env.
TOOLS=(
    "sw-cx24:${SW_CX24_VERSION:-}"
    "sw-asx24:${SW_ASX24_VERSION:-}"
    "sw-em24:${SW_EM24_VERSION:-}"
)

# --- Helpers -----------------------------------------------------------------

manifest_path() {
    # $1=tool $2=version
    echo "$VENDOR_DIR/$1/$2/version.json"
}

manifest_get() {
    # $1=manifest path $2=jq filter (e.g. .commit)
    jq -r "$2" "$1"
}

resolve_upstream_repo() {
    # Echoes a path to a working upstream checkout. Tries the
    # repo_path_local first; falls back to a temp clone of `repo`.
    # $1=tool $2=manifest_path
    local tool manifest local_path repo
    tool="$1"
    manifest="$2"
    local_path="$(manifest_get "$manifest" .repo_path_local)"
    repo="$(manifest_get "$manifest" .repo)"

    if [ "$local_path" != "null" ] && [ "$local_path" != "TBD" ]; then
        local abs="$REPO_ROOT/$local_path"
        if [ -d "$abs/.git" ]; then
            echo "$abs"
            return 0
        fi
    fi

    if [ "$repo" = "null" ] || [ "$repo" = "TBD" ]; then
        echo "error: $tool: no working repo_path_local and no canonical 'repo' URL set in $manifest" >&2
        return 4
    fi

    local tmp
    tmp="$(mktemp -d -t "vendor-fetch-${tool}.XXXXXX")"
    echo "vendor-fetch: $tool: cloning $repo into $tmp" >&2
    git clone --quiet "$repo" "$tmp" >&2
    echo "$tmp"
}

build_at_commit() {
    # Checkout the recorded commit in the upstream working tree
    # and run the build command. Echoes the absolute path to the
    # produced binary.
    # $1=tool $2=manifest $3=upstream_dir
    local tool manifest upstream commit build_cmd binary_src
    tool="$1"
    manifest="$2"
    upstream="$3"
    commit="$(manifest_get "$manifest" .commit)"
    build_cmd="$(manifest_get "$manifest" .build_cmd)"
    binary_src="$(manifest_get "$manifest" .binary_src)"

    if [ "$commit" = "null" ] || [ "$commit" = "TBD" ]; then
        echo "error: $tool: manifest has no pinned commit" >&2
        return 5
    fi

    echo "vendor-fetch: $tool: checking out $commit in $upstream" >&2
    (
        cd "$upstream"
        # Stash any local mods so we don't clobber the user's
        # in-progress work.
        git stash push --quiet --include-untracked --message "vendor-fetch.sh autostash" >/dev/null 2>&1 || true
        git -c advice.detachedHead=false checkout --quiet "$commit"
        echo "vendor-fetch: $tool: building with: $build_cmd" >&2
        eval "$build_cmd" >&2
    )

    echo "$upstream/$binary_src"
}

copy_aux_files() {
    # Copy headers_src and docs_src from upstream into the
    # versioned vendor dir. These are tracked.
    # $1=manifest $2=upstream $3=vendor_versioned_dir
    local manifest upstream dest headers_src docs_src
    manifest="$1"
    upstream="$2"
    dest="$3"
    headers_src="$(manifest_get "$manifest" .headers_src)"
    docs_src="$(manifest_get "$manifest" .docs_src)"

    if [ "$headers_src" != "null" ] && [ "$headers_src" != "TBD" ] \
       && [ -d "$upstream/$headers_src" ]; then
        rm -rf "$dest/includes"
        mkdir -p "$dest/includes"
        cp -R "$upstream/$headers_src/." "$dest/includes/"
    fi

    if [ "$docs_src" != "null" ] && [ "$docs_src" != "TBD" ] \
       && [ -d "$upstream/$docs_src" ]; then
        rm -rf "$dest/docs"
        mkdir -p "$dest/docs"
        cp -R "$upstream/$docs_src/." "$dest/docs/"
    fi
}

write_recorded_sha() {
    # Update version.json with sha256, size, recorded_at,
    # recorded_by for the current platform.
    # $1=manifest $2=sha $3=size
    local manifest sha size now tmp
    manifest="$1"
    sha="$2"
    size="$3"
    now="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    tmp="$(mktemp)"
    jq \
        --arg platform "$PLATFORM" \
        --arg sha "$sha" \
        --argjson size "$size" \
        --arg now "$now" \
        '.platforms[$platform] = {sha256: $sha, size: $size}
         | .recorded_at = $now
         | .recorded_by = "vendor-fetch.sh --record"' \
        "$manifest" > "$tmp"
    mv "$tmp" "$manifest"
}

# --- Per-tool fetch ---------------------------------------------------------

fetch_tool() {
    # $1=tool $2=version
    local tool version manifest dest_dir bin_path expected_sha actual_sha size upstream produced
    tool="$1"
    version="$2"

    if [ -z "$version" ]; then
        echo "vendor-fetch: $tool: no version pinned in active.env, skipping" >&2
        return 0
    fi

    manifest="$(manifest_path "$tool" "$version")"
    if [ ! -f "$manifest" ]; then
        echo "error: $tool: manifest not found at $manifest" >&2
        return 6
    fi

    dest_dir="$VENDOR_DIR/$tool/$version"
    bin_path="$dest_dir/bin/$tool"
    expected_sha="$(manifest_get "$manifest" ".platforms[\"$PLATFORM\"].sha256 // \"\"")"

    case "$MODE" in
    verify)
        if [ -z "$expected_sha" ] || [ "$expected_sha" = "TBD" ]; then
            echo "vendor-fetch: $tool $version: no recorded SHA for $PLATFORM"
            echo "             run: ./scripts/vendor-fetch.sh --record $tool"
            return 0
        fi

        # Fast path: binary already present and good.
        if [ -f "$bin_path" ]; then
            actual_sha="$(sha256_of "$bin_path")"
            if [ "$actual_sha" = "$expected_sha" ]; then
                echo "vendor-fetch: $tool $version: ok ($expected_sha)"
                return 0
            fi
            echo "vendor-fetch: $tool $version: SHA mismatch, rebuilding"
            echo "             expected: $expected_sha"
            echo "             actual:   $actual_sha"
            rm -f "$bin_path"
        else
            echo "vendor-fetch: $tool $version: no binary, building"
        fi

        upstream="$(resolve_upstream_repo "$tool" "$manifest")"
        produced="$(build_at_commit "$tool" "$manifest" "$upstream")"
        mkdir -p "$dest_dir/bin"
        cp "$produced" "$bin_path"
        chmod +x "$bin_path"
        copy_aux_files "$manifest" "$upstream" "$dest_dir"

        actual_sha="$(sha256_of "$bin_path")"
        if [ "$actual_sha" != "$expected_sha" ]; then
            echo "error: $tool $version: build produced wrong SHA on $PLATFORM" >&2
            echo "       expected: $expected_sha" >&2
            echo "       actual:   $actual_sha" >&2
            echo "       investigate: was the build deterministic? is the manifest stale?" >&2
            return 7
        fi
        echo "vendor-fetch: $tool $version: ok ($expected_sha)"
        ;;

    record)
        # Only act on the requested tool.
        if [ "$tool" != "$RECORD_TOOL" ]; then
            return 0
        fi

        if [ -n "$expected_sha" ] && [ "$expected_sha" != "TBD" ] && [ "$FORCE_RECORD" -ne 1 ]; then
            echo "error: $tool $version: SHA already recorded for $PLATFORM ($expected_sha)" >&2
            echo "       use --force to overwrite, or bump to a new version directory" >&2
            return 8
        fi

        upstream="$(resolve_upstream_repo "$tool" "$manifest")"
        produced="$(build_at_commit "$tool" "$manifest" "$upstream")"
        mkdir -p "$dest_dir/bin"
        cp "$produced" "$bin_path"
        chmod +x "$bin_path"
        copy_aux_files "$manifest" "$upstream" "$dest_dir"

        actual_sha="$(sha256_of "$bin_path")"
        size="$(wc -c < "$bin_path" | tr -d ' ')"
        write_recorded_sha "$manifest" "$actual_sha" "$size"
        echo "vendor-fetch: $tool $version: recorded $actual_sha ($size bytes) for $PLATFORM"
        echo "             review: git diff $manifest"
        ;;
    esac
}

# --- Main loop --------------------------------------------------------------

found_record_target=0
for entry in "${TOOLS[@]}"; do
    tool="${entry%%:*}"
    version="${entry#*:}"
    if [ "$MODE" = "record" ] && [ "$tool" = "$RECORD_TOOL" ]; then
        found_record_target=1
    fi
    fetch_tool "$tool" "$version"
done

if [ "$MODE" = "record" ] && [ "$found_record_target" -eq 0 ]; then
    echo "error: --record target '$RECORD_TOOL' is not in the TOOLS list" >&2
    exit 9
fi
