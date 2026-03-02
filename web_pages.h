// ============================================================================
//  Samogon — HTML сторінки (PROGMEM)
// ============================================================================
#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  Головна сторінка — статус + посилання на налаштування
// ═══════════════════════════════════════════════════════════════════════════
const char MAIN_PAGE[] PROGMEM = R"=====(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Samogon</title>
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
<h1>🔗 Samogon</h1>
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
<a href="/update" class="b">🔄 Оновлення</a>
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
.row{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #333}
.row:last-child{border-bottom:none}
.lbl{color:#aaa}.val{color:#4CAF50;font-weight:bold}
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
  var mode = document.querySelector('input[name="mode"]:checked').value;
  var ssid = document.getElementById('ssid').value.trim();
  var pass = document.querySelector('input[name="pass"]').value;
  var data = new URLSearchParams();
  data.append('mode', mode);
  data.append('ssid', ssid);
  data.append('pass', pass);
  fetch('/save_wifi', {method:'POST', body:data})
    .then(r=>r.text())
    .then(t=>{
      showMsg('wmsg','Налаштування збережено!','ok');
    })
    .catch(err=>{
      showMsg('wmsg','Помилка: '+err,'err');
    });
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
  if(cls=='err') setTimeout(function(){m.className='msg';},6000);
}
function restartDevice(){
  if(confirm('Перезавантажити пристрій?')){
    fetch('/restart').then(r=>{showMsg('wmsg','Перезавантаження...','ok');});
  }
}
function resetSettings(){
  if(confirm('Скинути ВСІ налаштування до заводських?')){
    fetch('/factory_reset').then(r=>{showMsg('wmsg','Скинуто. Перезавантаження...','ok');
      setTimeout(function(){location.reload();},5000);});
  }
}
function scanWifi(retry=0){
  document.getElementById('scanBtn').innerText='Сканування...';
  fetch('/scan_wifi').then(r=>r.json()).then(nets=>{
    var sel=document.getElementById('ssid_list');
    sel.innerHTML='<option value="">-- Оберіть мережу --</option>';
    if(nets.length > 0) {
      for(var i=0;i<nets.length;i++){
        sel.innerHTML+='<option value="'+nets[i].ssid+'">'+nets[i].ssid+' ('+nets[i].rssi+' dBm)</option>';
      }
      sel.style.display='block';
      document.getElementById('scanBtn').innerText='🔍 Сканувати';
    } else if(retry < 3) {
      setTimeout(function(){ scanWifi(retry+1); }, 1000);
    } else {
      sel.style.display='block';
      document.getElementById('scanBtn').innerText='🔍 Сканувати';
    }
  }).catch(e=>{
    document.getElementById('scanBtn').innerText='🔍 Сканувати';
  });
}
function selectSSID(){
  var sel=document.getElementById('ssid_list');
  if(sel.value){
    document.getElementById('ssid').value = sel.value.trim();
  }
}
function updateTopicPreview(){
  var tk=document.getElementById('user_token').value.trim();
  var dt=document.getElementById('device_type').value.trim()||'sam';
  var dv=document.getElementById('device_version').value.trim()||'1';
  var pp=document.getElementById('pub_preview');
  var sp=document.getElementById('sub_preview');
  var mt=document.getElementById('manual_topics');
  if(tk.length>0){
    pp.innerText=tk+'/'+dv+'/'+dt+'/data/{key}';
    sp.innerText=tk+'/'+dv+'/'+dt+'/cmd/{key}';
    mt.style.display='none';
  }else{
    pp.innerText=dt+'/data/{key} (вручну)';
    sp.innerText=dt+'/cmd/{key} (вручну)';
    mt.style.display='block';
  }
}
function updateStaIP(){
  fetch('/get_status').then(r=>r.json()).then(d=>{
    document.getElementById('sta_ip_val').innerText = d.wifi_ip || '-';
  });
}
setInterval(updateStaIP, 3000);
window.onload=function(){
  try {
    document.getElementById('WiFi').style.display='block';
    document.querySelector('.tablinks').className+=' active';
    if(typeof updateTopicPreview==='function') updateTopicPreview();
    updateStaIP();
  } catch(e){}
};
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

<label>Пароль WiFi:</label>
<input type="password" id="pass" name="pass" value="%PASS%" placeholder="Пароль">
<br><br>

<label style="color:#4CAF50">IP ESP (STA):</label>
<div id="sta_ip_val" style="padding:10px;background:#333;border-radius:5px;margin:8px 0;color:#2ecc71;font-family:monospace">
%STA_IP%
</div>
<br>

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

<div class="sep"></div>

<label>🏷 Тип пристрою:</label>
<input type="text" name="device_type" id="device_type" value="%DEV_TYPE%" placeholder="sam" oninput="updateTopicPreview()">

<label>📌 Версія протоколу:</label>
<input type="text" name="device_version" id="device_version" value="%DEV_VER%" placeholder="1" oninput="updateTopicPreview()">

<div class="sep"></div>

<label>📱 Номер телефону (User Token):</label>
<input type="text" name="user_token" id="user_token" value="%USER_TOKEN%" placeholder="380991234567" oninput="updateTopicPreview()">
<div class="hint">Вводите номер — топіки побудуються автоматично</div>

<div id="topic_preview" style="margin:10px 0;padding:10px;background:#1a1a2e;border-radius:8px;font-family:monospace;font-size:.85em">
<div style="color:#888;margin-bottom:4px">📤 Публікація:</div>
<div id="pub_preview" style="color:#2ecc71;margin-bottom:8px">—</div>
<div style="color:#888;margin-bottom:4px">📥 Підписка:</div>
<div id="sub_preview" style="color:#f39c12">—</div>
</div>

<div id="manual_topics" style="display:none">
<label>Топік публікації (Serial → MQTT):</label>
<input type="text" name="mqtt_pub_topic" id="mqtt_pub" value="%MQTT_PUB%">

<label>Топік підписки (MQTT → Serial):</label>
<input type="text" name="mqtt_sub_topic" id="mqtt_sub" value="%MQTT_SUB%">
</div>

<div id="mmsg" class="msg"></div>
<div style="margin-top:15px"><button type="submit" class="b">💾 Зберегти MQTT</button></div>
</form>
</div>

<!-- ─── Tab System ─── -->
<div id="System" class="tabcontent">
<div style="margin:10px 0">
<a href="/update" class="b">🔄 Оновлення прошивки</a>
<div class="hint">Завантаження нової прошивки через WiFi або MQTT</div>
</div>
<div class="sep"></div>
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

// ═══════════════════════════════════════════════════════════════════════════
//  Сторінка OTA оновлення прошивки через веб
// ═══════════════════════════════════════════════════════════════════════════
const char OTA_PAGE[] PROGMEM = R"=====(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Оновлення прошивки</title>
<style>
*{box-sizing:border-box}
body{font-family:Arial,sans-serif;background:#121212;color:#eee;margin:0;padding:10px}
.c{max-width:700px;margin:0 auto}
h1{text-align:center;color:#4CAF50}
.d{background:#1e1e1e;border-radius:10px;padding:15px;margin:10px 0}
.hdr{color:#4CAF50;font-size:1.1em;margin:10px 0 8px;font-weight:bold}
.b{display:inline-block;background:#4CAF50;border:0;padding:10px 20px;border-radius:5px;cursor:pointer;color:#fff;text-decoration:none;font-size:1em;margin:5px}
.b:hover{background:#45a049}
.b:disabled{background:#555;cursor:not-allowed}
.hint{color:#666;font-size:.8em;margin:2px 0 8px}
.msg{padding:10px;border-radius:5px;margin:10px 0;display:none}
.msg.ok{background:#1a472a;color:#2ecc71;display:block}
.msg.err{background:#4a1a1a;color:#e74c3c;display:block}
.pbar{width:100%;height:24px;background:#333;border-radius:12px;overflow:hidden;margin:10px 0;display:none}
.pfill{height:100%;background:linear-gradient(90deg,#4CAF50,#45a049);border-radius:12px;transition:width .3s;width:0%;text-align:center;line-height:24px;font-size:.85em;color:#fff;font-weight:bold}
input[type=file]{width:100%;padding:10px;background:#333;color:#eee;border:1px solid #444;border-radius:5px;font-size:1em;margin:5px 0;cursor:pointer}
.finfo{color:#888;font-size:.85em;margin:5px 0}
</style>
<script>
function uploadFW(){
var fi=document.getElementById('fw');
if(!fi.files.length){showMsg('omsg','Оберіть файл прошивки','err');return;}
var file=fi.files[0];
if(!file.name.endsWith('.bin')){showMsg('omsg','Файл має бути .bin','err');return;}
var fd=new FormData();
fd.append('firmware',file);
var xhr=new XMLHttpRequest();
var pb=document.getElementById('pbar');
var pf=document.getElementById('pfill');
pb.style.display='block';
document.getElementById('ubtn').disabled=true;
xhr.upload.addEventListener('progress',function(e){
if(e.lengthComputable){var p=Math.round((e.loaded/e.total)*100);pf.style.width=p+'%';pf.innerText=p+'%';}
});
xhr.onload=function(){
try{var r=JSON.parse(xhr.responseText);
if(r.status==='ok'){showMsg('omsg',r.msg,'ok');pf.style.width='100%';pf.innerText='100%';}
else{showMsg('omsg',r.msg||'Помилка','err');document.getElementById('ubtn').disabled=false;}
}catch(e){showMsg('omsg','Помилка відповіді','err');document.getElementById('ubtn').disabled=false;}
};
xhr.onerror=function(){showMsg('omsg','Помилка з\'єднання','err');document.getElementById('ubtn').disabled=false;};
xhr.open('POST','/api/ota_upload');
xhr.send(fd);
}
function showMsg(id,text,cls){var m=document.getElementById(id);m.innerText=text;m.className='msg '+cls;}
function showFileInfo(){
var fi=document.getElementById('fw');var info=document.getElementById('finfo');
if(fi.files.length){var f=fi.files[0];info.innerText=f.name+' ('+Math.round(f.size/1024)+' KB)';}
else{info.innerText='';}
}
</script>
</head><body>
<div class="c">
<h1>🔄 Оновлення прошивки</h1>
<div class="d">
<div class="hdr">📦 Завантаження через WiFi</div>
<div class="hint">Оберіть .bin файл скомпільованої прошивки</div>
<input type="file" id="fw" accept=".bin" onchange="showFileInfo()">
<div class="finfo" id="finfo"></div>
<div class="pbar" id="pbar"><div class="pfill" id="pfill">0%</div></div>
<div id="omsg" class="msg"></div>
<button class="b" id="ubtn" onclick="uploadFW()">⬆ Завантажити прошивку</button>
</div>
<div class="d">
<div class="hdr">📡 Оновлення через MQTT</div>
<div class="hint">Прошивку також можна відправити через MQTT брокер чанками.</div>
<div class="hint">Топіки: {sub_topic}/ota/begin, /ota/data, /ota/end</div>
<div class="hint">Статус: {pub_topic}/ota/status, /ota/progress</div>
</div>
<div style="text-align:center;margin:15px 0">
<a href="/" class="b">← Назад</a>
<a href="/settings" class="b">⚙ Налаштування</a>
</div>
</div>
</body></html>)=====";

// ═══════════════════════════════════════════════════════════════════════════
//  Сторінка автоматизації
// ═══════════════════════════════════════════════════════════════════════════
const char AUTOMATION_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Автоматика Arduino</title>
  <style>
    body { font-family: Arial, sans-serif; background: #f5f5f5; margin: 0; }
    .header { background: #7a2222; color: #fff; padding: 12px 16px; font-size: 1.3em; display: flex; align-items: center; }
    .header .menu { margin-right: 12px; }
    .status-bar { display: flex; justify-content: space-between; padding: 8px 16px; background: #fff; }
    .status-block { text-align: center; flex: 1; margin: 0 4px; }
    .status-block .label { font-size: 0.9em; color: #888; }
    .status-block .value { font-size: 1.2em; color: #1a1a1a; }
    .controls { background: #fff; margin: 12px 8px; border-radius: 8px; padding: 12px; box-shadow: 0 2px 8px #ddd; }
    .sliders { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
    .slider-block { flex: 1; text-align: center; }
    .slider-block label { display: block; font-size: 0.9em; margin-bottom: 4px; }
    .slider-block input[type=range] { width: 80%; }
    .percent { font-size: 1.1em; margin-bottom: 8px; text-align: center; }
    .checkmark { display: inline-block; background: #eee; border-radius: 8px; padding: 8px 16px; color: #2a7; font-size: 1.2em; cursor: pointer; margin-left: 12px; }
    .main-temp-block { background: #fff; margin: 12px 8px; border-radius: 8px; padding: 16px; box-shadow: 0 2px 8px #ddd; text-align: center; }
    .main-temp { font-size: 2.2em; color: #0055cc; margin: 8px 0; }
    .stop-start { display: flex; justify-content: space-between; margin-bottom: 8px; }
    .stop-start span { font-size: 1.1em; color: #0055cc; }
    .icon-row { display: flex; justify-content: space-between; align-items: center; margin-top: 12px; }
    .icon { font-size: 2em; }
    .icon.valve { color: #a22; }
    .icon.power { color: #1a7a1a; }
    .input-box { width: 60px; padding: 4px; font-size: 1em; margin: 0 8px; }
    .info-row { display: flex; justify-content: space-between; align-items: center; margin: 12px 8px; }
    .info-btn, .refresh-btn { background: #0055cc; color: #fff; border: none; border-radius: 8px; padding: 8px 16px; font-size: 1em; cursor: pointer; }
    .checkbox-row { margin: 12px 8px; }
    .expand-section { background: #fff; margin: 12px 8px; border-radius: 8px; padding: 12px; box-shadow: 0 2px 8px #ddd; }
    .expand-section label { display: block; margin-bottom: 6px; font-size: 1em; }
    .expand-section input[type=checkbox] { margin-right: 8px; }
    .expand-section .input-box { width: 80px; }
    .expand-section .checkmark { margin-left: 8px; }
    .expand-section .info-btn { margin-top: 8px; }
    .expand-section .select-btn { background: #aaa; color: #fff; border: none; border-radius: 8px; padding: 8px 16px; font-size: 1em; cursor: pointer; margin-top: 8px; }
  </style>
</head>
<body>
  <div class="header">
    <span class="menu">&#9776;</span>
    Автоматика Arduino
  </div>
  <div class="status-bar">
    <div class="status-block">
      <div class="label">Температура</div>
      <div class="value" id="mainTemp">23.6°C</div>
    </div>
    <div class="status-block">
      <div class="label">Тиск</div>
      <div class="value" id="pressure">727.1мм</div>
    </div>
    <div class="status-block">
      <div class="label">Автоматика ver4.8</div>
      <div class="value" id="version">23.3°C</div>
    </div>
  </div>
  <div class="controls">
    <div class="percent" id="percent">- 77.0%</div>
    <div class="sliders">
      <div class="slider-block">
        <label>грубо</label>
        <input type="range" min="0" max="100" value="50" id="coarseSlider">
      </div>
      <div class="slider-block">
        <label>точно</label>
        <input type="range" min="0" max="100" value="50" id="fineSlider">
      </div>
      <div class="slider-block">
        <span class="checkmark" id="checkBtn">&#10003;</span>
      </div>
    </div>
  </div>
  <div class="main-temp-block">
    <div class="stop-start">
      <span>Стоп: <span id="stopTemp">33.3°C</span></span>
      <span>Старт: <span id="startTemp">23.3°C</span></span>
    </div>
    <div class="main-temp" id="centerTemp">23.3°C</div>
    <div class="icon-row">
      <span class="icon valve">&#128739;</span>
      <input class="input-box" type="number" value="10" id="valveInput">
      <span class="icon power">&#128722;</span>
    </div>
  </div>
  <div class="info-row">
    <span>Спиртуозність у кубі</span>
    <button class="info-btn">&#9432;</button>
    <button class="refresh-btn">&#8635;</button>
  </div>
  <div class="checkbox-row">
    <input type="checkbox" id="expandChk"> <label for="expandChk">Ще</label>
  </div>
  <div class="expand-section" id="expandSection" style="display:none;">
    <label><input type="checkbox" checked> Спиртуозність у кубі</label>
    <label><input type="checkbox" checked> Швидкість відбирання</label>
    <div style="display:flex; align-items:center; margin-bottom:8px;">
      <button class="refresh-btn">&#8635;</button>
      <input class="input-box" type="number" placeholder="Об'єм, мл.">
      <button class="select-btn">Старт</button>
    </div>
    <label><input type="checkbox" checked> Сигналізація температури куба</label>
    <div style="display:flex; align-items:center; margin-bottom:8px;">
      <span class="icon">&#128276;</span>
      <input class="input-box" type="number" value="110.00">
      <span class="checkmark">&#10003;</span>
    </div>
    <label><input type="checkbox" checked> Змінити T° Старт-Стоп</label>
    <div style="display:flex; align-items:center; margin-bottom:8px;">
      <button class="refresh-btn">&#8635;</button>
      <input class="input-box" type="number" value="24">
      <span class="checkmark">&#10003;</span>
    </div>
    <label><input type="checkbox" checked> Центральна позиція дисплея</label>
    <button class="info-btn">&#9432;</button>
    <button class="select-btn">Вибрати</button>
  </div>
  <script>
    document.getElementById('expandChk').addEventListener('change', function() {
      document.getElementById('expandSection').style.display = this.checked ? 'block' : 'none';
    });
  </script>
</body>
</html>
)HTML";