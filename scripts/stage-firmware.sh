#!/usr/bin/env bash
# stage-firmware.sh - download device32 firmware bundles from GitHub Releases
# into docs/firmware and generate docs/firmware/index.json for the web flasher.
set -euo pipefail

REPO="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY must be set}"
MAX_RELEASES="${MAX_RELEASES:-5}"
EXPECTED_TAG="${EXPECTED_TAG:-}"

SITE_DIR="docs"
DEST="${SITE_DIR}/firmware"

echo "Staging up to ${MAX_RELEASES} release(s) from ${REPO} into ${DEST}"

# A tag-triggered run can race the Releases API by a few seconds.
if [ -n "${EXPECTED_TAG}" ]; then
  for attempt in $(seq 1 12); do
    if gh api "repos/${REPO}/releases/tags/${EXPECTED_TAG}" >/dev/null 2>&1; then
      echo "Release ${EXPECTED_TAG} is available."
      break
    fi
    echo "Waiting for release ${EXPECTED_TAG} (attempt ${attempt}/12)..."
    sleep 10
  done
fi

rm -rf "${DEST}"
mkdir -p "${DEST}"

# tag <TAB> published_at <TAB> html_url <TAB> comma-separated asset names
# tr -d '\r' keeps this working when the script is run from Git Bash on Windows,
# where a Windows jq.exe emits CRLF and the trailing \r breaks the case match.
gh api "repos/${REPO}/releases?per_page=100" \
  | jq -r '.[]
      | select(.draft == false and .prerelease == false)
      | [ .tag_name, .published_at, .html_url, ([.assets[].name] | join(",")) ]
      | @tsv' | tr -d '\r' > /tmp/releases.tsv

index='{"releases":[]}'
count=0

while IFS=$'\t' read -r tag published html assets; do
  [ -n "${tag}" ] || continue
  case ",${assets}," in
    *,device32-*-firmware.zip,*) ;;
    *) continue ;;
  esac
  if [ "${count}" -ge "${MAX_RELEASES}" ]; then break; fi

  echo "::group::Downloading firmware for ${tag}"
  tmp="$(mktemp -d)"
  gh release download "${tag}" --repo "${REPO}" \
    --pattern 'device32-*-firmware.zip' --dir "${tmp}" --clobber
  # The zip contains firmware/<tag>/<app>/..., so unpack at the site root.
  # -o/-q: overwrite silently so a re-run can never block on an interactive prompt.
  unzip -oq "${tmp}"/*.zip -d "${SITE_DIR}"
  rm -rf "${tmp}"
  echo "::endgroup::"

  apps="$(find "${DEST}/${tag}" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' \
    | sort | jq -R -s -c 'split("\n") | map(select(length > 0))')"

  index="$(jq -c \
    --arg tag "${tag}" \
    --arg published "${published}" \
    --arg html "${html}" \
    --argjson apps "${apps}" \
    '.releases += [{ tag: $tag, published_at: $published, html_url: $html, apps: $apps }]' \
    <<<"${index}")"
  count=$((count + 1))
done < /tmp/releases.tsv

echo "${index}" | jq . > "${DEST}/index.json"
echo "Staged $(jq '.releases | length' "${DEST}/index.json") release(s)."
