// data/script.js
const intervals = [1,5,10,15,20,30,60];
let currentWeek = null;
let chart = null;

function humanBytes(b) {
  if (b < 1024) return b + ' B';
  if (b < 1024*1024) return (b/1024).toFixed(1) + ' KB';
  return (b/1024/1024).toFixed(2) + ' MB';
}

async function fetchJSON(path) {
  const r = await fetch(path);
  return await r.json();
}

async function refreshStorage() {
  const info = await fetchJSON('/api/storageinfo');
  document.getElementById('storage').innerText = `Speicher: ${humanBytes(info.used_bytes)} von ${humanBytes(info.total_bytes)} (~${info.percent}%)`;
  // populate interval select with estimated weeks
  const sel = document.getElementById('intervalSelect');
  sel.innerHTML = '';
  intervals.forEach(i => {
    const weeks = info.weeks_possible_for_interval[String(i)];
    const opt = document.createElement('option');
    opt.value = i;
    opt.text = `${i} min (~${weeks} Wochen)`;
    sel.appendChild(opt);
  });
}

async function listWeeks() {
  const arr = await fetchJSON('/api/weeks');
  const div = document.getElementById('weeksList');
  div.innerHTML = '<b>Vorhandene Wochen</b> (Auswählen/anklicken zum Anzeigen)<br>';
  if (arr.length === 0) div.innerHTML += 'keine Daten';
  arr.forEach(w => {
    const a = document.createElement('a');
    a.href = '#';
    a.innerText = w;
    a.onclick = (e) => { e.preventDefault(); loadWeek(w); return false; };
    div.appendChild(a);
    div.appendChild(document.createTextNode(' '));
  });
}

async function loadWeek(week) {
  currentWeek = week;
  const url = `/api/download_week?week=${encodeURIComponent(week)}`;
  const r = await fetch(url);
  if (!r.ok) { alert('Fehler beim Laden'); return; }
  const csv = await r.text();
  const lines = csv.trim().split('\n');
  const labels = [];
  const temps = [];
  const hums = [];
  for (let i=0;i<lines.length;i++) {
    const parts = lines[i].split(';');
    if (parts.length<3) continue;
    const ts = parseInt(parts[0])*1000;
    labels.push(new Date(ts).toLocaleString());
    temps.push(parseFloat(parts[1]));
    hums.push(parseFloat(parts[2]));
  }
  drawChart(labels, temps, hums);
  // Update last measured value
  displayLatestMeasurement();
}

function drawChart(labels, temps, hums) {
    const ctx = document.getElementById('chart').getContext('2d');

    // vorhandenen Chart zerstören
    if (chart) chart.destroy();

    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [
                {
                    label: 'Temperature (°C)',
                    data: temps,
                    borderColor: 'red',
                    fill: false,
                    yAxisID: 'yTemp'
                },
                {
                    label: 'Humidity (%)',
                    data: hums,
                    borderColor: 'blue',
                    fill: false,
                    yAxisID: 'yHum'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false, // damit div height genutzt wird
            scales: {
                x: {
                    display: true,
                    title: { display: true, text: 'Zeit' }
                },
                yTemp: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: { display: true, text: '°C' }
                },
                yHum: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: { display: true, text: '%' },
                    grid: { drawOnChartArea: false } // damit Linien sich nicht überlagern
                }
            }
        }
    });
}

async function downloadCurrentWeek() {
  if (!currentWeek) { alert('Keine Woche gewählt'); return; }
  const url = `/api/download_week?week=${encodeURIComponent(currentWeek)}`;
  const a = document.createElement('a');
  a.href = url;
  a.download = currentWeek;
  document.body.appendChild(a);
  a.click();
  a.remove();
}

async function downloadAllWeekZip() {
  // Get list of weeks
  const weeks = await fetchJSON('/api/download_all');
  if (!weeks || weeks.length==0) { alert('Keine Daten'); return; }
  const zip = new JSZip();
  // fetch each CSV
  for (let i=0;i<weeks.length;i++) {
    const w = weeks[i];
    const r = await fetch(`/api/download_week?week=${encodeURIComponent(w)}`);
    if (!r.ok) continue;
    const txt = await r.text();
    zip.file(w, txt);
  }
  const content = await zip.generateAsync({type:"blob"});
  const url = URL.createObjectURL(content);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'all_weeks.zip';
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

async function deleteAll() {
  if (!confirm('Alle Daten löschen? Diese Aktion ist endgültig.')) return;
  const pw = prompt('Geben Sie das Admin-Passwort ein:');
  if (!pw) return;
  const response = await fetch('/api/delete_all', {
    method: 'POST',
    headers: { 'Authorization': pw }
  });

  if (!response.ok) { // Prüft auf Statuscodes 200-299
      // Liest die Fehlermeldung vom Server (z.B. "forbidden")
      const errorText = await response.text(); 
      alert(`Löschen fehlgeschlagen: ${errorText}`);
      return; // Abbruch bei Fehler
  }

  await refreshStorage();
  await listWeeks();
}

async function deletePrev() {
  if (!currentWeek) { alert('Wähle zuerst unter \"Vorhandene Wochen\" eine Woche aus'); return; }
  if (!confirm('Alle Wochen vor der ausgewählten löschen?')) return;
  const pw = prompt('Geben Sie das Admin-Passwort ein:');
  if (!pw) return;
  const response = await fetch(`/api/delete_prev?current=${encodeURIComponent(currentWeek)}`, {
    method:'POST', 
    headers:{'Authorization': pw}
  });

  await refreshStorage();
  await listWeeks();
}

// Update the UI based on measurement status
function updateMeasurementUI(isOn) {
  const btn = document.getElementById('btnToggleMeasurement');
  const status = document.getElementById('measureStatus');

  if (isOn) {
    btn.innerText = 'Messung stoppen';
    status.innerText = 'Messung läuft';
    status.style.color = 'green';
    status.classList.add('status-active');
    status.classList.remove('status-inactive');
  } else {
    btn.innerText = 'Messung starten';
    status.innerText = 'Messung gestoppt';
    status.style.color = 'red';
    status.classList.remove('status-active');
    status.classList.add('status-inactive');
  }
}

// Fetch current measurement status from server and update UI
async function refreshMeasurementState() {
  const r = await fetch('/api/status');
  const js = await r.json();

  updateMeasurementUI(js.measurementActive);

  // Intervall im Dropdown setzen
  document.getElementById('intervalSelect').value = js.interval;

  return js.measurementActive;
}

// Toggle measurement on/off on server and update UI
async function toggleMeasurement() {
  const r = await fetch('/api/toggleMeasurement', { method: 'POST' });
  const js = await r.json();

  updateMeasurementUI(js.measurementActive);
  await refreshStorage();
  await listWeeks();
  location.reload();
}

async function flushNow() {
  try {
    const res = await fetch('/api/flush', { method: 'POST' });
    if (!res.ok) { alert('Flush fehlgeschlagen'); return; }
    const json = await res.json();
    if (json.status === 'ok') alert('Buffer gespeichert');
  } catch (e) {
    alert('Netzwerkfehler');
  }
  location.reload();
}

async function applyInterval() {
  const sel = document.getElementById('intervalSelect');
  const val = parseInt(sel.value);

  await fetch('/api/set_interval', {
    method: 'POST',
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ interval: val })
  });

  alert("Messintervall gespeichert");
  location.reload();
}

async function displayLatestMeasurement() {
  try {
    const r = await fetch('/api/latestMeasurement');
    if (!r.ok) return;

    const js = await r.json();
    const span = document.getElementById('live');
    const timeStr = new Date(js.ts * 1000).toLocaleTimeString();
    span.innerText = `${js.temp.toFixed(1)} °C, ${js.hum.toFixed(1)} % (${timeStr})`;
  } catch (e) {
    console.error('Failed to fetch latest measurement', e);
  }
}


document.addEventListener('DOMContentLoaded', async () => {
  await refreshStorage();
  await listWeeks();
  await refreshMeasurementState();

  document.getElementById('btnToggleMeasurement').addEventListener('click', toggleMeasurement);
  document.getElementById('btnApplyInterval').addEventListener('click', applyInterval);
  document.getElementById('downloadWeek').addEventListener('click', downloadCurrentWeek);
  document.getElementById('downloadAllWeek').addEventListener('click', downloadAllWeekZip);
  document.getElementById('btnFlushBuffer').addEventListener('click', flushNow);
  document.getElementById('deletePrev').addEventListener('click', deletePrev);
  document.getElementById('deleteAll').addEventListener('click', deleteAll);

  displayLatestMeasurement();
});
