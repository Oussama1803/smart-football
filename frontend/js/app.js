/**
 * Smart Football Dashboard Client Logic
 * Handles real-time polling, analytics computation, session comparison,
 * and interactive goal target visualization.
 */

let autoRefreshTimer = null;
let currentSessionFilter = "";
let is9ZoneMode = false;
let sessionsCache = [];

document.addEventListener("DOMContentLoaded", () => {
  initEventListeners();
  loadAllData();
  setupAutoRefresh();
});

function initEventListeners() {
  const sessionSelect = document.getElementById("session-select");
  sessionSelect.addEventListener("change", (e) => {
    currentSessionFilter = e.target.value;
    loadStats();
    loadShots();
  });

  document.getElementById("btn-refresh").addEventListener("click", () => {
    loadAllData();
  });

  const autoSelect = document.getElementById("auto-refresh-select");
  autoSelect.addEventListener("change", () => {
    setupAutoRefresh();
  });

  const btnToggleZones = document.getElementById("btn-toggle-zones");
  btnToggleZones.addEventListener("click", () => {
    is9ZoneMode = !is9ZoneMode;
    btnToggleZones.textContent = is9ZoneMode ? "Vue 9 zones (Extension)" : "Vue 4 zones (Prototype)";
    document.getElementById("goal-active-mode").textContent = is9ZoneMode ? "9 Zones (Extension)" : "4 Zones";
    renderGoalGrid();
    loadStats();
  });

  document.getElementById("comp-session-a").addEventListener("change", updateComparison);
  document.getElementById("comp-session-b").addEventListener("change", updateComparison);
}

function setupAutoRefresh() {
  if (autoRefreshTimer) {
    clearInterval(autoRefreshTimer);
    autoRefreshTimer = null;
  }
  const intervalMs = parseInt(document.getElementById("auto-refresh-select").value, 10);
  if (intervalMs > 0) {
    autoRefreshTimer = setInterval(() => {
      loadStats();
      loadShots();
      checkHealth();
      loadDevices();
      loadKeeperData();
    }, intervalMs);
  }
}

function loadAllData() {
  checkHealth();
  loadSessions();
  loadStats();
  loadShots();
  loadDevices();
  loadKeeperData();
}

/**
 * Health check: verify API, DB, and MQTT connectivity.
 */
async function checkHealth() {
  try {
    const res = await fetch("/health");
    if (!res.ok) throw new Error("API response not ok");
    const data = await res.json();

    const apiDot = document.getElementById("status-api-dot");
    const apiText = document.getElementById("status-api-text");
    apiDot.className = "dot online";
    apiText.textContent = "Opérationnel";

    const dbDot = document.getElementById("status-db-dot");
    const dbText = document.getElementById("status-db-text");
    if (data.database === "connected") {
      dbDot.className = "dot online";
      dbText.textContent = "Connecté";
    } else {
      dbDot.className = "dot offline";
      dbText.textContent = "Erreur";
    }

    const mqttDot = document.getElementById("status-mqtt-dot");
    const mqttText = document.getElementById("status-mqtt-text");
    if (data.mqtt_connected) {
      mqttDot.className = "dot online";
      mqttText.textContent = "Connecté";
    } else {
      mqttDot.className = "dot online";
      mqttText.textContent = "Actif";
    }
  } catch (err) {
    document.getElementById("status-api-dot").className = "dot offline";
    document.getElementById("status-api-text").textContent = "Indisponible";
    document.getElementById("status-db-dot").className = "dot offline";
    document.getElementById("status-db-text").textContent = "Inconnu";
  }
}

/**
 * Load training sessions for filtering and comparison.
 */
async function loadSessions() {
  try {
    const res = await fetch("/api/sessions");
    if (!res.ok) return;
    sessionsCache = await res.json();

    const sessionSelect = document.getElementById("session-select");
    const compA = document.getElementById("comp-session-a");
    const compB = document.getElementById("comp-session-b");

    const previousVal = sessionSelect.value;
    sessionSelect.innerHTML = '<option value="">Toutes les séances</option>';
    compA.innerHTML = '<option value="">Choisir Séance A...</option>';
    compB.innerHTML = '<option value="">Choisir Séance B...</option>';

    sessionsCache.forEach((s) => {
      const opt = document.createElement("option");
      opt.value = s.session_id;
      opt.textContent = `${s.session_id} (${s.total_shots} tirs)`;
      sessionSelect.appendChild(opt);

      const optA = opt.cloneNode(true);
      const optB = opt.cloneNode(true);
      compA.appendChild(optA);
      compB.appendChild(optB);
    });

    if (previousVal) sessionSelect.value = previousVal;

    // Default comparison selection if available
    if (sessionsCache.length >= 2) {
      if (!compA.value) compA.value = sessionsCache[0].session_id;
      if (!compB.value) compB.value = sessionsCache[1].session_id;
      updateComparison();
    }
  } catch (err) {
    console.error("Error loading sessions:", err);
  }
}

/**
 * Load aggregated statistics and update KPI cards & goal heatmap.
 */
async function loadStats() {
  try {
    const url = currentSessionFilter ? `/api/stats?session_id=${encodeURIComponent(currentSessionFilter)}` : "/api/stats";
    const res = await fetch(url);
    if (!res.ok) return;
    const stats = await res.json();

    // Update KPIs
    document.getElementById("kpi-total-shots").textContent = stats.total_shots;
    document.getElementById("kpi-shots-on-target").textContent =
      `${stats.shots_on_target} cadrés / ${stats.missed_shots} hors-cadre`;
    document.getElementById("kpi-accuracy").textContent = stats.accuracy_rate_pct.toFixed(1);
    document.getElementById("kpi-avg-speed").textContent = stats.avg_speed_kmh.toFixed(1);
    document.getElementById("kpi-max-speed").textContent = stats.max_speed_kmh.toFixed(1);
    document.getElementById("kpi-avg-pressure").textContent = Math.round(stats.avg_pressure_reading);
    document.getElementById("kpi-avg-impact").textContent = Math.round(stats.avg_impact_intensity);

    // Update Goal Visualizer
    updateGoalHeatmap(stats.zone_distribution, stats.total_shots);
  } catch (err) {
    console.error("Error loading stats:", err);
  }
}

/**
 * Render Goal grid layout (4 zones or 9 zones).
 */
function renderGoalGrid() {
  const goalGrid = document.getElementById("goal-grid");
  if (!is9ZoneMode) {
    goalGrid.style.gridTemplateColumns = "1fr 1fr";
    goalGrid.style.gridTemplateRows = "1fr 1fr";
    goalGrid.innerHTML = `
      <div class="goal-zone" id="zone-upper_left" data-zone="upper_left">
        <div class="zone-label">Haut Gauche</div>
        <div class="zone-hits" id="hits-upper_left">0</div>
        <div class="zone-pct" id="pct-upper_left">0%</div>
      </div>
      <div class="goal-zone" id="zone-upper_right" data-zone="upper_right">
        <div class="zone-label">Haut Droit</div>
        <div class="zone-hits" id="hits-upper_right">0</div>
        <div class="zone-pct" id="pct-upper_right">0%</div>
      </div>
      <div class="goal-zone" id="zone-lower_left" data-zone="lower_left">
        <div class="zone-label">Bas Gauche</div>
        <div class="zone-hits" id="hits-lower_left">0</div>
        <div class="zone-pct" id="pct-lower_left">0%</div>
      </div>
      <div class="goal-zone" id="zone-lower_right" data-zone="lower_right">
        <div class="zone-label">Bas Droit</div>
        <div class="zone-hits" id="hits-lower_right">0</div>
        <div class="zone-pct" id="pct-lower_right">0%</div>
      </div>
    `;
  } else {
    // 9 zones layout: 3 columns x 3 rows
    goalGrid.style.gridTemplateColumns = "1fr 1fr 1fr";
    goalGrid.style.gridTemplateRows = "1fr 1fr 1fr";
    goalGrid.innerHTML = `
      <div class="goal-zone" id="zone-upper_left" data-zone="upper_left"><div class="zone-label">Haut G.</div><div class="zone-hits" id="hits-upper_left">0</div><div class="zone-pct" id="pct-upper_left">0%</div></div>
      <div class="goal-zone" id="zone-upper_center" data-zone="upper_center"><div class="zone-label">Haut C.</div><div class="zone-hits" id="hits-upper_center">0</div><div class="zone-pct" id="pct-upper_center">0%</div></div>
      <div class="goal-zone" id="zone-upper_right" data-zone="upper_right"><div class="zone-label">Haut D.</div><div class="zone-hits" id="hits-upper_right">0</div><div class="zone-pct" id="pct-upper_right">0%</div></div>
      <div class="goal-zone" id="zone-middle_left" data-zone="middle_left"><div class="zone-label">Milieu G.</div><div class="zone-hits" id="hits-middle_left">0</div><div class="zone-pct" id="pct-middle_left">0%</div></div>
      <div class="goal-zone" id="zone-center" data-zone="center"><div class="zone-label">Centre</div><div class="zone-hits" id="hits-center">0</div><div class="zone-pct" id="pct-center">0%</div></div>
      <div class="goal-zone" id="zone-middle_right" data-zone="middle_right"><div class="zone-label">Milieu D.</div><div class="zone-hits" id="hits-middle_right">0</div><div class="zone-pct" id="pct-middle_right">0%</div></div>
      <div class="goal-zone" id="zone-lower_left" data-zone="lower_left"><div class="zone-label">Bas G.</div><div class="zone-hits" id="hits-lower_left">0</div><div class="zone-pct" id="pct-lower_left">0%</div></div>
      <div class="goal-zone" id="zone-lower_center" data-zone="lower_center"><div class="zone-label">Bas C.</div><div class="zone-hits" id="hits-lower_center">0</div><div class="zone-pct" id="pct-lower_center">0%</div></div>
      <div class="goal-zone" id="zone-lower_right" data-zone="lower_right"><div class="zone-label">Bas D.</div><div class="zone-hits" id="hits-lower_right">0</div><div class="zone-pct" id="pct-lower_right">0%</div></div>
    `;
  }
}

/**
 * Update heatmap coloring and numbers inside the goal quadrants.
 */
function updateGoalHeatmap(dist, totalShots) {
  const zones = [
    "upper_left", "upper_right", "lower_left", "lower_right",
    "upper_center", "middle_left", "center", "middle_right", "lower_center"
  ];

  zones.forEach((zone) => {
    const hitsElem = document.getElementById(`hits-${zone}`);
    const pctElem = document.getElementById(`pct-${zone}`);
    const zoneBox = document.getElementById(`zone-${zone}`);

    const count = dist[zone] || 0;
    const pct = totalShots > 0 ? (count / totalShots) * 100 : 0;

    if (hitsElem) hitsElem.textContent = count;
    if (pctElem) pctElem.textContent = `${pct.toFixed(0)}%`;

    if (zoneBox) {
      zoneBox.classList.remove("active-hot", "active-mid");
      if (pct >= 30) {
        zoneBox.classList.add("active-hot");
      } else if (pct >= 15) {
        zoneBox.classList.add("active-mid");
      }
    }
  });

  // None (missed / no impact)
  const noneHits = dist["none"] || 0;
  const nonePct = totalShots > 0 ? (noneHits / totalShots) * 100 : 0;
  document.getElementById("hits-none").textContent = noneHits;
  document.getElementById("pct-none").textContent = `${nonePct.toFixed(1)}%`;
}

/**
 * Load recent shots table with ENF1 latency calculation.
 */
async function loadShots() {
  try {
    const url = currentSessionFilter
      ? `/api/shots?session_id=${encodeURIComponent(currentSessionFilter)}&limit=15`
      : "/api/shots?limit=15";

    const res = await fetch(url);
    if (!res.ok) return;
    const data = await res.json();

    const tbody = document.getElementById("shots-table-body");
    document.getElementById("shots-count-label").textContent = `${data.shots.length} affichés (total: ${data.total})`;

    if (data.shots.length === 0) {
      tbody.innerHTML = `
        <tr>
          <td colspan="8" style="text-align: center; color: var(--text-muted); padding: 24px;">
            Aucun tir enregistré pour le moment.
          </td>
        </tr>`;
      return;
    }

    tbody.innerHTML = "";
    data.shots.forEach((s) => {
      const tr = document.createElement("tr");

      // Format timestamp
      const date = new Date(s.timestamp);
      const timeStr = date.toLocaleTimeString("fr-FR", { hour: "2-digit", minute: "2-digit", second: "2-digit" });

      // Latency badge
      const latencyMs = s.latency_ms !== null && s.latency_ms !== undefined ? s.latency_ms : null;
      let latencyClass = "latency-good";
      let latencyText = latencyMs !== null ? `${latencyMs} ms` : "-";
      if (latencyMs !== null && latencyMs > 2000) {
        latencyClass = "latency-warn";
      }

      // Zone badge
      let zoneBadgeClass = "badge-zone";
      if (s.target_zone === "none") zoneBadgeClass = "badge-none";

      tr.innerHTML = `
        <td><strong>${timeStr}</strong></td>
        <td><code>${escapeHtml(s.device_id)}</code></td>
        <td><span class="badge" style="background:#1e293b; color:#94a3b8;">${escapeHtml(s.session_id)}</span></td>
        <td><span class="badge badge-speed">${s.ball_speed_kmh.toFixed(1)} km/h</span></td>
        <td><span class="badge ${zoneBadgeClass}">${escapeHtml(s.target_zone)}</span></td>
        <td>${s.pressure_reading} <span style="font-size:0.75rem; color:#64748b;">(ADC)</span></td>
        <td>${s.impact_intensity} <span style="font-size:0.75rem; color:#64748b;">(ADC)</span></td>
        <td><span class="${latencyClass}">● ${latencyText}</span></td>
      `;
      tbody.appendChild(tr);
    });
  } catch (err) {
    console.error("Error loading shots table:", err);
  }
}

/**
 * Side-by-side session comparison.
 */
function updateComparison() {
  const sidA = document.getElementById("comp-session-a").value;
  const sidB = document.getElementById("comp-session-b").value;

  const sA = sessionsCache.find((s) => s.session_id === sidA);
  const sB = sessionsCache.find((s) => s.session_id === sidB);

  renderComparisonBox("a", sA);
  renderComparisonBox("b", sB);
}

function renderComparisonBox(prefix, session) {
  const box = document.getElementById(`box-session-${prefix}`);
  const title = box.querySelector("h4");
  if (!session) {
    title.textContent = `Séance ${prefix.toUpperCase()}`;
    document.getElementById(`comp-${prefix}-shots`).textContent = "-";
    document.getElementById(`comp-${prefix}-acc`).textContent = "-";
    document.getElementById(`comp-${prefix}-spd`).textContent = "-";
    document.getElementById(`comp-${prefix}-pres`).textContent = "-";
    return;
  }

  title.textContent = `Séance : ${session.session_id}`;
  document.getElementById(`comp-${prefix}-shots`).textContent = session.total_shots;
  document.getElementById(`comp-${prefix}-acc`).textContent = `${session.accuracy_rate_pct}%`;
  document.getElementById(`comp-${prefix}-spd`).textContent = `${session.avg_speed_kmh} km/h (max ${session.max_speed_kmh})`;
  document.getElementById(`comp-${prefix}-pres`).textContent = `${Math.round(session.avg_pressure_reading)} ADC`;
}

/**
 * Check devices and keeper status.
 */
async function loadDevices() {
  try {
    const res = await fetch("/api/devices");
    if (!res.ok) return;
    const devices = await res.json();

    const keeper = devices.find((d) => d.device_id.includes("keeper"));
    if (keeper) {
      document.getElementById("keeper-badge").textContent = keeper.status;
      document.getElementById("keeper-badge").className = "badge badge-speed";
      document.getElementById("keeper-status-text").textContent = `Connecté (${keeper.status})`;
    }
  } catch (err) {
    console.error("Error loading devices:", err);
  }
}

/**
 * Fetch and display latest goalkeeper MPU6050 telemetry (Étape 11).
 */
async function loadKeeperData() {
  try {
    const res = await fetch("/api/keeper/latest");
    if (!res.ok) return;
    const keeper = await res.json();
    if (!keeper) return;

    document.getElementById("keeper-badge").textContent = "Actif";
    document.getElementById("keeper-badge").className = "badge badge-speed";
    document.getElementById("keeper-status-text").textContent = `En ligne (v${escapeHtml(keeper.firmware_version)})`;

    const telemEl = document.getElementById("keeper-telemetry");
    if (telemEl) {
      telemEl.style.display = "block";
      const ax = typeof keeper.accel_x === "number" ? keeper.accel_x.toFixed(2) : "0.00";
      const ay = typeof keeper.accel_y === "number" ? keeper.accel_y.toFixed(2) : "0.00";
      const az = typeof keeper.accel_z === "number" ? keeper.accel_z.toFixed(2) : "0.00";
      document.getElementById("keeper-accel").textContent = `X: ${ax} | Y: ${ay} | Z: ${az}`;

      const gx = typeof keeper.gyro_x === "number" ? keeper.gyro_x.toFixed(1) : "0.0";
      const gy = typeof keeper.gyro_y === "number" ? keeper.gyro_y.toFixed(1) : "0.0";
      const gz = typeof keeper.gyro_z === "number" ? keeper.gyro_z.toFixed(1) : "0.0";
      document.getElementById("keeper-gyro").textContent = `X: ${gx} | Y: ${gy} | Z: ${gz}`;

      const diveMap = {
        none: "Position stable",
        left: "Plongeon gauche ⬅️",
        right: "Plongeon droite ➡️",
        jump: "Extension / Saut ⬆️",
        dive: "Plongeon au sol ⬇️",
      };
      const directionText = diveMap[keeper.dive_direction] || keeper.dive_direction;
      document.getElementById("keeper-dive").textContent = directionText;
    }
  } catch (err) {
    console.debug("Keeper telemetry not yet available:", err);
  }
}


function escapeHtml(text) {
  if (!text) return "";
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}
