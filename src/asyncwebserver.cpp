#include "asyncwebserver.h"
#include "messages.h"
#include "webhandler.h"
#include <ArduinoJson.h>
#ifdef ENABLE_STORAGE
#include <Preferences.h>
#endif

#ifdef ENABLE_SERVER

AsyncWebServer server(80);

void initWebServer()
{
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers",
                                       "Accept, Content-Type, Authorization");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Credentials", "true");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.on("/", HTTP_GET, startGui);
  server.onNotFound(
      [](AsyncWebServerRequest *request) { request->send(404, "text/plain", "Page not found!"); });

  // Route to handle
  // http://your-server/message?text=Hello&repeat=3&id=42&delay=30&graph=1,2,3,4&miny=0&maxy=15
  server.on("/api/message", HTTP_GET, handleMessage);
  server.on("/api/removemessage", HTTP_GET, handleMessageRemove);

  server.on("/api/info", HTTP_GET, handleGetInfo);

  // Handle API request to set an active plugin by ID
  server.on("/api/plugin", HTTP_PATCH, handleSetPlugin);

  // Handle API request to set the brightness (0..255);
  server.on("/api/brightness", HTTP_PATCH, handleSetBrightness);
  server.on("/api/data", HTTP_GET, handleGetData);

  // Scheduler
  server.on("/api/schedule", HTTP_POST, handleSetSchedule);
  server.on("/api/schedule/clear", HTTP_GET, handleClearSchedule);
  server.on("/api/schedule/stop", HTTP_GET, handleStopSchedule);
  server.on("/api/schedule/start", HTTP_GET, handleStartSchedule);

  server.on("/api/storage/clear", HTTP_GET, handleClearStorage);

  // Configuration endpoints
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on(
      "/api/config",
      HTTP_POST,
      [](AsyncWebServerRequest *request) {
        // Response is sent after full body is received.
      },
      nullptr,
      handleSetConfigBody);
  server.on("/api/config/reset", HTTP_POST, handleResetConfig);

  // City Clock config API
  server.on("/api/cityclock", HTTP_GET, [](AsyncWebServerRequest *request) {
#ifdef ENABLE_STORAGE
    Preferences prefs;
    prefs.begin("cityclock", true);
    JsonDocument doc;
    doc["cityIndex"] = prefs.getInt("cityIdx", 0);
    prefs.end();
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
#else
    request->send(200, "application/json", "{\"cityIndex\":0}");
#endif
  });

  // POST /api/cityclock — save city index to NVS (no plugin switch)
  server.on(
      "/api/cityclock", HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
#ifdef ENABLE_STORAGE
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err)
        {
          request->send(400, "application/json", "{\"error\":\"invalid json\"}");
          return;
        }
        if (!doc["cityIndex"].is<int>())
        {
          request->send(400, "application/json", "{\"error\":\"cityIndex required\"}");
          return;
        }
        int idx = doc["cityIndex"].as<int>();
        if (idx != 0)
        {
          request->send(400, "application/json", "{\"error\":\"only cityIndex 0 (Taipei) is supported\"}");
          return;
        }
        Preferences prefs;
        prefs.begin("cityclock", false);
        prefs.putInt("cityIdx", idx);
        prefs.end();
        Serial.printf("[API] Saved cityclock cityIdx=%d\n", idx);
        request->send(200, "application/json", "{\"ok\":true}");
#else
        request->send(501, "application/json", "{\"error\":\"no storage\"}");
#endif
      });

  server.on("/marquee", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", R"(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Marquee</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}.container{background:#fff;border-radius:20px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:600px;width:100%;padding:40px}h1{color:#333;margin-bottom:10px}label{display:block;margin-top:15px;font-weight:600}input[type=text]{width:100%;padding:10px;margin-top:5px;border:2px solid #e0e0e0;border-radius:8px;font-size:16px}input[type=text]:focus{outline:0;border-color:#667eea}input[type=range]{width:100%;margin-top:5px}button{margin-top:20px;padding:12px 30px;border:none;border-radius:8px;font-weight:600;cursor:pointer;background:#667eea;color:#fff;font-size:16px}button:hover{background:#5a6fd6}#status{margin-top:15px;padding:10px;border-radius:5px;display:none;font-size:13px}#status.ok{background:#d4edda;color:#155724;display:block}#status.err{background:#f8d7da;color:#721c24;display:block}.speed-val{font-size:13px;color:#999;margin-top:3px}</style></head><body><div class="container"><h1>Marquee</h1><p style="color:#666;margin-bottom:10px">Scrolling text on LED matrix (Latin + Cyrillic)</p><div id="status"></div><label>Text<input type="text" id="txt" placeholder="Hello! Привет!" maxlength="500"></label><label>Speed<input type="range" id="spd" min="10" max="200" value="50"><span class="speed-val" id="spdVal">50ms</span></label><button id="send">Send</button></div><script>
const spdEl=document.getElementById('spd'),spdVal=document.getElementById('spdVal'),statusEl=document.getElementById('status');
spdEl.oninput=()=>{spdVal.textContent=spdEl.value+'ms'};
let ws,marqueeId=null;
function show(t,ok){statusEl.textContent=t;statusEl.className=ok?'ok':'err';setTimeout(()=>{statusEl.style.display='none'},4000)}
function connect(){
  const h=location.host||'localhost';
  ws=new WebSocket('ws://'+h+'/ws');
  ws.onopen=()=>{show('Connected',1)};
  ws.onmessage=(e)=>{
    try{const d=JSON.parse(e.data);
      if(d.plugins){for(const p of d.plugins){if(p.name==='Marquee'){marqueeId=p.id;break}}}
    }catch(ex){}
  };
  ws.onerror=()=>{show('WebSocket error',0)};
  ws.onclose=()=>{setTimeout(connect,3000)};
}
connect();
document.getElementById('send').onclick=()=>{
  const txt=document.getElementById('txt').value;
  if(!txt){show('Enter text first',0);return}
  if(!ws||ws.readyState!==1){show('Not connected',0);return}
  if(marqueeId===null){show('Plugin not found',0);return}
  const msg={event:'marquee',text:txt,speed:parseInt(spdEl.value),plugin:marqueeId};
  ws.send(JSON.stringify(msg));
  show('Sent!',1);
};
</script></body></html>
    )");
  });

  server.on("/cityclock", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", R"(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>City Clock</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}.container{background:#fff;border-radius:20px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:600px;width:100%;padding:40px}h1{color:#333;margin-bottom:10px}p{color:#666;margin-bottom:10px}label{display:block;margin-top:15px;font-weight:600}select{width:100%;padding:12px;margin-top:5px;border:2px solid #e0e0e0;border-radius:8px;font-size:16px;background:#fff;cursor:pointer}button{margin-top:20px;padding:12px 30px;border:none;border-radius:8px;font-weight:600;cursor:pointer;background:#667eea;color:#fff;font-size:16px;width:100%;transition:background .2s}button:hover{background:#5a6fd6}#status{margin-top:15px;padding:10px;border-radius:5px;display:none;font-size:13px}#status.ok{background:#d4edda;color:#155724;display:block}#status.err{background:#f8d7da;color:#721c24;display:block}.info{background:#e7f3ff;border-left:4px solid #2196F3;padding:10px;margin-top:20px;border-radius:4px;font-size:13px;color:#1565c0;line-height:1.6}</style></head><body><div class="container"><h1>City Clock</h1><p>Display time and weather for a city</p><div id="status"></div><label>Select City<select id="citySelect"><option value="0">Taipei (UTC+8)</option></select></label><button id="save">Save</button><div class="info">City Clock is locked to Taipei. Weather updates every 10 minutes.</div></div><script>
const cityEl=document.getElementById('citySelect'),statusEl=document.getElementById('status');
function show(t,ok){statusEl.textContent=t;statusEl.className=ok?'ok':'err';statusEl.style.display='block';setTimeout(()=>{statusEl.style.display='none'},4000)}
async function load(){
  try{
    const r=await fetch('/api/cityclock');
    if(!r.ok)return;
    const d=await r.json();
    if(d.cityIndex!==undefined)cityEl.value=String(d.cityIndex);
  }catch(e){show('Load error',0)}
}
load();
document.getElementById('save').onclick=async()=>{
  const idx=parseInt(cityEl.value);
  try{
    const r=await fetch('/api/cityclock',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cityIndex:idx})});
    if(!r.ok){show('Save failed',0);return}
    show('Saved!',1);
  }catch(e){show('Error: '+e.message,0)}
};
</script></body></html>
    )");
  });

  // Forecast config API
  server.on("/api/forecast", HTTP_GET, [](AsyncWebServerRequest *request) {
#ifdef ENABLE_STORAGE
    Preferences prefs;
    prefs.begin("forecast", true);
    JsonDocument doc;
    doc["cityIndex"] = prefs.getInt("cityIdx", 0);
    prefs.end();
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
#else
    request->send(200, "application/json", "{\"cityIndex\":0}");
#endif
  });

  server.on(
      "/api/forecast", HTTP_POST,
      [](AsyncWebServerRequest *request) {},
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
#ifdef ENABLE_STORAGE
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, data, len);
        if (err)
        {
          request->send(400, "application/json", "{\"error\":\"invalid json\"}");
          return;
        }
        if (!doc["cityIndex"].is<int>())
        {
          request->send(400, "application/json", "{\"error\":\"cityIndex required\"}");
          return;
        }
        int idx = doc["cityIndex"].as<int>();
        if (idx < 0 || idx > 3)
        {
          request->send(400, "application/json", "{\"error\":\"cityIndex 0-3\"}");
          return;
        }
        Preferences prefs;
        prefs.begin("forecast", false);
        prefs.putInt("cityIdx", idx);
        prefs.end();
        Serial.printf("[API] Saved forecast cityIdx=%d\n", idx);
        request->send(200, "application/json", "{\"ok\":true}");
#else
        request->send(501, "application/json", "{\"error\":\"no storage\"}");
#endif
      });

  server.on("/forecast", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", R"(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Weather Forecast</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}.container{background:#fff;border-radius:20px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:600px;width:100%;padding:40px}h1{color:#333;margin-bottom:10px}p{color:#666;margin-bottom:10px}label{display:block;margin-top:15px;font-weight:600}select{width:100%;padding:12px;margin-top:5px;border:2px solid #e0e0e0;border-radius:8px;font-size:16px;background:#fff;cursor:pointer}button{margin-top:20px;padding:12px 30px;border:none;border-radius:8px;font-weight:600;cursor:pointer;background:#667eea;color:#fff;font-size:16px;width:100%;transition:background .2s}button:hover{background:#5a6fd6}#status{margin-top:15px;padding:10px;border-radius:5px;display:none;font-size:13px}#status.ok{background:#d4edda;color:#155724;display:block}#status.err{background:#f8d7da;color:#721c24;display:block}.info{background:#e7f3ff;border-left:4px solid #2196F3;padding:10px;margin-top:20px;border-radius:4px;font-size:13px;color:#1565c0;line-height:1.6}</style></head><body><div class="container"><h1>Weather Forecast</h1><p>Tomorrow's weather on LED matrix</p><div id="status"></div><label>Select City<select id="citySelect"><option value="0">Espoo</option><option value="1">Taipei</option><option value="2">St. Petersburg</option><option value="3">Berlin</option></select></label><button id="save">Save</button><div class="info">City will be used when Forecast plugin runs in the scheduler.<br>Data from Open-Meteo API, updates every 30 minutes.</div></div><script>
const cityEl=document.getElementById('citySelect'),statusEl=document.getElementById('status');
function show(t,ok){statusEl.textContent=t;statusEl.className=ok?'ok':'err';statusEl.style.display='block';setTimeout(()=>{statusEl.style.display='none'},4000)}
async function load(){
  try{
    const r=await fetch('/api/forecast');
    if(!r.ok)return;
    const d=await r.json();
    if(d.cityIndex!==undefined)cityEl.value=String(d.cityIndex);
  }catch(e){show('Load error',0)}
}
load();
document.getElementById('save').onclick=async()=>{
  const idx=parseInt(cityEl.value);
  try{
    const r=await fetch('/api/forecast',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cityIndex:idx})});
    if(!r.ok){show('Save failed',0);return}
    show('Saved!',1);
  }catch(e){show('Error: '+e.message,0)}
};
</script></body></html>
    )");
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", R"(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>LED Config</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}.container{background:#fff;border-radius:20px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:600px;width:100%;padding:40px}h1{color:#333;margin-bottom:10px}#msg{padding:12px;margin:15px 0;border-radius:5px;display:none;word-wrap:break-word;font-size:13px;max-height:100px;overflow-y:auto}#msg.ok{background:#d4edda;color:#155724;border:1px solid #c3e6cb}#msg.err{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}label{display:block;margin-top:15px;font-weight:600}input{width:100%;padding:10px;margin-top:5px;border:2px solid #e0e0e0;border-radius:8px}input:focus{outline:0;border-color:#667eea}button{margin-top:20px;padding:12px 20px;border:none;border-radius:8px;font-weight:600;cursor:pointer;margin-right:10px}#save{background:#667eea;color:#fff}#reset{background:#ff4757;color:#fff}.hint{font-size:11px;color:#999;margin-top:3px}</style></head><body><div class="container"><h1>⚙️ LED Config</h1><div id="msg"></div><form id="form"><label>Weather Location<input id="wl" placeholder="Hamburg"><span class="hint">City name (e.g., Turin, Milan, Hamburg)</span></label><label>NTP Server<input id="ntp" placeholder="de.pool.ntp.org"><span class="hint">Time sync server</span></label><label>Timezone<input id="tz" placeholder="CET-1CEST,M3.5.0,M10.5.0/3"><span class="hint">POSIX timezone (CET-1CEST for Italy/Germany)</span></label><button type="submit" id="save">💾 Save</button><button type="button" id="reset">⚠️ Reset</button></form></div><script>const msg=document.getElementById('msg');function show(t,ok){msg.textContent=t;msg.className=ok?'ok':'err';msg.style.display='block';console.log((ok?'✅':'❌')+' '+t);setTimeout(()=>{msg.style.display='none'},6e3)}async function load(){console.log('[Config] Loading...');try{const r=await fetch('/api/config');console.log('[Config] Response:',r.status);if(!r.ok){show('Load failed ('+r.status+')',0);return}const d=await r.json();console.log('[Config] Data:',d);document.getElementById('wl').value=d.weatherLocation||'';document.getElementById('ntp').value=d.ntpServer||'';document.getElementById('tz').value=d.tzInfo||'';show('✅ Loaded',1)}catch(e){console.error('[Config] Load error:',e);show('Load error: '+e.message,0)}}document.getElementById('form').onsubmit=async e=>{e.preventDefault();const data={weatherLocation:document.getElementById('wl').value,ntpServer:document.getElementById('ntp').value,tzInfo:document.getElementById('tz').value,autoStartSchedule:false};console.log('[Config] Saving:',data);try{const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});console.log('[Config] Response:',r.status);if(!r.ok){const err=await r.text();console.error('[Config] Error:',err);show('Save failed: '+err,0);return}show('✅ Saved!',1);setTimeout(load,500)}catch(e){console.error('[Config] Exception:',e);show('Save error: '+e.message,0)}};document.getElementById('reset').onclick=async()=>{if(!confirm('Reset all?'))return;console.log('[Config] Resetting');try{const r=await fetch('/api/config/reset',{method:'POST'});if(!r.ok){show('Reset failed',0);return}show('✅ Reset!',1);setTimeout(load,500)}catch(e){show('Reset error: '+e.message,0)}};load()</script></body></html>
    )");
  });

  server.begin();
}

#endif
