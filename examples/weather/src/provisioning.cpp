#include "provisioning.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "config.h"

// ---------------------------------------------------------------------------
// SoftAP captive-portal onboarding.
//
// Flow:
//   1. No credentials (or the saved AP is unreachable) -> raise "device32-..."
//      SoftAP, answer every DNS query with our own IP, serve a one-page form.
//   2. The form POSTs the SSID/password, which is written to NVS.
//   3. The portal tears itself down and the device joins the new network.
//
// Only NVS is touched for credentials; nothing is baked into the binary.
// ---------------------------------------------------------------------------

static WiFiUDP s_dnsUdp;
static WebServer s_server(80);
static bool s_routesRegistered = false;
static volatile bool s_credsSubmitted = false;

// The SoftAP always runs on this address; it is also the address returned for
// every DNS query.
static IPAddress s_apIP(WIFI_AP_IP);

// Nearby networks are scanned ONCE, before the SoftAP is raised, and cached.
// Scanning while the AP is up forces the radio off the AP channel and drops the
// client that is trying to load the portal page, so we never scan during setup.
struct ScannedNetwork {
  String ssid;
  int32_t rssi;
  bool secure;
};
static const int kMaxScannedNetworks = 32;
static ScannedNetwork s_scanned[kMaxScannedNetworks];
static int s_scannedCount = 0;
static bool s_wifiEventHookInstalled = false;

// ---------------------------------------------------------------------------
// Credential storage (NVS)
// ---------------------------------------------------------------------------

static bool loadCredentials(String &ssid, String &pass) {
  Preferences prefs;
  prefs.begin(WIFI_NVS_NAMESPACE, true);
  ssid = prefs.getString(WIFI_NVS_SSID_KEY, "");
  pass = prefs.getString(WIFI_NVS_PASS_KEY, "");
  prefs.end();
  return ssid.length() > 0;
}

static void saveCredentials(const String &ssid, const String &pass) {
  Preferences prefs;
  prefs.begin(WIFI_NVS_NAMESPACE, false);
  prefs.putString(WIFI_NVS_SSID_KEY, ssid);
  prefs.putString(WIFI_NVS_PASS_KEY, pass);
  prefs.end();
}

bool wifiHasCredentials() {
  Preferences prefs;
  prefs.begin(WIFI_NVS_NAMESPACE, true);
  String ssid = prefs.getString(WIFI_NVS_SSID_KEY, "");
  prefs.end();
  return ssid.length() > 0;
}

void wifiClearCredentials() {
  Preferences prefs;
  prefs.begin(WIFI_NVS_NAMESPACE, false);
  prefs.remove(WIFI_NVS_SSID_KEY);
  prefs.remove(WIFI_NVS_PASS_KEY);
  prefs.end();
}

// Logs AP lifecycle events so the serial console shows whether the radio came
// up and whether the client actually received a DHCP lease.
static void logWifiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_START:
      Serial.println("[wifi] AP started");
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.println("[wifi] client joined the AP");
      break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      Serial.println("[wifi] client received an IP (DHCP OK)");
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.println("[wifi] client left the AP");
      break;
    default:
      break;
  }
}

// One blocking scan in station mode, performed before the portal AP exists.
// Returns nothing; results land in s_scanned[]/s_scannedCount.
static void scanNetworksForPortal() {
  s_scannedCount = 0;

  WiFi.mode(WIFI_STA);
  delay(100);

  // A scan issued immediately after the mode change can come back empty, which
  // would leave the user with an empty dropdown. Retry once before giving up.
  int found = WiFi.scanNetworks();
  if (found <= 0) {
    WiFi.scanDelete();
    delay(400);
    found = WiFi.scanNetworks();
  }

  for (int i = 0; i < found && s_scannedCount < kMaxScannedNetworks; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;

    const int32_t rssi = WiFi.RSSI(i);
    const bool secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

    // Keep the strongest reading per SSID (mesh networks repeat the name).
    bool merged = false;
    for (int j = 0; j < s_scannedCount; j++) {
      if (s_scanned[j].ssid == ssid) {
        if (rssi > s_scanned[j].rssi) {
          s_scanned[j].rssi = rssi;
          s_scanned[j].secure = secure;
        }
        merged = true;
        break;
      }
    }
    if (merged) continue;

    s_scanned[s_scannedCount].ssid = ssid;
    s_scanned[s_scannedCount].rssi = rssi;
    s_scanned[s_scannedCount].secure = secure;
    s_scannedCount++;
  }

  WiFi.scanDelete();
  Serial.printf("[wifi] pre-scan found %d network(s)\n", s_scannedCount);
}

// ---------------------------------------------------------------------------
// DNS responder (captive portal)
//
// The bundled DNSServer only answers packets where ARCount == 0. Windows and
// several resolvers append an EDNS0 OPT record to every query, so they would
// get NXDOMAIN and never reach the portal. We therefore answer DNS ourselves:
// any query gets an A record pointing at the SoftAP address, and any additional
// records are dropped from the reply.
// ---------------------------------------------------------------------------

// BEGIN_DNS_BUILD - self-contained and host-testable (no Arduino types).
static int buildDnsResponse(const uint8_t *req, int len, uint8_t *out, int outCap,
                            uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3) {
  if (len < 12 || outCap < 12) return 0;

  const uint8_t flags = req[2];
  if (flags & 0x80) return 0;         // QR set: already a response
  if ((flags & 0x78) != 0) return 0;  // opcode other than QUERY

  if (((req[4] << 8) | req[5]) != 1) return 0;  // exactly one question

  // Walk past the single QNAME (labels or a compression pointer).
  int qEnd = 12;
  while (qEnd < len) {
    const uint8_t label = req[qEnd];
    if (label == 0) { qEnd += 1; break; }
    if ((label & 0xC0) == 0xC0) { qEnd += 2; break; }
    qEnd += label + 1;
  }
  if (qEnd + 4 > len) return 0;  // need QTYPE + QCLASS
  qEnd += 4;

  const int answerLen = 2 + 2 + 2 + 4 + 2 + 4;  // ptr, type, class, ttl, rdlen, addr
  if (qEnd + answerLen > outCap) return 0;

  memcpy(out, req, qEnd);  // header + question only; drops any OPT record

  out[2] = 0x81;                   // QR = 1, RD = 1
  out[3] = 0x80;                   // RA = 1, RCODE = NOERROR
  out[6] = 0x00; out[7] = 0x01;    // ANCOUNT = 1
  out[8] = 0x00; out[9] = 0x00;    // NSCOUNT = 0
  out[10] = 0x00; out[11] = 0x00;  // ARCOUNT = 0

  int p = qEnd;
  out[p++] = 0xC0; out[p++] = 0x0C;  // name: pointer back to the question
  out[p++] = 0x00; out[p++] = 0x01;  // TYPE  = A
  out[p++] = 0x00; out[p++] = 0x01;  // CLASS = IN
  out[p++] = 0x00; out[p++] = 0x00;  // TTL (high)
  out[p++] = 0x00; out[p++] = 0x3C;  // TTL = 60s
  out[p++] = 0x00; out[p++] = 0x04;  // RDLENGTH = 4
  out[p++] = ip0; out[p++] = ip1; out[p++] = ip2; out[p++] = ip3;
  return p;
}
// END_DNS_BUILD

// Drains and answers one pending DNS query, if any.
static void dnsProcess() {
  if (s_dnsUdp.parsePacket() <= 0) return;

  uint8_t req[512];
  const int len = s_dnsUdp.read(req, sizeof(req));
  if (len <= 0) return;

  uint8_t resp[512];
  const int respLen = buildDnsResponse(req, len, resp, sizeof(resp),
                                       s_apIP[0], s_apIP[1], s_apIP[2], s_apIP[3]);
  if (respLen <= 0) return;

  s_dnsUdp.beginPacket(s_dnsUdp.remoteIP(), s_dnsUdp.remotePort());
  s_dnsUdp.write(resp, respLen);
  s_dnsUdp.endPacket();
}

// ---------------------------------------------------------------------------
// OLED helpers (portrait canvas, 64x128 after rotation 1)
// ---------------------------------------------------------------------------

static void drawCentered(const String &text, int y, int size) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((display.width() - (int)w) / 2, y);
  display.println(text);
}

// Word/segment aware wrap so long SSIDs and dotted IPs stay on screen.
static int drawWrappedText(const String &text, int y, int maxWidth, int lineHeight) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int start = 0;
  const int len = text.length();
  while (start < len) {
    String line = text.substring(start);
    int16_t x1, y1;
    uint16_t w, h;

    display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
    while (line.length() > 1 && (int)w > maxWidth) {
      // Prefer to break after a natural boundary, else drop a character.
      int cut = -1;
      for (int i = line.length() - 1; i > 0; i--) {
        char c = line[i];
        if (c == ' ' || c == '.' || c == '-' || c == '_') { cut = i + 1; break; }
      }
      line = (cut > 0 && cut < (int)line.length())
                 ? line.substring(0, cut)
                 : line.substring(0, line.length() - 1);
      display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
    }

    display.setCursor((maxWidth - (int)w) / 2, y);
    display.println(line);
    y += lineHeight;
    start += line.length();
    while (start < len && text[start] == ' ') start++;
  }
  return y;
}

static void drawConnectingScreen(const String &ssid, int step) {
  const int W = display.width();
  const int H = display.height();
  display.clearDisplay();

  drawCentered("Connecting", H / 2 - 24, 1);

  String dots = "WiFi";
  for (int i = 0; i < (step % 4); i++) dots += ".";
  drawCentered(dots, H / 2 - 8, 1);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  drawWrappedText(ssid, H / 2 + 12, W, 10);

  display.display();
}

static void drawSetupScreen(const IPAddress &ip, int step, bool showError) {
  const int W = display.width();
  const int H = display.height();
  display.clearDisplay();
  display.drawRoundRect(0, 0, W, H, 4, SSD1306_WHITE);

  // Inverted title bar
  display.fillRect(2, 2, W - 4, 11, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(4, 4);
  display.println("WiFi Setup");

  int y = 18;
  display.setTextColor(SSD1306_WHITE);

  if (showError) {
    y = drawWrappedText("Last network failed.", y, W - 6, 10);
    y += 4;
  }

  display.setCursor(4, y);
  display.println("1. Join this AP:");
  y += 11;
  y = drawWrappedText(WIFI_AP_SSID, y, W - 6, 10);

  y += 4;
  display.setCursor(4, y);
  display.println("2. Open a browser");
  display.setCursor(4, y + 10);
  display.println("at:");
  y += 21;
  y = drawWrappedText(ip.toString(), y, W - 6, 10);

  display.display();
}

// ---------------------------------------------------------------------------
// Captive-portal web UI
// ---------------------------------------------------------------------------

static const char PORTAL_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>device32 WiFi setup</title>
<style>
 body{font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;margin:0;background:#07101c;color:#eef4ff}
 .card{max-width:420px;margin:6vh auto;padding:24px;border:1px solid #25354b;border-radius:12px;background:#0c1626}
 h1{font-size:20px;margin:0 0 6px}
 p{color:#8291a8;font-size:13px;line-height:1.6;margin:0 0 18px}
 label{display:block;font-size:11px;text-transform:uppercase;letter-spacing:.08em;color:#8291a8;margin:14px 0 6px}
 input,select,button{width:100%;box-sizing:border-box;height:46px;border-radius:8px;border:1px solid #344962;background:#0b1524;color:#eef4ff;padding:0 12px;font:inherit}
 button{border:0;background:#4080ff;color:#fff;margin-top:22px;cursor:pointer;font-weight:600}
 button:disabled{opacity:.6;cursor:default}
 .row{color:#52647d;font-size:11px;margin-top:14px}
</style>
</head>
<body>
<div class="card" id="card">
  <h1>device32 WiFi setup</h1>
  <p>Choose your network and enter its password. The device saves it and connects automatically.</p>
  <form id="f">
    <label for="ssidSel">Network</label>
    <select id="ssidSel">
      <option value="">Scanning&hellip;</option>
    </select>

    <div id="manualWrap" style="display:none">
      <label for="ssidManual">Network name (SSID)</label>
      <input id="ssidManual" placeholder="Type the network name" autocomplete="off" autocapitalize="off" spellcheck="false">
    </div>

    <label for="pass">Password</label>
    <input id="pass" name="pass" type="password" placeholder="Leave blank for open networks" autocomplete="off">
    <button id="go" type="submit">Save &amp; connect</button>
  </form>
  <div class="row" id="hint">Scanning nearby networks&hellip;</div>
</div>
<script>
const MANUAL='__manual__';
const f=document.getElementById('f'),hint=document.getElementById('hint'),card=document.getElementById('card');
const sel=document.getElementById('ssidSel'),manualWrap=document.getElementById('manualWrap'),manual=document.getElementById('ssidManual');

function addOpt(value,text){const o=document.createElement('option');o.value=value;o.textContent=text;sel.appendChild(o);}
function showManual(on){manualWrap.style.display=on?'block':'none';}
sel.addEventListener('change',()=>showManual(sel.value===MANUAL));

function fill(list){
  sel.innerHTML='';
  list.sort((a,b)=>b.rssi-a.rssi);
  if(!list.length){
    addOpt(MANUAL,'No networks found');
    sel.value=MANUAL;
    showManual(true);
    hint.textContent='Scan found nothing. Type your network name below.';
    return;
  }
  addOpt('','Choose a network ('+list.length+' found)');
  list.forEach(n=>addOpt(n.ssid,n.ssid+(n.secure?'  [locked]':'  [open]')+'   '+n.rssi+' dBm'));
  addOpt(MANUAL,'Other... (type it)');
  sel.value='';
  showManual(false);
  hint.textContent=list.length+' network(s) found - pick one, or choose Other.';
}

fetch('/api/wifi/scan').then(r=>r.json()).then(fill)
  .catch(()=>{fill([]);hint.textContent='Scan unavailable - type your network name below.';});

f.addEventListener('submit',async e=>{
  e.preventDefault();
  const ssid=(sel.value===MANUAL?manual.value:sel.value).trim();
  if(!ssid){hint.textContent='Please choose or type a network name.';return;}
  const btn=document.getElementById('go');btn.disabled=true;hint.textContent='Saving...';
  try{
    const res=await fetch('/api/wifi',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(document.getElementById('pass').value)});
    if(!res.ok) throw new Error('save failed');
    card.innerHTML='<h1>Saved</h1><p>The device is connecting to your network now. You can close this page and watch the display.</p>';
  }catch(_){btn.disabled=false;hint.textContent='Save failed, please try again.';}
});
</script>
</body>
</html>)HTML";

static String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

static void handlePortalPage() {
  Serial.printf("[http] request %s\n", s_server.uri().c_str());
  s_server.sendHeader("Cache-Control", "no-store");
  s_server.send_P(200, "text/html", PORTAL_PAGE);
}

static void handleStatus() {
  String body = String("{\"provisioned\":") + (wifiHasCredentials() ? "true" : "false") +
                ",\"connected\":" + (WiFi.status() == WL_CONNECTED ? "true" : "false") + "}";
  s_server.send(200, "application/json", body);
}

// Serves the pre-scan cache. Deliberately does NOT scan: scanning from AP mode
// makes the radio leave the AP channel and drops the connected client.
static void handleScan() {
  String json = "[";
  for (int i = 0; i < s_scannedCount; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(s_scanned[i].ssid) + "\",\"rssi\":" +
            String(s_scanned[i].rssi) + ",\"secure\":" +
            (s_scanned[i].secure ? "true" : "false") + "}";
  }
  json += "]";
  s_server.send(200, "application/json", json);
}

static void handleSave() {
  if (!s_server.hasArg("ssid")) {
    s_server.send(400, "application/json", "{\"ok\":false,\"error\":\"ssid required\"}");
    return;
  }

  String ssid = s_server.arg("ssid");
  ssid.trim();
  if (ssid.length() == 0) {
    s_server.send(400, "application/json", "{\"ok\":false,\"error\":\"ssid required\"}");
    return;
  }

  saveCredentials(ssid, s_server.arg("pass"));
  Serial.printf("[wifi] credentials saved for \"%s\"\n", ssid.c_str());
  s_server.send(200, "application/json", "{\"ok\":true}");
  s_credsSubmitted = true;
}

static void handleRedirect() {
  Serial.printf("[http] redirect %s\n", s_server.uri().c_str());
  s_server.sendHeader("Location", "/", true);
  s_server.send(302, "text/html", "");
}

static void registerRoutes() {
  if (s_routesRegistered) return;

  s_server.on("/", HTTP_GET, handlePortalPage);
  s_server.on("/wifi", HTTP_GET, handlePortalPage);
  s_server.on("/api/wifi/status", HTTP_GET, handleStatus);
  s_server.on("/api/wifi/scan", HTTP_GET, handleScan);
  s_server.on("/api/wifi", HTTP_POST, handleSave);

  // Captive-portal probes: bounce every OS to the configuration form.
  s_server.on("/generate_204", HTTP_GET, handleRedirect);            // Android
  s_server.on("/gen_204", HTTP_GET, handleRedirect);                 // Android (alt)
  s_server.on("/hotspot-detect.html", HTTP_GET, handlePortalPage);   // Apple / macOS
  s_server.on("/library/test/success.html", HTTP_GET, handleRedirect);
  s_server.on("/ncsi.txt", HTTP_GET, handleRedirect);                // Windows
  s_server.on("/connecttest.txt", HTTP_GET, handleRedirect);         // Windows (alt)
  s_server.on("/canonical.html", HTTP_GET, handlePortalPage);        // Firefox
  s_server.on("/success.txt", HTTP_GET, handleRedirect);             // Firefox (alt)
  s_server.onNotFound(handleRedirect);

  s_routesRegistered = true;
}

// ---------------------------------------------------------------------------
// Connect / portal drivers
// ---------------------------------------------------------------------------

// One association attempt, with the dot animation on the OLED. Never blocks
// longer than timeoutMs so a stale SSID cannot wedge the boot.
static bool wifiConnectSta(const String &ssid, const String &pass, uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  unsigned long start = millis();
  unsigned long lastDraw = 0;
  int step = 0;
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    if (millis() - lastDraw >= 500) {
      drawConnectingScreen(ssid, step++);
      lastDraw = millis();
    }
    delay(50);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] connected to \"%s\" as %s\n", ssid.c_str(),
                  WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.printf("[wifi] could not reach \"%s\" within %ums\n", ssid.c_str(), timeoutMs);
  WiFi.disconnect(true);
  return false;
}

// Runs a single portal session: raises the SoftAP, answers DNS/HTTP, and
// returns once the user has submitted credentials.
static void runPortalSession(bool showError) {
  s_credsSubmitted = false;

  // Collect nearby networks BEFORE the AP exists - see scanNetworksForPortal().
  scanNetworksForPortal();

  // Raise the AP. We do not call WiFi.softAPConfig(): the default SoftAP address
  // is already 192.168.4.1, and softAPConfig() tears the DHCP server down and
  // back up, which can leave clients without a lease.
  WiFi.mode(WIFI_AP);
  const bool apOk = WiFi.softAP(WIFI_AP_SSID, nullptr, PROV_AP_CHANNEL, 0, PROV_AP_MAX_CONN);

  if (!s_wifiEventHookInstalled) {
    WiFi.onEvent(logWifiEvent);
    s_wifiEventHookInstalled = true;
  }

  // The AP netif applies its address asynchronously; wait briefly so the IP we
  // log and show is the real one rather than 0.0.0.0.
  IPAddress reportedIp;
  for (int i = 0; i < 20; i++) {
    reportedIp = WiFi.softAPIP();
    if ((uint32_t)reportedIp != 0) break;
    delay(50);
  }
  Serial.printf("[wifi] softAP=%s reported ip=%s ssid=\"%s\"\n",
                apOk ? "ok" : "FAILED", reportedIp.toString().c_str(), WIFI_AP_SSID);

  s_dnsUdp.begin(53);
  registerRoutes();
  s_server.begin();

  // Trust whatever the stack actually applied, but never show a blank address.
  const IPAddress displayedIp = ((uint32_t)reportedIp == 0) ? s_apIP : reportedIp;
  Serial.printf("[wifi] setup portal ready - join \"%s\", then open http://%s/\n",
                WIFI_AP_SSID, displayedIp.toString().c_str());

  unsigned long lastDraw = 0;
  int step = 0;
  while (!s_credsSubmitted) {
    dnsProcess();
    s_server.handleClient();
    if (millis() - lastDraw >= 500) {
      drawSetupScreen(displayedIp, step++, showError);
      lastDraw = millis();
    }
    delay(WIFI_SETUP_POLL_MS);
  }

  s_dnsUdp.stop();
  s_server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  delay(100);
}

void wifiRunSetupPortal() {
  bool showError = false;
  for (;;) {
    runPortalSession(showError);

    String ssid, pass;
    loadCredentials(ssid, pass);
    drawConnectingScreen(ssid, 0);

    if (wifiConnectSta(ssid, pass, WIFI_CONNECT_TIMEOUT_MS)) {
      return;
    }

    // The submitted network did not answer - reopen the portal with a hint
    // rather than leaving the device stranded with no connectivity.
    showError = true;
  }
}

bool wifiConnectOrProvision(uint32_t connectTimeoutMs) {
  String ssid, pass;
  if (loadCredentials(ssid, pass)) {
    drawConnectingScreen(ssid, 0);
    if (wifiConnectSta(ssid, pass, connectTimeoutMs)) {
      return true;
    }
    Serial.println("[wifi] saved network unreachable, entering setup portal");
  } else {
    Serial.println("[wifi] no credentials stored, entering setup portal");
  }

  wifiRunSetupPortal();
  return true;
}
