const FIRMWARE_INDEX_URL = 'firmware/index.json';
const REPOSITORY_URL = 'https://github.com/unixvoid/device32';
const RELEASES_URL = `${REPOSITORY_URL}/releases`;

// Keep in sync with the matrix in .github/workflows/build.yml (minus the
// intentionally unpublished weather / wifi_scanner examples).
const APP_META = {
  full_demo:    { label: 'Full Demo (shipped)', hint: 'The build that ships on device32. Cycles through every animation and game; tap the button to advance, hold 3s to toggle auto-play.' },
  boids:        { label: 'Boids',        hint: 'Flocking simulation.' },
  brick_break:  { label: 'Brick Break',  hint: 'Breakout clone driven by the button.' },
  caves:        { label: 'Caves',        hint: 'Procedural cave generation.' },
  fluid_cloud:  { label: 'Fluid Cloud',  hint: 'Fluid-like particle cloud.' },
  game_of_life: { label: 'Game of Life', hint: "Conway's Game of Life." },
  lava_lamp:    { label: 'Lava Lamp',    hint: 'Metaball lava lamp effect.' },
  morph:        { label: 'Morph',        hint: 'Shape morphing animation.' },
  pong:         { label: 'Pong',         hint: 'Pong, played with the button.' },
  snake:        { label: 'Snake',        hint: 'Snake, played with the button.' },
  starfield:    { label: 'Starfield',    hint: 'Warp-speed starfield.' },
  timer:        { label: 'Timer',        hint: 'Countdown timer with button input.' },
};

const installButton = document.querySelector('#espInstall');
const releaseSelect = document.querySelector('#releaseSelect');
const appSelect = document.querySelector('#appSelect');
const flashButton = document.querySelector('#flashButton');
const releaseLink = document.querySelector('#releaseLink');
const appHint = document.querySelector('#appHint');

let releases = [];

function semverParts(tag) {
  const match = String(tag).match(/^v?(\d+)\.(\d+)\.(\d+)(?:[-+].*)?$/);
  return match ? [Number(match[1]), Number(match[2]), Number(match[3])] : null;
}

// Newest first: semver when both tags are semver, otherwise by publish date.
function compareDesc(a, b) {
  const pa = semverParts(a.tag);
  const pb = semverParts(b.tag);
  if (pa && pb) {
    for (let i = 0; i < 3; i += 1) {
      if (pb[i] !== pa[i]) return pb[i] - pa[i];
    }
  }
  return new Date(b.published_at || 0) - new Date(a.published_at || 0);
}

function releasedOn(release) {
  if (!release.published_at) return release.tag;
  return `${release.tag}  /  ${new Date(release.published_at).toLocaleDateString()}`;
}

function manifestUrl(release, app) {
  return `firmware/${encodeURIComponent(release.tag)}/${encodeURIComponent(app)}/manifest.json`;
}

function currentRelease() {
  return releases[Number(releaseSelect.value)] || null;
}

function refreshApps() {
  const release = currentRelease();
  if (!release) {
    appSelect.replaceChildren(new Option('Pick a release first'));
    appSelect.disabled = true;
    refreshFlasher();
    return;
  }

  const apps = release.apps.slice().sort((a, b) => {
    if (a === 'full_demo') return -1;
    if (b === 'full_demo') return 1;
    return a.localeCompare(b);
  });

  appSelect.replaceChildren(...apps.map((id) => {
    const option = new Option((APP_META[id] && APP_META[id].label) || id, id);
    return option;
  }));
  appSelect.disabled = apps.length === 0;
  refreshFlasher();
}

function refreshFlasher() {
  const release = currentRelease();
  const app = appSelect.value;

  if (!release || !app) {
    flashButton.disabled = true;
    appHint.textContent = '';
    return;
  }

  installButton.manifest = manifestUrl(release, app);
  releaseLink.href = release.html_url || RELEASES_URL;
  flashButton.disabled = false;

  const meta = APP_META[app];
  const label = (meta && meta.label) || app;
  appHint.textContent = meta && meta.hint
    ? `${label} \u2014 ${meta.hint}`
    : label;
}

async function loadIndex() {
  try {
    const response = await fetch(FIRMWARE_INDEX_URL, { cache: 'no-cache' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);

    const index = await response.json();
    releases = (index.releases || [])
      .filter((release) => Array.isArray(release.apps) && release.apps.length)
      .sort(compareDesc);

    if (!releases.length) {
      throw new Error('No firmware releases have been published yet');
    }

    releaseSelect.replaceChildren(...releases.map((release, i) => new Option(releasedOn(release), i)));
    releaseSelect.disabled = false;
    refreshApps();
  } catch (error) {
    releaseSelect.replaceChildren(new Option(`Unable to load firmware (${error.message})`));
    releaseSelect.disabled = true;
    appSelect.replaceChildren(new Option('Unavailable'));
    appSelect.disabled = true;
    flashButton.disabled = true;
  }
}

releaseSelect.addEventListener('change', refreshApps);
appSelect.addEventListener('change', refreshFlasher);

loadIndex();
