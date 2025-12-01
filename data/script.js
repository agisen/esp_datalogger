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
  div.innerHTML = '<b>Vorhandene Wochen</b> (anklicken zum Anzeigen)<br>';
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

async function downloadAllZip() {
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
  await fetch('/api/delete_all', {method:'POST', headers:{'X-Auth': pw}});
  await refreshStorage();
  await listWeeks();
}

async function deletePrev() {
  if (!currentWeek) { alert('Wähle zuerst eine Woche'); return; }
  if (!confirm('Alle Wochen vor der aktuellen löschen?')) return;
  const pw = prompt('Geben Sie das Admin-Passwort ein:');
  if (!pw) return;
  await fetch(`/api/delete_prev?current=${encodeURIComponent(currentWeek)}`, {method:'POST', headers:{'X-Auth': pw}});
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
  return js.measurementActive;
}

// Toggle measurement on/off on server and update UI
async function toggleMeasurement() {
  const r = await fetch('/api/toggleMeasurement', { method: 'POST' });
  const js = await r.json();

  updateMeasurementUI(js.measurementActive);
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
}

document.addEventListener('DOMContentLoaded', async () => {
  await refreshStorage();
  await listWeeks();
  await refreshMeasurementState();

  document.getElementById('downloadWeek').addEventListener('click', downloadCurrentWeek);
  document.getElementById('downloadAll').addEventListener('click', downloadAllZip);
  document.getElementById('deletePrev').addEventListener('click', deletePrev);
  document.getElementById('deleteAll').addEventListener('click', deleteAll);
  document.getElementById('btnToggleMeasurement').addEventListener('click', toggleMeasurement);
  document.getElementById('btnFlushBuffer').addEventListener('click', flushNow);
});
