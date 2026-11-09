#!/usr/bin/env bash
# stage-local.sh - build the flasher apps locally and stage them into docs/firmware
# so the web flasher can be tested against http://localhost without tagging.
set -euo pipefail

PIO_ENV="${PIO_ENV:-seeed_xiao_esp32c3}"
VERSION="${VERSION:-0.0.0}"
APPS=(boids brick_break caves fluid_cloud full_demo game_of_life
      lava_lamp morph pong snake starfield timer)

BOOT_APP0="$(find "$HOME/.platformio/packages" -name boot_app0.bin -print -quit)"
[ -n "${BOOT_APP0}" ] || { echo "boot_app0.bin not found; run 'pio run' once first"; exit 1; }

rm -rf "docs/firmware/${VERSION}"

for APP in "${APPS[@]}"; do
  echo "==> building ${APP}"
  ( cd "examples/${APP}" && pio run -e "${PIO_ENV}" )

  BUILD="examples/${APP}/.pio/build/${PIO_ENV}"
  OUT="docs/firmware/${VERSION}/${APP}"
  mkdir -p "${OUT}"
  cp "${BUILD}/bootloader.bin" "${OUT}/bootloader.bin"
  cp "${BUILD}/partitions.bin" "${OUT}/partitions.bin"
  cp "${BUILD}/firmware.bin"   "${OUT}/firmware.bin"
  cp "${BOOT_APP0}"            "${OUT}/boot_app0.bin"

  cat > "${OUT}/manifest.json" <<EOF
{
  "name": "device32 - ${APP}",
  "version": "${VERSION}",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-C3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin",  "offset": 57344 },
        { "path": "firmware.bin",   "offset": 65536 }
      ]
    }
  ]
}
EOF
done

APPS_JSON="$(printf '%s\n' "${APPS[@]}" | sort | jq -R -s -c 'split("\n") | map(select(length > 0))')"
jq -n --arg tag "${VERSION}" --argjson apps "${APPS_JSON}" \
  '{ releases: [ { tag: $tag, published_at: null, html_url: null, apps: $apps } ] }' \
  > docs/firmware/index.json

echo "Staged ${#APPS[@]} apps as release ${VERSION}."
echo "Now run:  python -m http.server 8000 --directory docs"
echo "Then open: http://localhost:8000"
