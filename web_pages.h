// ============================================================================
//  MQTT Bridge — HTML сторінки (PROGMEM)
// ============================================================================
#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  Головна сторінка — статус + посилання на налаштування
// ═══════════════════════════════════════════════════════════════════════════
const char MAIN_PAGE[] PROGMEM = R"=====(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MQTT Bridge</title>
<style>
*{box-sizing:border-box}
body{font-family:Arial,sans-serif;background:#121212;color:#eee;margin:0;padding:10px}
.c{max-width:700px;margin:0 auto}
h1{text-align:center;color:#4CAF50;margin-bottom:5px}
.sub{text-align:center;color:#888;font-size:.85em;margin-bottom:15px}
.d{background:#1e1e1e;border-radius:10px;padding:15px;margin:10px 0}
.row{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #333}
.row:last-child{border-bottom:none}
.lbl{color:#aaa}.val{color:#4CAF50;font-weight:bold}
.val.err{color:#ff5555}
.b{display:inline-block;background:#4CAF50;border:0;padding:10px 20px;border-radius:5px;cursor:pointer;color:#fff;text-decoration:none;font-size:1em;margin:5px}
.b:hover{background:#45a049}
.b.red{background:#c0392b}.b.red:hover{background:#e74c3c}
.hdr{color:#4CAF50;font-size:1.1em;margin:10px 0 5px;font-weight:bold}
.mqtt-data{background:#1a1a2e;border-radius:8px;padding:10px;margin:8px 0;max-height:200px;overflow-y:auto;font-family:monospace;font-size:.9em}
.mqtt-item{padding:2px 0;color:#ddd}
.mqtt-key{color:#f39c12}.mqtt-val{color:#2ecc71}
</style>
<script>
function uD(){
fetch('/get_status').then(r=>r.json()).then(d=>{
document.getElementById('wifi_st').innerText=d.wifi_connected?'Підключено':'Відключено';
document.getElementById('wifi_st').className='val'+(d.wifi_connected?'':' err');
document.getElementById('wifi_ip').innerText=d.wifi_ip||'-';
document.getElementById('wifi_rssi').innerText=d.wifi_rssi?d.wifi_rssi+' dBm':'-';
document.getElementById('wifi_ssid').innerText=d.wifi_ssid||'-';
document.getElementById('mqtt_st').innerText=d.mqtt_connected?'Підключено':'Відключено';
document.getElementById('mqtt_st').className='val'+(d.mqtt_connected?'':' err');
document.getElementById('mqtt_srv').innerText=d.mqtt_server||'-';
document.getElementById('serial_raw').innerText=d.serial_last||'немає даних';
var sd=document.getElementById('serial_data');
sd.innerHTML='';
if(d.serial_keys&&d.serial_keys.length>0){
for(var i=0;i<d.serial_keys.length;i++){
sd.innerHTML+='<div class="mqtt-item"><span class="mqtt-key">'+d.serial_keys[i]+'</span> = <span class="mqtt-val">'+d.serial_values[i]+'</span></div>';
}}else{sd.innerHTML='<div class="mqtt-item" style="color:#888">очікування даних...</div>';}
document.getElementById('uptime').innerText=formatUptime(d.uptime);
}).catch(e=>{
document.getElementById('wifi_st').innerText='???';
document.getElementById('wifi_st').className='val err';
});}
function formatUptime(s){if(!s&&s!==0)return'-';var d=Math.floor(s/86400);var h=Math.floor((s%86400)/3600);var m=Math.floor((s%3600)/60);var sec=s%60;var r='';if(d>0)r+=d+'д ';r+=h+'г '+m+'хв '+sec+'с';return r;}
setInterval(uD,3000);
window.onload=uD;
</script>
</head><body>
<div class="c">
<h1>🔗 MQTT Bridge</h1>
<div class="sub">Serial ↔ MQTT Gateway</div>

<div class="d">
<div class="hdr">📶 WiFi</div>
<div class="row"><span class="lbl">Статус:</span><span id="wifi_st" class="val">---</span></div>
<div class="row"><span class="lbl">SSID:</span><span id="wifi_ssid" class="val">---</span></div>
<div class="row"><span class="lbl">IP:</span><span id="wifi_ip" class="val">---</span></div>
<div class="row"><span class="lbl">Сигнал:</span><span id="wifi_rssi" class="val">---</span></div>
</div>

<div class="d">
<div class="hdr">📡 MQTT</div>
<div class="row"><span class="lbl">Статус:</span><span id="mqtt_st" class="val">---</span></div>
<div class="row"><span class="lbl">Сервер:</span><span id="mqtt_srv" class="val">---</span></div>
</div>

<div class="d">
<div class="hdr">📟 Serial дані</div>
<div class="row"><span class="lbl">Raw:</span><span id="serial_raw" class="val" style="font-family:monospace;font-size:.85em;word-break:break-all">---</span></div>
<div class="mqtt-data" id="serial_data"><div class="mqtt-item" style="color:#888">очікування даних...</div></div>
</div>

<div class="d">
<div class="row"><span class="lbl">Час роботи:</span><span id="uptime" class="val">---</span></div>
</div>

<div style="text-align:center;margin:15px 0">
<a href="/settings" class="b">⚙ Налаштування</a>
<a href="/send" class="b">📤 Відправити</a>
</div>
</div>
</body></html>)=====";

// ═══════════════════════════════════════════════════════════════════════════
//  Сторінка відправки Serial / MQTT команд
// ═══════════════════════════════════════════════════════════════════════════
const char SEND_PAGE[] PROGMEM = R"=====(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Відправити</title>
<style>
*{box-sizing:border-box}
body{font-family:Arial,sans-serif;background:#121212;color:#eee;margin:0;padding:10px}
.c{max-width:700px;margin:0 auto}
h1{text-align:center;color:#4CAF50}
.d{background:#1e1e1e;border-radius:10px;padding:15px;margin:10px 0}
.hdr{color:#4CAF50;font-size:1.1em;margin:10px 0 8px;font-weight:bold}
input[type=text],textarea{width:100%;padding:10px;background:#333;color:#eee;border:1px solid #444;border-radius:5px;font-size:1em;margin:5px 0}
textarea{height:60px;resize:vertical;font-family:monospace}
.b{display:inline-block;background:#4CAF50;border:0;padding:10px 20px;border-radius:5px;cursor:pointer;color:#fff;text-decoration:none;font-size:1em;margin:5px}
.b:hover{background:#45a049}
.log{background:#0a0a0a;border-radius:5px;padding:10px;margin:8px 0;font-family:monospace;font-size:.85em;max-height:150px;overflow-y:auto;color:#888}
.ok{color:#2ecc71}.err{color:#e74c3c}
</style>
<script>
function sendSerial(){
var cmd=document.getElementById('scmd').value;
if(!cmd)return;
fetch('/api/serial_send?cmd='+encodeURIComponent(cmd)).then(r=>r.text()).then(t=>{
addLog('Serial TX: '+cmd,'ok');
document.getElementById('scmd').value='';
}).catch(e=>addLog('Помилка: '+e,'err'));
}
function sendMqtt(){
var t=document.getElementById('mtopic').value;
var p=document.getElementById('mpayload').value;
if(!t)return;
fetch('/api/mqtt_pub?topic='+encodeURIComponent(t)+'&payload='+encodeURIComponent(p)).then(r=>r.text()).then(txt=>{
addLog('MQTT PUB: '+t+' = '+p,'ok');
}).catch(e=>addLog('Помилка: '+e,'err'));
}
function addLog(msg,cls){
var l=document.getElementById('log');
l.innerHTML='<div class="'+cls+'">'+msg+'</div>'+l.innerHTML;
}
</script>
</head><body>
<div class="c">
<h1>📤 Відправити</h1>

<div class="d">
<div class="hdr">📟 Serial команда</div>
<input type="text" id="scmd" placeholder="Введіть команду для Serial...">
<button class="b" onclick="sendSerial()">Відправити в Serial</button>
</div>

<div class="d">
<div class="hdr">📡 MQTT публікація</div>
<input type="text" id="mtopic" placeholder="Топік (наприклад bridge/cmd/power)">
<input type="text" id="mpayload" placeholder="Payload (значення)">
<button class="b" onclick="sendMqtt()">Публікувати в MQTT</button>
</div>

<div class="d">
<div class="hdr">📋 Лог</div>
<div class="log" id="log"><div style="color:#555">тут будуть відповіді...</div></div>
</div>

<div style="text-align:center;margin:15px 0">
<a href="/" class="b">← Назад</a>
</div>
</div>
</body></html>)=====";

// ═══════════════════════════════════════════════════════════════════════════
//  Сторінка налаштувань (WiFi + MQTT) з табами
// ═══════════════════════════════════════════════════════════════════════════
const char SETTINGS_PAGE[] PROGMEM = R"=====(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Налаштування</title>
<style>
*{box-sizing:border-box}
body{font-family:Arial,sans-serif;background:#121212;color:#eee;margin:0;padding:10px}
.c{max-width:700px;margin:0 auto}
h1{text-align:center;color:#4CAF50}
.d{background:#1e1e1e;border-radius:10px;padding:15px;margin:10px 0}
.tab{overflow:hidden;border:1px solid #444;background:#1e1e1e;border-radius:5px 5px 0 0;display:flex}
.tab button{flex:1;background:inherit;border:none;outline:none;cursor:pointer;padding:14px 10px;color:#eee;font-size:1em;transition:.3s}
.tab button:hover{background:#333}
.tab button.active{background:#4CAF50;color:#fff}
.tabcontent{display:none;padding:15px;border:1px solid #444;border-top:none;background:#1e1e1e;border-radius:0 0 5px 5px}
label{display:block;margin:8px 0 3px;color:#aaa;font-size:.95em}
input[type=text],input[type=number],input[type=password]{width:100%;padding:10px;background:#333;color:#eee;border:1px solid #444;border-radius:5px;font-size:1em}
input[type=radio]{margin-right:5px}
.radio-group{margin:8px 0}
.radio-group label{display:inline-flex;align-items:center;margin-right:15px;color:#eee;cursor:pointer}
.b{display:inline-block;background:#4CAF50;border:0;padding:10px 20px;border-radius:5px;cursor:pointer;color:#fff;text-decoration:none;font-size:1em;margin:5px}
.b:hover{background:#45a049}
.b.red{background:#c0392b}.b.red:hover{background:#e74c3c}
.sep{border-top:1px solid #333;margin:20px 0}
.msg{padding:10px;border-radius:5px;margin:10px 0;display:none}
.msg.ok{background:#1a472a;color:#2ecc71;display:block}
.msg.err{background:#4a1a1a;color:#e74c3c;display:block}
.hint{color:#666;font-size:.8em;margin:2px 0 8px}
</style>
<script>
function openTab(evt,name){
var tc=document.getElementsByClassName('tabcontent');
for(var i=0;i<tc.length;i++)tc[i].style.display='none';
var tl=document.getElementsByClassName('tablinks');
for(var i=0;i<tl.length;i++)tl[i].className=tl[i].className.replace(' active','');
document.getElementById(name).style.display='block';
evt.currentTarget.className+=' active';
}
function saveWifi(e){
e.preventDefault();
var f=new FormData(document.getElementById('wifiForm'));
fetch('/save_wifi',{method:'POST',body:new URLSearchParams(f)}).then(r=>r.text()).then(t=>{
showMsg('wmsg','Налаштування WiFi збережено! Перепідключення...','ok');
setTimeout(function(){location.reload();},5000);
}).catch(err=>showMsg('wmsg','Помилка: '+err,'err'));
}
function saveMqtt(e){
e.preventDefault();
var f=new FormData(document.getElementById('mqttForm'));
fetch('/save_mqtt',{method:'POST',body:new URLSearchParams(f)}).then(r=>r.text()).then(t=>{
showMsg('mmsg','Налаштування MQTT збережено!','ok');
}).catch(err=>showMsg('mmsg','Помилка: '+err,'err'));
}
function showMsg(id,text,cls){
var m=document.getElementById(id);
m.innerText=text;m.className='msg '+cls;
setTimeout(function(){m.className='msg';},6000);
}
function restartDevice(){
if(confirm('Перезавантажити пристрій?')){
fetch('/restart').then(r=>{showMsg('wmsg','Перезавантаження...','ok');});
}}
function resetSettings(){
if(confirm('Скинути ВСІ налаштування до заводських?')){
fetch('/factory_reset').then(r=>{showMsg('wmsg','Скинуто. Перезавантаження...','ok');
setTimeout(function(){location.reload();},5000);});
}}
function scanWifi(){
document.getElementById('scanBtn').innerText='Сканування...';
fetch('/scan_wifi').then(r=>r.json()).then(nets=>{
var sel=document.getElementById('ssid_list');
sel.innerHTML='<option value="">-- Оберіть мережу --</option>';
for(var i=0;i<nets.length;i++){
sel.innerHTML+='<option value="'+nets[i].ssid+'">'+nets[i].ssid+' ('+nets[i].rssi+' dBm)</option>';
}
sel.style.display='block';
document.getElementById('scanBtn').innerText='🔍 Сканувати';
}).catch(e=>{document.getElementById('scanBtn').innerText='🔍 Сканувати';});
}
function selectSSID(){
var sel=document.getElementById('ssid_list');
if(sel.value){document.getElementById('ssid').value=sel.value;}
}
window.onload=function(){document.getElementById('WiFi').style.display='block';
document.querySelector('.tablinks').className+=' active';};
</script>
</head><body>
<div class="c">
<h1>⚙ Налаштування</h1>
<div class="d">
<div class="tab">
<button class="tablinks" onclick="openTab(event,'WiFi')">📶 WiFi</button>
<button class="tablinks" onclick="openTab(event,'MQTT')">📡 MQTT</button>
<button class="tablinks" onclick="openTab(event,'System')">🔧 Система</button>
</div>

<!-- ─── Tab WiFi ─── -->
<div id="WiFi" class="tabcontent">
<form id="wifiForm" onsubmit="saveWifi(event)">
<div class="radio-group">
<label><input type="radio" name="mode" value="router" %s_router%> Роутер (STA)</label>
<label><input type="radio" name="mode" value="hotspot" %s_hotspot%> Точка доступу (AP)</label>
</div>

<label>SSID:</label>
<input type="text" name="ssid" id="ssid" value="%SSID%">
<select id="ssid_list" onchange="selectSSID()" style="display:none;width:100%;padding:8px;margin:5px 0;background:#333;color:#eee;border:1px solid #444;border-radius:5px"></select>
<button type="button" id="scanBtn" class="b" onclick="scanWifi()" style="font-size:.85em;padding:6px 12px;margin:3px 0">🔍 Сканувати</button>

<label>Пароль:</label>
<input type="text" name="pass" value="%PASS%">

<div id="wmsg" class="msg"></div>
<div style="margin-top:15px"><button type="submit" class="b">💾 Зберегти WiFi</button></div>
</form>
</div>

<!-- ─── Tab MQTT ─── -->
<div id="MQTT" class="tabcontent">
<form id="mqttForm" onsubmit="saveMqtt(event)">
<label>Сервер:</label>
<input type="text" name="mqtt_server" value="%MQTT_SRV%">
<div class="hint">IP або домен MQTT брокера</div>

<label>Порт:</label>
<input type="number" name="mqtt_port" value="%MQTT_PORT%">

<label>Користувач:</label>
<input type="text" name="mqtt_user" value="%MQTT_USER%">

<label>Пароль:</label>
<input type="password" name="mqtt_pass" value="%MQTT_PASS%">

<label>Client ID:</label>
<input type="text" name="mqtt_client_id" value="%MQTT_CID%">
<div class="hint">Унікальний ідентифікатор пристрою</div>

<label>Топік публікації (Serial → MQTT):</label>
<input type="text" name="mqtt_pub_topic" value="%MQTT_PUB%">
<div class="hint">Дані з Serial публікуються в {топік}/{ключ}</div>

<label>Топік підписки (MQTT → Serial):</label>
<input type="text" name="mqtt_sub_topic" value="%MQTT_SUB%">
<div class="hint">Повідомлення з {топік}/# пересилаються в Serial</div>

<div id="mmsg" class="msg"></div>
<div style="margin-top:15px"><button type="submit" class="b">💾 Зберегти MQTT</button></div>
</form>
</div>

<!-- ─── Tab System ─── -->
<div id="System" class="tabcontent">
<div style="margin:10px 0">
<button class="b red" onclick="restartDevice()">🔄 Перезавантаження</button>
</div>
<div class="sep"></div>
<div style="margin:10px 0">
<button class="b red" onclick="resetSettings()">🗑 Скинути налаштування</button>
<div class="hint">Видаляє всі збережені WiFi та MQTT налаштування</div>
</div>
</div>

</div>
<div style="text-align:center;margin:15px 0">
<a href="/" class="b">← Назад</a>
</div>
</div>
</body></html>)=====";

