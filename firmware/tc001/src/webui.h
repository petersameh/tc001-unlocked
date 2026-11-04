#pragma once
#include <Arduino.h>

static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TC001</title>
<style>
:root{--bg:#0e0f12;--card:#17191e;--line:#2a2d35;--fg:#e8e8ea;--mute:#8b8f99;--acc:#ff3b30;--up:#30d158;--down:#ff453a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:760px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:4px 0 12px;display:flex;align-items:baseline;gap:10px}h1 small{color:var(--mute);font-weight:400;font-size:13px}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.08em;color:var(--mute);margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:14px}
#screen{width:100%;aspect-ratio:4/1;background:#000;border-radius:10px;display:block}
.tabs{display:flex;gap:4px;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:4px;margin-bottom:14px;position:sticky;top:8px;z-index:2}
.tabs button{flex:1;min-width:0;font-size:14px;padding:9px 4px!important;border:0;background:transparent;border-radius:9px;padding:9px}.tabs button.on{background:#2b2e37}
.tab{display:none}.tab.on{display:block}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}
button{background:#262931;color:var(--fg);border:1px solid var(--line);border-radius:10px;padding:9px 14px;font:inherit;cursor:pointer}
button.p{background:var(--acc);border-color:var(--acc);color:#fff}button:active{transform:translateY(1px)}
button.sm{padding:4px 10px;font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.stat{background:#1d2026;border-radius:10px;padding:10px}.stat b{display:block;font-size:18px}.stat span{color:var(--mute);font-size:12px}
.up{color:var(--up)}.down{color:var(--down)}
label{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:8px 0;border-bottom:1px solid var(--line)}
label:last-child{border-bottom:0}
input,select{background:#0f1115;color:var(--fg);border:1px solid var(--line);border-radius:8px;padding:7px 9px;font:inherit;max-width:60%}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--acc)}input[type=color]{padding:2px;width:46px;height:32px}
input[type=range]{accent-color:var(--acc)}
.item{display:flex;align-items:center;gap:10px;padding:8px 0;border-bottom:1px solid var(--line)}.item:last-child{border-bottom:0}
.item .nm{flex:1}
#cityRes{display:flex;flex-direction:column;gap:6px;margin-top:8px}#cityRes button{text-align:left}
#dcv{width:100%;aspect-ratio:4/1;display:block;touch-action:none;cursor:crosshair;border-radius:8px;background:#000}
.tools{display:flex;gap:6px;flex-wrap:wrap;margin-top:10px}.tools button.on{background:var(--acc);border-color:var(--acc);color:#fff}
.sw{display:flex;gap:6px;flex-wrap:wrap;margin-top:10px}.sw i{width:26px;height:26px;border-radius:6px;border:1px solid var(--line);cursor:pointer;display:block}
.slots{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.slots div{border:2px solid var(--line);border-radius:8px;padding:4px;cursor:pointer;text-align:center;font-size:12px;color:var(--mute)}
.slots div.on{border-color:var(--acc);color:var(--fg)}.slots canvas{width:100%;aspect-ratio:4/1;display:block;background:#000;border-radius:4px}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:#262931;border:1px solid var(--line);padding:10px 16px;border-radius:10px;opacity:0;transition:.3s;pointer-events:none}
#toast.on{opacity:1}
.mute{color:var(--mute);font-size:13px}code{background:#0f1115;padding:1px 5px;border-radius:5px}
</style></head><body><main>
<h1>TC001 <small id="fw"></small></h1>

<div class="card"><canvas id="screen" width="320" height="80"></canvas>
<div class="row"><button onclick="act('prev')">&#9664;</button><button onclick="act('pause')" id="pauseBtn">Pause</button><button onclick="act('next')">&#9654;</button><span class="mute" id="curApp" style="align-self:center"></span></div></div>

<div class="tabs"><button data-t="home" class="on">Home</button><button data-t="dash">Dashboard</button><button data-t="draw">Draw</button><button data-t="auto">Automations</button><button data-t="rec">Recording</button><button data-t="set">Settings</button></div>

<!-- ============ HOME ============ -->
<section class="tab on" id="t-home">
<div class="card"><h2>Status</h2><div class="grid" id="stats"></div></div>
<div class="card"><h2>Send notification</h2>
<label>Text <input id="nText" placeholder="Hello!" maxlength="120"></label>
<label>Icon <select id="nIcon"><option value="">none</option><option>bell</option><option>heart</option><option>check</option><option>youtube</option><option>sun</option><option>cloud</option><option>rain</option></select></label>
<label>Colour <input type="color" id="nColor" value="#ffffff"></label>
<label>Seconds <input type="number" id="nDur" value="8" min="1" max="600"></label>
<label>Beep <input type="checkbox" id="nBeep"></label>
<div class="row"><button class="p" id="nSend" onclick="sendNotify()">Send</button><span class="mute" id="nMsg" style="align-self:center"></span></div></div>
</section>

<!-- ============ DASHBOARD ============ -->
<section class="tab" id="t-dash">
<div class="card"><h2>Live widgets</h2><div class="grid" id="dstats"></div>
<div class="row"><button class="p" onclick="showDash(1)">Pin dashboard</button><button onclick="showDash(0)">Show now</button><button onclick="act('dashboard/refresh')">Refresh data</button></div></div>

<div class="card"><h2>Layout</h2>
<label>Style <select id="dashStyle"><option value="1">Compact – two lines, no scrolling</option><option value="0">Ticker – pinned clock + scrolling</option></select></label>
<p class="mute" id="styleHint"></p>
<label>Dashboard screen enabled <input type="checkbox" id="dashEn"></label>
<label>Seconds in rotation (0 = stay) <input type="number" id="dashDur" min="0" max="3600"></label>
<label>Ticker speed <input type="range" id="tickSpeed" min="5" max="60"></label>
<label>Seconds bar under clock <input type="checkbox" id="dashSecBar"></label>
<label>Clock colour <input type="color" id="colDashClock"></label></div>

<div class="card"><h2>Items</h2><p class="mute" style="margin-top:0">Compact uses on/off only (fixed positions); Ticker also uses the order.</p><div id="items"></div></div>

<div class="card"><h2>Stock</h2>
<label>Symbol <input id="stockSym" placeholder="EGX:COMI"></label>
<label>Label colour <input type="color" id="colStock"></label>
<label>Refresh every (s) <input type="number" id="stockEvery" min="30" max="3600"></label>
<p class="mute">TradingView format <code>EXCHANGE:SYMBOL</code> — e.g. <code>EGX:COMI</code>, <code>EGX:TMGH</code>, <code>NASDAQ:AAPL</code>. EGX prices are ~15 min delayed.</p></div>

<div class="card"><h2>Weather</h2>
<label>City <input id="cityQ" placeholder="Search city…"></label>
<div id="cityRes"></div>
<label>Selected <span id="cityNow" class="mute"></span></label>
<label>Refresh every (s) <input type="number" id="wxEvery" min="300" max="86400"></label>
<p class="mute">Open-Meteo · free · no API key. Units follow the °F setting.</p></div>

<div class="row" style="margin-bottom:14px"><button class="p" onclick="saveDash()">Save dashboard</button></div>
</section>

<!-- ============ DRAW ============ -->
<section class="tab" id="t-draw">
<div class="card"><h2>Pixel editor · 32×8</h2>
<canvas id="dcv" width="640" height="160"></canvas>
<div class="tools">
<button data-tool="pen" class="on">✏️ Pen</button><button data-tool="erase">⌫ Eraser</button><button data-tool="fill">🪣 Fill</button><button data-tool="pick">💧 Pick</button>
<input type="color" id="dColor" value="#ff3b30" title="Colour">
<button onclick="undoDraw()">↶ Undo</button><button onclick="clearDraw()">Clear</button>
</div>
<div class="sw" id="swatches"></div>
<label style="margin-top:8px">Live on clock (updates as you draw) <input type="checkbox" id="dLive"></label>
<div class="row"><button class="p" onclick="sendFrame(true)">Show on clock</button><button onclick="copyFromClock()">Copy from clock</button><button onclick="shift(-1)">◀ Shift</button><button onclick="shift(1)">Shift ▶</button></div></div>
<div class="card"><h2>Saved drawings (on the clock)</h2><div class="slots" id="slots"></div>
<div class="row"><button onclick="loadSlot()">Load selected</button><button class="p" onclick="saveSlot()">Save to selected</button></div>
<label style="margin-top:8px">Include drawing screen in rotation <input type="checkbox" id="drawEn"></label></div>
</section>

<!-- ============ AUTOMATIONS ============ -->
<section class="tab" id="t-auto">
<div class="card"><h2>Rooms</h2><div class="grid" id="hRooms"></div>
<div class="row"><button onclick="homeRefresh()">Refresh now</button><button onclick="showHome(1)">Pin home screen</button><button onclick="showHome(0)">Show home screen</button></div></div>
<div class="card"><h2>Devices</h2><div id="hDevs"><p class="mute">Connect Tuya below, then press Discover remotes.</p></div></div>
<div class="card"><h2>Rules</h2><div id="hRules"></div>
<div class="row"><button class="p" onclick="editRule(-1)">+ New rule</button></div>
<div id="hEdit" style="display:none;margin-top:12px;border-top:1px solid var(--line);padding-top:10px"></div></div>
<div class="card"><h2>Log</h2><div id="hLog" class="mute" style="font-family:ui-monospace,monospace;font-size:12px;white-space:pre-wrap;max-height:240px;overflow:auto"></div></div>
<div class="card"><h2>Tuya connection</h2>
<label>Access ID <input id="hCid" autocomplete="off"></label>
<label>Access Secret <input id="hSecret" type="password" autocomplete="new-password"></label>
<label>Region <select id="hRegion"><option value="eu">Central Europe (eu)</option><option value="eu-w">Western Europe</option><option value="us">Western America</option><option value="us-e">Eastern America</option><option value="in">India</option><option value="cn">China</option></select></label>
<label>Poll rooms every (s) <input type="number" id="hEvery" min="60" max="3600"></label>
<label>Hub 1 ID <input id="hHub0"></label><label>Hub 1 label (1 letter) <input id="hLab0" maxlength="1" style="width:56px"></label>
<label>Hub 2 ID <input id="hHub1"></label><label>Hub 2 label (1 letter) <input id="hLab1" maxlength="1" style="width:56px"></label>
<div class="row"><button class="p" onclick="saveTuya()">Save connection</button><button onclick="discover()">Discover remotes</button></div>
<p class="mute" id="hConnMsg"></p>
<p class="mute">Access ID / Secret: iot.tuya.com → Cloud → your project → Overview. The secret is stored on the clock and never shown again.</p></div>
</section>

<!-- ============ RECORDING ============ -->
<section class="tab" id="t-rec">
<div class="card" style="text-align:center"><h2 style="text-align:left">Recording mode</h2>
<button id="recBtn" class="p" style="font-size:20px;padding:16px 28px;margin:6px 0" onclick="recToggle()">● Start recording</button>
<p class="mute" id="recState">On the clock: long-press the middle button to start, short-press middle to stop. Left/right = previous/next slide.</p>
<div class="row" style="justify-content:center"><button onclick="post('/api/rec/next')">Next slide ▶</button></div></div>
<div class="card"><h2>Slides</h2><div id="recItems"></div>
<label>Seconds per slide <input type="number" id="recSecs" min="1" max="60"></label>
<label>Logo slide uses drawing slot <select id="recLogo"><option value="0">1</option><option value="1">2</option><option value="2">3</option><option value="3">4</option><option value="4">5</option><option value="5">6</option></select></label></div>
<div class="card"><h2>Camera</h2>
<label>Brightness while recording <input type="range" id="recBright" min="5" max="255"></label>
<label>Mute notifications &amp; beeps while recording <input type="checkbox" id="recMute"></label>
<p class="mute">LED panels can flicker or band on camera when dim; ~60% brightness usually films cleanly. The Subscribe slide flashes at under 1 Hz so it doesn't strobe on video.</p></div>
<div class="row" style="margin-bottom:14px"><button class="p" onclick="recSave()">Save recording settings</button></div>
</section>

<!-- ============ SETTINGS ============ -->
<section class="tab" id="t-set">
<div class="card"><h2>Screens</h2><div id="apps"></div>
<label>Seconds per screen (0 = manual) <input type="number" id="appDur" min="0" max="3600"></label></div>
<div class="card"><h2>Display</h2>
<label>Brightness <input type="range" id="bright" min="1" max="255"></label>
<label>24-hour clock <input type="checkbox" id="h24"></label>
<label>Show seconds (clock screen, 24-hour only) <input type="checkbox" id="secs"></label>
<label>Date format <select id="dateFmt"><option value="0">MM/DD</option><option value="1">DD/MM</option></select></label>
<label>Week starts Monday <input type="checkbox" id="weekMon"></label>
<label>Clock colour <input type="color" id="colClock"></label>
<label>Date colour <input type="color" id="colDate"></label>
<label>YouTube number colour <input type="color" id="colYt"></label>
<label>Sensor text colour <input type="color" id="colTemp"></label>
<label>Timezone (POSIX) <input id="tz" placeholder="<+03>-3"></label>
<p class="mute">Examples: <code>&lt;+03&gt;-3</code> (fixed UTC+3), <code>EET-2EEST,M4.5.5/0,M10.5.4/24</code> (Cairo with DST).</p></div>
<div class="card"><h2>YouTube</h2>
<label>Channel ID <input id="ytId" placeholder="UC..."></label>
<label>API key <input id="ytKey" type="password" placeholder="(unchanged)"></label>
<label>Refresh every (s) <input type="number" id="ytEvery" min="60" max="86400"></label></div>
<div class="card"><h2>Sensors</h2>
<label>Fahrenheit <input type="checkbox" id="tempF"></label>
<label>Temperature offset (&deg;C) <input type="number" step="0.1" id="tempOffset"></label>
<label>Battery raw = 0% <input type="number" id="batEmpty"></label>
<label>Battery raw = 100% <input type="number" id="batFull"></label></div>
<div class="row" style="margin-bottom:14px"><button class="p" onclick="save()">Save settings</button></div>
<div class="card"><h2>Firmware</h2>
<form id="otaForm"><input type="file" id="otaFile" accept=".bin"> <button type="submit">Upload &amp; flash</button></form>
<p class="mute" id="otaMsg"></p>
<div class="row"><button onclick="act('beep')">Test beep</button><button onclick="if(confirm('Reboot clock?'))act('reboot')">Reboot</button></div></div>
</section>
</main><div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const APPS={dashboard:'Dashboard',clock:'Clock',date:'Date',youtube:'YouTube subscribers',temp:'Temperature',humidity:'Humidity',battery:'Battery',drawing:'Drawing',home:'Home (rooms & AC)'};
const ITEMS={stock:'Stock price',youtube:'YouTube subscribers',weather:'Weather'};
let CFG={},order=[],city={};
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('on');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('on'),1800)}
const post=(u,b)=>fetch(u,{method:'POST',headers:b?{'Content-Type':'application/json'}:{},body:b?JSON.stringify(b):undefined});
async function act(a){await post('/api/'+a);toast('✓');refresh()}
async function showDash(pin){await post('/api/show?app=dashboard'+(pin?'&pin=1':''));toast(pin?'Pinned':'Showing');refresh()}
document.querySelectorAll('.tabs button').forEach(b=>b.onclick=()=>{document.querySelectorAll('.tabs button').forEach(x=>x.classList.toggle('on',x===b));
document.querySelectorAll('.tab').forEach(s=>s.classList.toggle('on',s.id==='t-'+b.dataset.t));try{localStorage.tab=b.dataset.t}catch(e){}});
try{const t=localStorage.tab;if(t)document.querySelector(`.tabs [data-t="${t}"]`).click()}catch(e){}
const cv=$('screen'),cx=cv.getContext('2d');
async function drawScreen(){try{const h=await (await fetch('/api/screen')).text();cx.fillStyle='#000';cx.fillRect(0,0,320,80);
for(let i=0;i<256;i++){const c=h.substr(i*6,6);const x=i%32,y=(i/32)|0;cx.fillStyle=c==='000000'?'#111':'#'+c;cx.beginPath();cx.arc(x*10+5,y*10+5,c==='000000'?1.5:4,0,7);cx.fill()}}catch(e){}}
const age=s=>s<0?'':s<90?s+'s ago':Math.round(s/60)+' min ago';
const card=([k,v,n,c])=>`<div class="stat"><span>${k}</span><b class="${c||''}">${v}</b><span>${n||''}</span></div>`;
async function refresh(){try{const s=await (await fetch('/api/status')).json();$('fw').textContent='fw '+s.fw+' · '+s.ip;
$('pauseBtn').textContent=s.paused?'Resume':'Pause';$('curApp').textContent='showing: '+(APPS[s.app]||s.app)+(s.paused?' (paused)':'');
const st=s.stock||{},wx=s.weather||{};const up=(st.changePct||0)>=0;
const stockCard=['Stock · '+(st.symbol||''),st.price!=null?st.price.toFixed(2)+' '+(st.currency||''):'—',st.err||((st.changePct!=null?(up?'▲ ':'▼ ')+Math.abs(st.changePct).toFixed(2)+'% · ':'')+age(st.ageS)),st.err?'':(up?'up':'down')];
const wxCard=['Weather · '+(wx.city||''),wx.temp!=null?Math.round(wx.temp)+'°':'—',wx.err||((wx.desc||'')+' · '+age(wx.ageS))];
const ytCard=['Subscribers',s.ytSubs>=0?s.ytSubs.toLocaleString():'—',s.ytErr||age(s.ytAgeS)];
const batCard=['Battery',s.batPct+' %','raw '+s.batRaw];
$('dstats').innerHTML=[stockCard,ytCard,wxCard,batCard].map(card).join('');
$('stats').innerHTML=[['Time',s.time||'not synced'],ytCard,stockCard,wxCard,['Room temp',s.tempC!=null?s.tempC+' °C':'—','sensor'],['Humidity',s.humidity!=null?s.humidity+' %':'—'],batCard,
['Wi-Fi',s.ssid,s.rssi+' dBm'],['Uptime',Math.floor(s.uptime/3600)+'h '+Math.floor(s.uptime%3600/60)+'m','heap '+(s.heap/1024|0)+' KB']].map(card).join('')}catch(e){}}
function renderItems(){const all=[...order,...Object.keys(ITEMS).filter(k=>!order.includes(k))];
$('items').innerHTML=all.map(k=>{const on=order.includes(k),i=order.indexOf(k);return `<div class="item"><input type="checkbox" ${on?'checked':''} onchange="toggleItem('${k}',this.checked)"><span class="nm">${ITEMS[k]}</span>
<button class="sm" ${!on||i===0?'disabled':''} onclick="mv('${k}',-1)">▲</button><button class="sm" ${!on||i===order.length-1?'disabled':''} onclick="mv('${k}',1)">▼</button></div>`}).join('')}
function toggleItem(k,on){order=on?[...order,k]:order.filter(x=>x!==k);renderItems()}
function mv(k,d){const i=order.indexOf(k),j=i+d;[order[i],order[j]]=[order[j],order[i]];renderItems()}
async function load(){const s=CFG=await (await fetch('/api/settings')).json();
for(const k of ['bright','appDur','tz','ytId','ytEvery','tempOffset','batEmpty','batFull','colClock','colDate','colYt','colTemp','dashDur','tickSpeed','colDashClock','stockSym','colStock','stockEvery','wxEvery'])$(k).value=s[k];
for(const k of ['h24','secs','weekMon','tempF','dashSecBar'])$(k).checked=s[k];$('dateFmt').value=s.dateFmt;$('dashEn').checked=s.apps.dashboard;$('drawEn').checked=!!s.apps.drawing;$('dashStyle').value=s.dashStyle;hint();
$('ytKey').placeholder=s.ytKeySet?'(saved – leave blank to keep)':'paste API key';
city={city:s.city,lat:s.lat,lon:s.lon};$('cityNow').textContent=`${s.city} (${(+s.lat).toFixed(2)}, ${(+s.lon).toFixed(2)})`;
order=s.dashOrder.split(',').map(x=>x.trim()).filter(x=>ITEMS[x]);renderItems();
$('apps').innerHTML=Object.keys(APPS).map(k=>`<label>${APPS[k]} <input type="checkbox" data-app="${k}" ${s.apps[k]?'checked':''}></label>`).join('')}
async function save(){const b={};for(const k of ['bright','appDur','ytEvery','batEmpty','batFull','dateFmt'])b[k]=parseInt($(k).value);
b.tempOffset=parseFloat($('tempOffset').value)||0;for(const k of ['tz','ytId','ytKey','colClock','colDate','colYt','colTemp'])b[k]=$(k).value;
for(const k of ['h24','secs','weekMon','tempF'])b[k]=$(k).checked;b.apps={};document.querySelectorAll('[data-app]').forEach(e=>b.apps[e.dataset.app]=e.checked);
const r=await post('/api/settings',b);toast(r.ok?'Saved ✓':'Save failed');$('ytKey').value='';load()}
async function saveDash(){const b={dashStyle:parseInt($('dashStyle').value),dashOrder:order.join(','),dashDur:parseInt($('dashDur').value)||0,tickSpeed:parseInt($('tickSpeed').value),dashSecBar:$('dashSecBar').checked,
colDashClock:$('colDashClock').value,colStock:$('colStock').value,stockSym:$('stockSym').value,stockEvery:parseInt($('stockEvery').value)||60,
wxEvery:parseInt($('wxEvery').value)||900,city:city.city,lat:+city.lat,lon:+city.lon,apps:{dashboard:$('dashEn').checked}};
const r=await post('/api/settings',b);toast(r.ok?'Dashboard saved ✓':'Save failed');load();setTimeout(refresh,3000)}
let cq;$('cityQ').oninput=e=>{clearTimeout(cq);const q=e.target.value.trim();if(q.length<2){$('cityRes').innerHTML='';return}
cq=setTimeout(async()=>{try{const r=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=5&name='+encodeURIComponent(q))).json();
$('cityRes').innerHTML=(r.results||[]).map(c=>`<button class="sm" data-c='${JSON.stringify({city:c.name,lat:c.latitude,lon:c.longitude}).replace(/'/g,"&#39;")}'>${c.name}, ${c.admin1||''} ${c.country||''}</button>`).join('')||'<span class="mute">No match</span>';
$('cityRes').querySelectorAll('button').forEach(b=>b.onclick=()=>{city=JSON.parse(b.dataset.c);$('cityNow').textContent=`${city.city} (${city.lat.toFixed(2)}, ${city.lon.toFixed(2)}) – save to apply`;$('cityRes').innerHTML='';$('cityQ').value=''})}
catch(e){$('cityRes').innerHTML='<span class="mute">City search needs internet on this device</span>'}},350)};
$('tickSpeed').onchange=e=>post('/api/settings',{tickSpeed:parseInt(e.target.value)});
$('bright').oninput=e=>post('/api/settings',{bright:parseInt(e.target.value)});
$('otaForm').onsubmit=async e=>{e.preventDefault();const f=$('otaFile').files[0];if(!f)return;$('otaMsg').textContent='Uploading…';
const fd=new FormData();fd.append('firmware',f);const r=await fetch('/update',{method:'POST',body:fd});$('otaMsg').textContent=await r.text()};
async function sendNotify(){const btn=$('nSend'),msg=$('nMsg');const b={text:$('nText').value,icon:$('nIcon').value,color:$('nColor').value,duration:parseInt($('nDur').value)||8,beep:$('nBeep').checked};
if(!b.text){msg.textContent='Type some text first';$('nText').focus();return}btn.disabled=true;btn.textContent='Sending…';msg.textContent='';try{const r=await post('/api/notify',b);msg.textContent=r.ok?'✓ Shown on the clock at '+new Date().toLocaleTimeString():'✗ '+await r.text();toast(r.ok?'Sent ✓':'Failed')}catch(e){msg.textContent='✗ Could not reach the clock: '+e.message}finally{btn.disabled=false;btn.textContent='Send'}}
$('nText').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();sendNotify()}});
// ---------- pixel editor ----------
let PX=new Array(256).fill('000000'),tool='pen',undoStack=[],painting=false,lastCell=-1,sendT=null,selSlot=0;
const dc=$('dcv'),dx=dc.getContext('2d');
const BASE=['ff0000','ff8000','ffd60a','30d158','00c7be','0a84ff','5e5ce6','bf5af2','ff2d55','ffffff','8e8e93','000000'];
let recent=[];try{recent=JSON.parse(localStorage.recent||'[]')}catch(e){}
function dColor(){return $('dColor').value.slice(1).toLowerCase()}
function paintGrid(){dx.fillStyle='#000';dx.fillRect(0,0,640,160);for(let i=0;i<256;i++){const x=i%32,y=i/32|0;dx.fillStyle=PX[i]==='000000'?'#17191e':'#'+PX[i];dx.fillRect(x*20+1,y*20+1,18,18)}}
function swatches(){$('swatches').innerHTML=[...new Set([...recent,...BASE])].slice(0,18).map(c=>`<i style="background:#${c}" data-c="${c}" title="#${c}"></i>`).join('');
$('swatches').querySelectorAll('i').forEach(e=>e.onclick=()=>{$('dColor').value='#'+e.dataset.c;if(tool==='erase'||tool==='pick')setTool('pen')})}
function addRecent(c){if(c==='000000')return;recent=[c,...recent.filter(x=>x!==c)].slice(0,6);try{localStorage.recent=JSON.stringify(recent)}catch(e){}swatches()}
function setTool(t){tool=t;document.querySelectorAll('[data-tool]').forEach(b=>b.classList.toggle('on',b.dataset.tool===t))}
document.querySelectorAll('[data-tool]').forEach(b=>b.onclick=()=>setTool(b.dataset.tool));
function cellAt(e){const r=dc.getBoundingClientRect();const x=Math.floor((e.clientX-r.left)/r.width*32),y=Math.floor((e.clientY-r.top)/r.height*8);return x<0||x>31||y<0||y>7?-1:y*32+x}
function flood(i,from,to){if(from===to)return;const st=[i];while(st.length){const k=st.pop();if(PX[k]!==from)continue;PX[k]=to;const x=k%32,y=k/32|0;if(x>0)st.push(k-1);if(x<31)st.push(k+1);if(y>0)st.push(k-32);if(y<7)st.push(k+32)}}
function applyAt(i){if(i<0)return;const c=dColor();
if(tool==='pen')PX[i]=c;else if(tool==='erase')PX[i]='000000';else if(tool==='fill')flood(i,PX[i],c);
else if(tool==='pick'){if(PX[i]!=='000000')$('dColor').value='#'+PX[i];setTool('pen');return}
paintGrid();queueLive()}
dc.onpointerdown=e=>{e.preventDefault();if(tool!=='pick')pushUndo();painting=true;lastCell=-1;dc.setPointerCapture(e.pointerId);const i=cellAt(e);lastCell=i;applyAt(i)};
dc.onpointermove=e=>{if(!painting||(tool!=='pen'&&tool!=='erase'))return;const i=cellAt(e);if(i!==lastCell){lastCell=i;applyAt(i)}};
dc.onpointerup=dc.onpointercancel=()=>{if(painting&&tool==='pen')addRecent(dColor());painting=false};
function pushUndo(){undoStack.push(PX.slice());if(undoStack.length>60)undoStack.shift()}
function undoDraw(){if(!undoStack.length)return;PX=undoStack.pop();paintGrid();queueLive()}
function clearDraw(){pushUndo();PX.fill('000000');paintGrid();queueLive()}
function shift(d){pushUndo();const n=new Array(256).fill('000000');for(let i=0;i<256;i++){const x=i%32,y=i/32|0,nx=x+d;if(nx>=0&&nx<32)n[y*32+nx]=PX[i]}PX=n;paintGrid();queueLive()}
function queueLive(){if(!$('dLive').checked)return;clearTimeout(sendT);sendT=setTimeout(()=>sendFrame(true,true),120)}
async function sendFrame(show,quiet){const r=await fetch('/api/draw'+(show?'?show=1':''),{method:'POST',headers:{'Content-Type':'text/plain'},body:PX.join('')});if(!quiet)toast(r.ok?'On the clock ✓':'Failed');if(show&&!quiet)refresh()}
$('dLive').onchange=e=>{if(e.target.checked)sendFrame(true)};
async function copyFromClock(){const h=await (await fetch('/api/screen')).text();pushUndo();for(let i=0;i<256;i++)PX[i]=h.substr(i*6,6);paintGrid();toast('Copied current screen')}
function thumb(cv,h){const c=cv.getContext('2d');c.fillStyle='#000';c.fillRect(0,0,64,16);if(!h)return;for(let i=0;i<256;i++){const v=h.substr(i*6,6);if(v!=='000000'){c.fillStyle='#'+v;c.fillRect((i%32)*2,(i/32|0)*2,2,2)}}}
async function loadSlots(){const r=await (await fetch('/api/draw/slots')).json();selSlot=r.active;
$('slots').innerHTML=r.slots.map((h,i)=>`<div data-s="${i}" class="${i===selSlot?'on':''}"><canvas width="64" height="16"></canvas>Slot ${i+1}${h?'':' · empty'}</div>`).join('');
$('slots').querySelectorAll('div').forEach((d,i)=>{thumb(d.querySelector('canvas'),r.slots[i]);d.onclick=()=>{selSlot=i;$('slots').querySelectorAll('div').forEach(x=>x.classList.toggle('on',x===d))}})}
async function loadSlot(){const h=await (await post('/api/draw/load?slot='+selSlot)).text();pushUndo();for(let i=0;i<256;i++)PX[i]=h.substr(i*6,6)||'000000';paintGrid();toast('Loaded slot '+(selSlot+1));if($('dLive').checked)sendFrame(true,true)}
async function saveSlot(){await sendFrame(false,true);const r=await post('/api/draw/save?slot='+selSlot);toast(r.ok?'Saved to slot '+(selSlot+1)+' ✓':'Save failed');loadSlots()}
$('drawEn').onchange=e=>post('/api/settings',{apps:{drawing:e.target.checked}}).then(()=>toast(e.target.checked?'Drawing added to rotation':'Removed from rotation'));
async function initDraw(){const h=await (await fetch('/api/draw')).text();for(let i=0;i<256;i++)PX[i]=h.substr(i*6,6)||'000000';paintGrid();swatches();loadSlots()}
// ---------- automations ----------
let H=null,KEYS={},KEYC={},editIdx=null,draft=null,devSig='';
const MODES=['Cool','Heat','Auto','Fan','Dry'],WINDS=['Auto','Low','Mid','High'],DAYN=['Su','Mo','Tu','We','Th','Fr','Sa'];
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function hubLab(i){return (H&&H.hubs[i]&&H.hubs[i].label)||(i?'B':'L')}
function srcList(){const l=[['outTemp','Outdoor temp °C'],['outHum','Outdoor humidity %'],['rain','Raining (1 = yes)']];
(H?H.hubs:[]).forEach((h,i)=>{if(h.id){l.push(['temp'+i,hubLab(i)+' room temp °C']);l.push(['hum'+i,hubLab(i)+' room humidity %'])}});
(H?H.remotes:[]).filter(r=>r.cat==5).forEach(r=>l.push(['ac:'+r.id,r.name+' is on (1 = on)']));
l.push(['roomTempClock','Clock sensor °C']);return l}
const srcName=v=>(srcList().find(x=>x[0]===v)||[v,v])[1];
const remName=id=>((H&&H.remotes.find(r=>r.id===id))||{name:'?'}).name;
function actText(a){if(a.type==='notify')return 'notify “'+esc(a.text)+'”'+(a.beep?' + beep':'');
if(a.type==='ac')return esc(remName(a.remote))+(a.power?' ON '+MODES[a.mode]+' '+a.temp+'°':' OFF');
return esc(remName(a.remote))+' ['+esc(a.name||a.key)+']'}
function ruleText(r){const c=(r.cond||[]).map(c=>esc(srcName(c.src))+' '+(c.op==='=='?'=':c.op==='>='?'≥':'≤')+' '+c.val).join(' AND ')||'(no conditions)';
const t=r.from&&r.to?` · ${r.from}–${r.to}`:'';const d=(r.days??127)===127?'':' · '+DAYN.filter((_,i)=>r.days&1<<i).join(' ');
return `IF ${c}${t}${d} → ${(r.act||[]).map(actText).join(', ')||'(no actions)'} · cooldown ${r.cooldown??30} min`}
async function homeLoad(){try{H=await (await fetch('/api/home')).json();renderHomeUI()}catch(e){}}
function keyOpts(id,sel){return (KEYS[id]||[]).map(k=>`<option value="${k.key_id}" ${k.key_id==sel?'selected':''}>${esc(k.name)}</option>`).join('')}
async function loadKeys(id){if(KEYS[id]||!id)return;try{const j=await (await fetch('/api/home/keys?remote='+encodeURIComponent(id))).json();
if(j.keys){KEYS[id]=j.keys;KEYC[id]=j.cat;const el=$('k_'+id);if(el)el.innerHTML='<option value="">choose button</option>'+keyOpts(id)}else toast('Keys: '+(j.err||'failed'))}catch(e){toast('Could not load buttons')}}
function renderHomeUI(){if(!H)return;
const rc=[];H.hubs.forEach((h,i)=>{if(!h.id)return;const ac=H.remotes.find(r=>r.cat==5&&r.hub===h.id);
rc.push(card([hubLab(i)+' room',h.temp!=null?h.temp.toFixed(1)+'°':'—',(h.hum!=null?Math.round(h.hum)+'% · ':'')+(h.err||age(h.ageS))]));
if(ac)rc.push(card([ac.name,ac.power==1?'ON':ac.power==0?'OFF':'—',ac.power==1?MODES[ac.mode]+' '+ac.temp+'° · fan '+WINDS[ac.wind]:'',ac.power==1?'up':'']))});
rc.push(card(['Outdoor',H.outTemp!=null?Math.round(H.outTemp)+'°':'—',H.outHum!=null?Math.round(H.outHum)+'% humidity':'']));
$('hRooms').innerHTML=rc.join('');
const sig=JSON.stringify(H.remotes)+Object.keys(KEYS).join();
if(H.remotes.length&&sig!==devSig&&!$('hDevs').contains(document.activeElement)){devSig=sig;
$('hDevs').innerHTML=H.remotes.map(r=>{const hub=H.hubs.findIndex(h=>h.id===r.hub);
if(r.cat==5)return `<div class="item" style="flex-wrap:wrap"><span class="nm"><b>${esc(r.name)}</b> <span class="mute">${hubLab(hub)} · ${r.power==1?'on':r.power==0?'off':'?'}</span></span>
<select id="m_${r.id}">${MODES.map((m,i)=>`<option value="${i}" ${i==r.mode?'selected':''}>${m}</option>`).join('')}</select>
<input type="number" id="t_${r.id}" value="${r.temp||24}" min="16" max="30" style="width:64px">
<select id="w_${r.id}">${WINDS.map((m,i)=>`<option value="${i}" ${i==r.wind?'selected':''}>${m}</option>`).join('')}</select>
<button class="sm p" onclick="acSend('${r.id}',1)">On</button><button class="sm" onclick="acSend('${r.id}',0)">Off</button></div>`;
return `<div class="item"><span class="nm"><b>${esc(r.name)}</b> <span class="mute">${hubLab(hub)}</span></span>
<select id="k_${r.id}" onfocus="loadKeys('${r.id}')" style="max-width:45%"><option value="">${KEYS[r.id]?'choose button':'tap to load buttons'}</option>${keyOpts(r.id)}</select>
<button class="sm" onclick="keySend('${r.id}')">Press</button></div>`}).join('')}
const st=H.ruleState||[];
$('hRules').innerHTML=(H.rules||[]).map((r,i)=>`<div class="item" style="align-items:flex-start"><input type="checkbox" title="enabled" ${r.en?'checked':''} onchange="toggleRule(${i},this.checked)">
<span class="nm"><b>${esc(r.name||'Rule')}</b> <span class="mute">${st[i]==1?'● condition true now':st[i]==0?'○ condition false now':'… waiting for data'}</span><br><span class="mute">${ruleText(r)}</span></span>
<button class="sm" onclick="testRule(${i})">Test</button><button class="sm" onclick="editRule(${i})">Edit</button><button class="sm" onclick="delRule(${i})">✕</button></div>`).join('')||'<p class="mute">No rules yet.</p>';
$('hLog').textContent=(H.log||[]).join('\n')||'(empty)';
const conn=$('t-auto').querySelector('#hCid').closest('.card');
if(!conn.contains(document.activeElement)){$('hCid').placeholder=H.idSet?'(saved – leave blank to keep)':'paste Access ID';$('hSecret').placeholder=H.secretSet?'(saved – leave blank to keep)':'paste Access Secret';
$('hRegion').value=H.region;$('hEvery').value=H.every;H.hubs.forEach((h,i)=>{$('hHub'+i).value=h.id;$('hLab'+i).value=h.label})}
$('hConnMsg').textContent=H.lastErr?'Last error: '+H.lastErr:(H.idSet&&H.secretSet?'Connected credentials saved.':'')}
async function acSend(id,p){const b={type:'ac',remote:id,power:p,mode:+$('m_'+id).value,temp:+$('t_'+id).value,wind:+$('w_'+id).value};
const r=await post('/api/home/action',b);toast(r.ok?'Sent to '+remName(id):'Failed');setTimeout(homeLoad,2500)}
async function keySend(id){await loadKeys(id);const v=$('k_'+id).value;const k=(KEYS[id]||[]).find(x=>x.key_id==v);if(!k){toast('Choose a button first');return}
const r=await post('/api/home/action',{type:'key',remote:id,key:k.key,key_id:k.key_id,cat:KEYC[id],name:k.name});toast(r.ok?'Pressed '+k.name:'Failed');setTimeout(homeLoad,2500)}
async function saveRules(arr){const r=await post('/api/home/rules',arr);toast(r.ok?'Rules saved ✓':'Save failed: '+await r.text());await homeLoad()}
function toggleRule(i,on){const a=JSON.parse(JSON.stringify(H.rules));a[i].en=on;saveRules(a)}
function delRule(i){if(!confirm('Delete rule “'+H.rules[i].name+'”?'))return;const a=JSON.parse(JSON.stringify(H.rules));a.splice(i,1);saveRules(a)}
async function testRule(i){await post('/api/home/rules/test?i='+i);toast('Running “'+H.rules[i].name+'” actions…');setTimeout(homeLoad,3000)}
async function homeRefresh(){await post('/api/home/refresh');toast('Refreshing…');setTimeout(homeLoad,4000)}
async function showHome(pin){await post('/api/show?app=home'+(pin?'&pin=1':''));toast(pin?'Home screen pinned':'Showing home screen');refresh()}
async function saveTuya(){const b={region:$('hRegion').value,every:parseInt($('hEvery').value)||300,hubs:[0,1].map(i=>({id:$('hHub'+i).value.trim(),label:$('hLab'+i).value.trim().slice(0,1)}))};
if($('hCid').value.trim())b.cid=$('hCid').value.trim();if($('hSecret').value.trim())b.secret=$('hSecret').value.trim();
const r=await post('/api/home/config',b);$('hCid').value='';$('hSecret').value='';toast(r.ok?'Connection saved ✓':'Save failed');setTimeout(homeLoad,4000)}
async function discover(){$('hConnMsg').textContent='Discovering…';const r=await (await post('/api/home/discover')).json().catch(()=>({}));
$('hConnMsg').textContent=r.ok?'Found '+r.count+' remotes ✓':'Discover failed: '+(r.err||'?');devSig='';homeLoad()}
// rule editor
function defaultAct(t){const acs=H.remotes.filter(r=>r.cat==5),oth=H.remotes.filter(r=>r.cat!=5);
if(t==='ac')return {type:'ac',remote:acs[0]?acs[0].id:'',power:1,mode:0,temp:24,wind:0};
if(t==='key')return {type:'key',remote:oth[0]?oth[0].id:'',key:'',key_id:0,cat:0,name:''};
return {type:'notify',text:'HELLO',beep:false}}
function editRule(i){editIdx=i;draft=i<0?{name:'New rule',en:false,cooldown:30,from:'',to:'',days:127,cond:[{src:'outTemp',op:'>=',val:30}],act:[defaultAct('notify')]}:JSON.parse(JSON.stringify(H.rules[i]));
$('hEdit').style.display='block';drawEditor();$('hEdit').scrollIntoView({behavior:'smooth',block:'start'})}
function sel(opts,cur,on){return `<select onchange="${on}">${opts.map(([v,t])=>`<option value="${esc(v)}" ${String(v)===String(cur)?'selected':''}>${esc(t)}</option>`).join('')}</select>`}
function drawEditor(){const d=draft,acs=H.remotes.filter(r=>r.cat==5).map(r=>[r.id,r.name]),oth=H.remotes.filter(r=>r.cat!=5).map(r=>[r.id,r.name]);
const conds=d.cond.map((c,j)=>`<div class="item">${sel(srcList(),c.src,`draft.cond[${j}].src=this.value`)}${sel([['>=','≥'],['<=','≤'],['==','=']],c.op,`draft.cond[${j}].op=this.value`)}
<input type="number" step="0.5" value="${c.val}" style="width:80px" oninput="draft.cond[${j}].val=parseFloat(this.value)||0"><button class="sm" onclick="draft.cond.splice(${j},1);drawEditor()">✕</button></div>`).join('');
const acts=d.act.map((a,j)=>{let f='';
if(a.type==='ac')f=sel(acs,a.remote,`draft.act[${j}].remote=this.value`)+sel([[1,'On'],[0,'Off']],a.power,`draft.act[${j}].power=+this.value`)+sel(MODES.map((m,i)=>[i,m]),a.mode,`draft.act[${j}].mode=+this.value`)
+`<input type="number" value="${a.temp}" min="16" max="30" style="width:64px" oninput="draft.act[${j}].temp=+this.value">`+sel(WINDS.map((m,i)=>[i,'Fan '+m]),a.wind,`draft.act[${j}].wind=+this.value`);
else if(a.type==='key')f=sel(oth,a.remote,`draft.act[${j}].remote=this.value;loadKeys(this.value).then(drawEditor)`)
+`<select onfocus="loadKeys(draft.act[${j}].remote).then(()=>{})" onchange="pickKey(${j},this.value)"><option value="">${KEYS[a.remote]?'choose button':'tap to load'}</option>${keyOpts(a.remote,a.key_id)}</select>`;
else f=`<input value="${esc(a.text)}" maxlength="60" oninput="draft.act[${j}].text=this.value"><label style="border:0;padding:0">beep <input type="checkbox" ${a.beep?'checked':''} onchange="draft.act[${j}].beep=this.checked"></label>`;
return `<div class="item" style="flex-wrap:wrap">${sel([['ac','Set AC'],['key','Press button'],['notify','Clock notification']],a.type,`draft.act[${j}]=defaultAct(this.value);drawEditor()`)}${f}<button class="sm" onclick="draft.act.splice(${j},1);drawEditor()">✕</button></div>`}).join('');
$('hEdit').innerHTML=`<h2>${editIdx<0?'New rule':'Edit rule'}</h2>
<label>Name <input value="${esc(d.name)}" oninput="draft.name=this.value"></label>
<p class="mute" style="margin:10px 0 0">WHEN all of these are true</p>${conds}<button class="sm" onclick="draft.cond.push({src:'outTemp',op:'>=',val:30});drawEditor()">+ condition</button>
<label style="margin-top:8px">Only between <span><input type="time" value="${d.from||''}" oninput="draft.from=this.value"> – <input type="time" value="${d.to||''}" oninput="draft.to=this.value"></span></label>
<label>Days <span>${DAYN.map((n,i)=>`<label style="display:inline;border:0;padding:0 4px">${n}<input type="checkbox" ${d.days&1<<i?'checked':''} onchange="draft.days^=${1<<i}"></label>`).join('')}</span></label>
<p class="mute" style="margin:10px 0 0">THEN do</p>${acts}<button class="sm" onclick="draft.act.push(defaultAct('notify'));drawEditor()">+ action</button>
<label style="margin-top:8px">Cooldown (minutes before it may fire again) <input type="number" min="1" max="1440" value="${d.cooldown??30}" oninput="draft.cooldown=parseInt(this.value)||30"></label>
<label>Enabled <input type="checkbox" ${d.en?'checked':''} onchange="draft.en=this.checked"></label>
<div class="row"><button class="p" onclick="saveDraft()">Save rule</button><button onclick="$('hEdit').style.display='none'">Cancel</button></div>`}
function pickKey(j,v){const a=draft.act[j],k=(KEYS[a.remote]||[]).find(x=>x.key_id==v);if(k){a.key=k.key;a.key_id=k.key_id;a.name=k.name;a.cat=KEYC[a.remote]}}
async function saveDraft(){if(!draft.from!=!draft.to){toast('Set both times or neither');return}
const a=JSON.parse(JSON.stringify(H.rules||[]));if(editIdx<0)a.push(draft);else a[editIdx]=draft;await saveRules(a);$('hEdit').style.display='none'}
homeLoad();setInterval(homeLoad,5000);
// ---------- recording ----------
const RSL={subs:'Subscribers count',bell:'Bell icon (ringing)',like:'Like button',subscribe:'SUBSCRIBE (flashing)',logo:'Logo (drawing slot)'};
let R={},rOrder=[];
async function recLoad(full){try{R=await (await fetch('/api/rec')).json();
$('recBtn').textContent=R.active?'■ Stop recording':'● Start recording';$('recBtn').style.background=R.active?'#30d158':'';$('recBtn').style.borderColor=R.active?'#30d158':'';
$('recState').textContent=R.active?'Recording mode ON · showing: '+(RSL[R.slide]||R.slide):'On the clock: long-press the middle button to start, short-press middle to stop. Left/right = previous/next slide.';
if(full){rOrder=R.order.split(',').map(x=>x.trim()).filter(x=>RSL[x]);$('recSecs').value=R.secs;$('recBright').value=R.bright;$('recMute').checked=R.mute;$('recLogo').value=R.logoSlot;recItems()}}catch(e){}}
function recItems(){const all=[...rOrder,...Object.keys(RSL).filter(k=>!rOrder.includes(k))];
$('recItems').innerHTML=all.map(k=>{const on=rOrder.includes(k),i=rOrder.indexOf(k);return `<div class="item"><input type="checkbox" ${on?'checked':''} onchange="recTog('${k}',this.checked)"><span class="nm">${RSL[k]}</span>
<button class="sm" ${!on||i===0?'disabled':''} onclick="recMv('${k}',-1)">▲</button><button class="sm" ${!on||i===rOrder.length-1?'disabled':''} onclick="recMv('${k}',1)">▼</button></div>`}).join('')}
function recTog(k,on){rOrder=on?[...rOrder,k]:rOrder.filter(x=>x!==k);recItems()}
function recMv(k,d){const i=rOrder.indexOf(k),j=i+d;[rOrder[i],rOrder[j]]=[rOrder[j],rOrder[i]];recItems()}
async function recSave(){if(!rOrder.length){toast('Pick at least one slide');return}
const r=await post('/api/rec',{order:rOrder.join(','),secs:parseInt($('recSecs').value)||4,bright:parseInt($('recBright').value),mute:$('recMute').checked,logoSlot:parseInt($('recLogo').value)});
toast(r.ok?'Recording settings saved ✓':'Save failed');recLoad(true)}
async function recToggle(){await post(R.active?'/api/rec/stop':'/api/rec/start');await recLoad(false);refresh()}
$('recBright').onchange=e=>post('/api/rec',{bright:parseInt(e.target.value)});
recLoad(true);setInterval(()=>recLoad(false),2000);
function hint(){$('styleHint').textContent=$('dashStyle').value==='1'?'Top: clock · weather · battery. Bottom: stock price (green up / red down) · YouTube subscribers. Values shorten automatically to fit.':'Clock pinned left · scrolling ticker in the middle · battery bar on the right edge.'}
$('dashStyle').onchange=()=>{hint();post('/api/settings',{dashStyle:parseInt($('dashStyle').value)})};
load();refresh();drawScreen();initDraw();setInterval(drawScreen,400);setInterval(refresh,5000);
</script></body></html>)HTML";
