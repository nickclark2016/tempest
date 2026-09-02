/**
 * Tempest Engine Profiler - Interactive Single-Page Web UI Application
 * Pure vanilla modern ES6+ JavaScript. Zero external dependencies.
 */

(function () {
  'use strict';

  // =========================================================================
  // State Management
  // =========================================================================

  const state = {
    // Connection & Streaming
    ws: null,
    wsUrl: '',
    isConnected: false,
    isLive: true,
    isEngineRecording: false,
    isOfflineReplay: false,
    heartbeatInterval: null,
    reconnectTimer: null,
    reconnectAttempts: 0,

    // Data Storage
    frames: [],             // Array of telemetry_frame objects
    tracks: [],             // Merged / active tracks
    markers: [],            // Global markers
    metrics: {},            // Metric stream map: name -> Array<{ timestamp_ns, value, unit }>
    zoneStats: new Map(),   // Map of zone name -> stats object
    
    // Time & Viewport
    baseTimeNs: null,
    minTimeNs: 0,
    maxTimeNs: 0,
    viewStartNs: 0,
    viewEndNs: 100000000,   // Default 100ms
    timeUnit: 'auto',       // 'auto', 'ms', 'us', 'ns'
    scrollY: 0,
    totalContentHeight: 0,

    // Selection & Interactivity
    selectedZone: null,
    hoveredZone: null,
    hoveredFrameIndex: null,
    showFrameBoundaries: true,
    lastStatsRecalcTime: 0,
    selectionRange: null,   // { startNs, endNs }
    searchQuery: '',
    searchResultsCount: 0,
    collapsedTracks: new Set(),
    selectedStatsRow: null,
    sortColumn: 'total',
    sortAscending: false,

    // Drag & Pan state
    isDragging: false,
    dragStartX: 0,
    dragStartY: 0,
    dragStartViewStart: 0,
    dragStartViewEnd: 0,
    dragStartScrollY: 0,
    isSelectingRange: false,
    rangeSelectStartX: 0,
    isOverviewDragging: false,

    // Layout Constants
    rulerHeight: 28,
    trackHeaderHeight: 22,
    frameHeaderHeight: 18,
    zoneHeight: 18,
    zoneSpacing: 2,
    trackPaddingBottom: 8,
  };

  // DOM Elements Cache
  const dom = {
    statusBadge: document.getElementById('status-badge'),
    statusText: document.getElementById('status-text'),
    liveToggleBtn: document.getElementById('btn-live-toggle'),
    liveToggleText: document.getElementById('live-toggle-text'),
    engineCaptureBtn: document.getElementById('btn-engine-capture'),
    engineCaptureText: document.getElementById('engine-capture-text'),
    queryStatsBtn: document.getElementById('btn-query-stats'),
    snapshotBtn: document.getElementById('btn-snapshot'),
    saveTraceBtn: document.getElementById('btn-save-trace'),
    openFileBtn: document.getElementById('btn-open-file'),
    fileInput: document.getElementById('file-input'),
    clearBtn: document.getElementById('btn-clear'),
    fitViewBtn: document.getElementById('btn-fit-view'),
    zoomInBtn: document.getElementById('btn-zoom-in'),
    zoomOutBtn: document.getElementById('btn-zoom-out'),
    selectTimeUnit: document.getElementById('select-time-unit'),
    shortcutsBtn: document.getElementById('btn-shortcuts'),
    shortcutsModal: document.getElementById('shortcuts-modal'),
    modalCloseBtn: document.getElementById('modal-close-btn'),
    searchInput: document.getElementById('search-input'),
    searchCount: document.getElementById('search-count'),
    searchClearBtn: document.getElementById('search-clear-btn'),
    
    // Overview & Timeline
    overviewCanvas: document.getElementById('overview-canvas'),
    overviewTimeRange: document.getElementById('overview-time-range'),
    overviewFrameStats: document.getElementById('overview-frame-stats'),
    timelineContainer: document.getElementById('timeline-container'),
    timelineCanvas: document.getElementById('timeline-canvas'),
    selectionBadge: document.getElementById('selection-badge'),
    selectionDurationVal: document.getElementById('selection-duration-val'),
    selectionRangeVal: document.getElementById('selection-range-val'),
    tooltip: document.getElementById('tooltip'),

    // Dock & Tabs
    dockSection: document.getElementById('dock-section'),
    dockResizeHandle: document.getElementById('dock-resize-handle'),
    toggleDockBtn: document.getElementById('btn-toggle-dock'),
    tabButtons: document.querySelectorAll('.dock-tabs .tab-btn'),
    tabPanes: document.querySelectorAll('.dock-content-area .tab-pane'),

    // Inspector Elements
    inspectorPlaceholder: document.getElementById('inspector-placeholder'),
    inspectorDetails: document.getElementById('inspector-details'),
    inspCategory: document.getElementById('insp-category'),
    inspName: document.getElementById('insp-name'),
    inspTrack: document.getElementById('insp-track'),
    inspLocation: document.getElementById('insp-location'),
    inspDuration: document.getElementById('insp-duration'),
    inspSelfTime: document.getElementById('insp-self-time'),
    inspFramePct: document.getElementById('insp-frame-pct'),
    inspDepth: document.getElementById('insp-depth'),
    inspMetricsList: document.getElementById('insp-metrics-list'),
    inspHierarchyTree: document.getElementById('insp-hierarchy-tree'),

    // Stats Table & Histogram
    statsFilter: document.getElementById('stats-filter'),
    statsTotalCount: document.getElementById('stats-total-count'),
    statsTable: document.getElementById('stats-table'),
    statsTbody: document.getElementById('stats-tbody'),
    exportStatsCsvBtn: document.getElementById('btn-export-stats-csv'),
    distTitle: document.getElementById('dist-title'),
    distSubtitle: document.getElementById('dist-subtitle'),
    histogramCanvas: document.getElementById('histogram-canvas'),

    // Metric Charts
    metricFpsVal: document.getElementById('metric-fps-val'),
    metricFrametimeVal: document.getElementById('metric-frametime-val'),
    metricMemoryVal: document.getElementById('metric-memory-val'),
    metricGpuVal: document.getElementById('metric-gpu-val'),
    chartFps: document.getElementById('chart-fps'),
    chartFrametime: document.getElementById('chart-frametime'),
    chartMemory: document.getElementById('chart-memory'),
    chartGpu: document.getElementById('chart-gpu'),

    // Info Elements
    infoStatus: document.getElementById('info-status'),
    infoWsUrl: document.getElementById('info-ws-url'),
    infoTotalFrames: document.getElementById('info-total-frames'),
    infoTotalZones: document.getElementById('info-total-zones'),
    infoTotalTracks: document.getElementById('info-total-tracks'),
    infoDuration: document.getElementById('info-duration'),
    infoTimeExtent: document.getElementById('info-time-extent'),

    // Drop Overlay
    dropOverlay: document.getElementById('drop-overlay'),
  };

  // Contexts
  const timelineCtx = dom.timelineCanvas.getContext('2d');
  const overviewCtx = dom.overviewCanvas.getContext('2d');
  const histogramCtx = dom.histogramCanvas.getContext('2d');
  const fpsCtx = dom.chartFps.getContext('2d');
  const frametimeCtx = dom.chartFrametime.getContext('2d');
  const memoryCtx = dom.chartMemory.getContext('2d');
  const gpuCtx = dom.chartGpu.getContext('2d');

  // =========================================================================
  // Formatting & Color Helpers
  // =========================================================================

  function formatTime(nanoseconds, forcedUnit = state.timeUnit) {
    if (nanoseconds === undefined || nanoseconds === null || isNaN(nanoseconds)) {
      return '0.00 ns';
    }
    const absNs = Math.abs(nanoseconds);

    if (forcedUnit === 'ns' || (forcedUnit === 'auto' && absNs < 1000)) {
      return `${nanoseconds.toFixed(0)} ns`;
    }
    if (forcedUnit === 'us' || (forcedUnit === 'auto' && absNs < 1000000)) {
      return `${(nanoseconds / 1000).toFixed(2)} µs`;
    }
    if (forcedUnit === 'ms' || (forcedUnit === 'auto' && absNs < 1000000000)) {
      return `${(nanoseconds / 1000000).toFixed(2)} ms`;
    }
    return `${(nanoseconds / 1000000000).toFixed(3)} s`;
  }

  function formatRelativeTime(nanoseconds, forcedUnit = state.timeUnit) {
    if (nanoseconds === undefined || nanoseconds === null || isNaN(nanoseconds)) {
      return '0.00 ns';
    }
    const base = (state.baseTimeNs !== null && state.baseTimeNs !== undefined) ? state.baseTimeNs : 0;
    return formatTime(nanoseconds - base, forcedUnit);
  }

  function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(Math.abs(bytes)) / Math.log(k));
    return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
  }

  function hashString(str) {
    let hash = 5381;
    for (let i = 0; i < str.length; i++) {
      hash = ((hash << 5) + hash) + str.charCodeAt(i);
      hash = hash & hash;
    }
    return Math.abs(hash);
  }

  function getZoneColor(name, category = 'cpu') {
    if (!name) return '#58a6ff';

    // Special category colors
    const lower = name.toLowerCase();
    if (category === 'gpu' || lower.includes('vk') || lower.includes('gpu') || lower.includes('draw') || lower.includes('compute') || lower.includes('pass')) {
      if (lower.includes('compute')) return 'hsl(170, 75%, 45%)';
      if (lower.includes('transfer') || lower.includes('copy')) return 'hsl(210, 80%, 55%)';
      if (lower.includes('shadow')) return 'hsl(280, 70%, 50%)';
      if (lower.includes('pbr') || lower.includes('render')) return 'hsl(190, 85%, 42%)';
      return 'hsl(180, 70%, 45%)';
    }

    if (lower.includes('wait') || lower.includes('sleep') || lower.includes('sync') || lower.includes('lock')) {
      return 'hsl(30, 70%, 45%)';
    }
    if (lower.includes('present') || lower.includes('swapchain')) {
      return 'hsl(45, 90%, 50%)';
    }
    if (lower.includes('init') || lower.includes('setup')) {
      return 'hsl(260, 65%, 55%)';
    }
    if (lower.includes('update') || lower.includes('tick')) {
      return 'hsl(130, 60%, 42%)';
    }

    // Deterministic palette hash
    const hash = hashString(name);
    const hue = hash % 360;
    const sat = 55 + (hash % 30);
    const lum = 40 + (hash % 20);
    return `hsl(${hue}, ${sat}%, ${lum}%)`;
  }

  // =========================================================================
  // WebSocket Client & Streaming
  // =========================================================================

  function initWebSocket() {
    if (state.ws) {
      try { state.ws.close(); } catch (e) {}
      state.ws = null;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host || '127.0.0.1:8080';
    state.wsUrl = `${protocol}//${host}/ws`;
    dom.infoWsUrl.textContent = state.wsUrl;

    updateStatus('Connecting...', 'connecting');

    try {
      state.ws = new WebSocket(state.wsUrl);
      state.ws.binaryType = 'arraybuffer';

      state.ws.onopen = () => {
        state.isConnected = true;
        state.reconnectAttempts = 0;
        updateStatus(state.isLive ? 'Live' : 'Paused', state.isLive ? 'live' : 'paused');

        // Start heartbeat ping
        if (state.heartbeatInterval) clearInterval(state.heartbeatInterval);
        state.heartbeatInterval = setInterval(() => {
          if (state.ws && state.ws.readyState === WebSocket.OPEN) {
            // Heartbeat can be an empty text or ping
            try { state.ws.send(JSON.stringify({ command: 'ping' })); } catch (e) {}
          }
        }, 5000);
      };

      state.ws.onclose = () => {
        state.isConnected = false;
        if (state.heartbeatInterval) clearInterval(state.heartbeatInterval);
        if (!state.isOfflineReplay) {
          updateStatus('Disconnected', 'disconnected');
          scheduleReconnect();
        }
      };

      state.ws.onerror = () => {
        state.isConnected = false;
        if (!state.isOfflineReplay) {
          updateStatus('Connection Error', 'disconnected');
        }
      };

      state.ws.onmessage = (event) => {
        if (typeof event.data === 'string') {
          handleTextMessage(event.data);
        } else if (event.data instanceof ArrayBuffer) {
          handleBinaryMessage(event.data);
        }
      };
    } catch (err) {
      console.warn('WebSocket init failed:', err);
      scheduleReconnect();
    }
  }

  function scheduleReconnect() {
    if (state.reconnectTimer) clearTimeout(state.reconnectTimer);
    if (state.isOfflineReplay) return;

    state.reconnectAttempts++;
    const delay = Math.min(5000, 1000 * Math.pow(1.5, state.reconnectAttempts - 1));
    state.reconnectTimer = setTimeout(() => {
      initWebSocket();
    }, delay);
  }

  function sendCommand(commandName, args = {}) {
    if (!state.ws || state.ws.readyState !== WebSocket.OPEN) {
      console.warn('Cannot send command: WebSocket not connected');
      return;
    }
    const payload = JSON.stringify({ command: commandName, ...args });
    state.ws.send(payload);
  }

  function updateStatus(text, type) {
    dom.statusText.textContent = text;
    dom.statusBadge.className = `status-indicator ${type}`;
    dom.infoStatus.textContent = text;
  }

  function handleTextMessage(text) {
    try {
      const data = JSON.parse(text);
      if (data.type === 'frame_data') {
        if (!state.isLive) return;
        ingestTelemetryFrame(data);
      } else if (data.type === 'stats') {
        if (Array.isArray(data.zones)) {
          updateStatsFromEngine(data.zones);
        }
      } else if (data.type === 'response') {
        if (data.command === 'start_capture') {
          state.isEngineRecording = true;
          dom.engineCaptureBtn.classList.add('recording');
          dom.engineCaptureText.textContent = 'Stop Recording';
        } else if (data.command === 'stop_capture') {
          state.isEngineRecording = false;
          dom.engineCaptureBtn.classList.remove('recording');
          dom.engineCaptureText.textContent = 'Record';
        }
      } else if (data.traceEvents) {
        // Chrome Trace JSON format received from snapshot
        ingestChromeTrace(data);
      }
    } catch (e) {
      console.warn('Failed to parse text message:', e, text);
    }
  }

  function handleBinaryMessage(arrayBuffer) {
    parseBinaryTprof(arrayBuffer);
  }

  // =========================================================================
  // Telemetry Ingestion & Data Management
  // =========================================================================

  function ingestTelemetryFrame(frame) {
    if (!state.isLive) {
      // If paused, completely drop incoming live frame
      return;
    }

    // 1. Process CPU tracks, markers, and metrics for frame.frame_index
    let cpuFrame = state.frames.find(f => f.frame_index === frame.frame_index);
    if (!cpuFrame) {
      cpuFrame = {
        frame_index: frame.frame_index,
        cpu_tracks: frame.cpu_tracks || [],
        gpu_tracks: [],
        markers: frame.markers || [],
        metrics: frame.metrics || [],
      };
      state.frames.push(cpuFrame);
    } else {
      if (frame.cpu_tracks && frame.cpu_tracks.length > 0) {
        if (!cpuFrame.cpu_tracks) cpuFrame.cpu_tracks = [];
        for (const t of frame.cpu_tracks) {
          const existing = cpuFrame.cpu_tracks.find(et => et.track_id === t.track_id);
          if (existing) {
            if (t.zones) existing.zones.push(...t.zones);
          } else {
            cpuFrame.cpu_tracks.push(t);
          }
        }
      }
      if (frame.markers && frame.markers.length > 0) {
        if (!cpuFrame.markers) cpuFrame.markers = [];
        cpuFrame.markers.push(...frame.markers);
      }
      if (frame.metrics && frame.metrics.length > 0) {
        if (!cpuFrame.metrics) cpuFrame.metrics = [];
        cpuFrame.metrics.push(...frame.metrics);
      }
    }
    updateDataBounds(cpuFrame);

    // 2. Process GPU tracks: route each GPU zone to its originating CPU frame_index
    if (frame.gpu_tracks && frame.gpu_tracks.length > 0) {
      for (const t of frame.gpu_tracks) {
        if (!t.zones || t.zones.length === 0) continue;
        for (const z of t.zones) {
          const targetIdx = (z.frame_index !== undefined && z.frame_index !== null && z.frame_index !== 0)
            ? z.frame_index
            : frame.frame_index;

          let targetGpuFrame = state.frames.find(f => f.frame_index === targetIdx);
          if (!targetGpuFrame) {
            targetGpuFrame = {
              frame_index: targetIdx,
              cpu_tracks: [],
              gpu_tracks: [],
              markers: [],
              metrics: [],
            };
            state.frames.push(targetGpuFrame);
          }
          if (!targetGpuFrame.gpu_tracks) targetGpuFrame.gpu_tracks = [];
          let gtrack = targetGpuFrame.gpu_tracks.find(gt => gt.track_id === t.track_id);
          if (!gtrack) {
            gtrack = {
              track_id: t.track_id,
              name: t.name,
              zones: [],
            };
            targetGpuFrame.gpu_tracks.push(gtrack);
          }
          if (!gtrack.zones.some(ez => ez.name === z.name && ez.start_ns === z.start_ns && ez.depth === z.depth)) {
            gtrack.zones.push(z);
          }
          updateDataBounds(targetGpuFrame);
        }
      }
    }

    // Keep frames sorted by frame_index and bounded in size
    state.frames.sort((a, b) => a.frame_index - b.frame_index);
    if (state.frames.length > 1000) {
      state.frames.splice(0, state.frames.length - 1000);
    }

    const frameMin = updateDataBounds(cpuFrame);
    recalculateTracksAndMetrics();

    // Throttled stats recalculation to keep live 60 FPS silky smooth
    const now = performance.now();
    if (now - state.lastStatsRecalcTime > 250) {
      state.lastStatsRecalcTime = now;
      recalculateStats();
    }
    updateInfoMetadata();

    // Auto-scroll timeline to follow live stream accommodating frames-in-flight delay
    if (state.isLive) {
      const duration = Math.max(state.viewEndNs - state.viewStartNs, 100000000);
      state.viewEndNs = state.maxTimeNs;
      let targetStart = state.maxTimeNs - duration;
      if (frameMin !== undefined && frameMin !== Infinity && frameMin < targetStart) {
        targetStart = frameMin;
        state.viewEndNs = Math.max(state.maxTimeNs, targetStart + duration);
      }
      state.viewStartNs = Math.max(state.minTimeNs, targetStart);
    }

    requestAnimationFrame(render);
  }

  function updateDataBounds(frame) {
    let frameMin = Infinity;
    let frameMax = -Infinity;

    let cpuMin = Infinity;
    let cpuMax = -Infinity;
    let gpuMin = Infinity;
    let gpuMax = -Infinity;
    let graphicsGpuMin = Infinity;
    let graphicsGpuMax = -Infinity;

    if (frame.cpu_tracks) {
      for (const t of frame.cpu_tracks) {
        if (t.zones) {
          for (const z of t.zones) {
            if (z.start_ns < cpuMin) cpuMin = z.start_ns;
            if (z.end_ns > cpuMax) cpuMax = z.end_ns;
            if (z.start_ns < frameMin) frameMin = z.start_ns;
            if (z.end_ns > frameMax) frameMax = z.end_ns;
          }
        }
      }
    }
    if (frame.gpu_tracks) {
      for (const t of frame.gpu_tracks) {
        const isGraphics = !t.name || t.name.toLowerCase().includes('graphics') || frame.gpu_tracks.length === 1;
        if (t.zones) {
          for (const z of t.zones) {
            if (z.start_ns < gpuMin) gpuMin = z.start_ns;
            if (z.end_ns > gpuMax) gpuMax = z.end_ns;
            if (isGraphics) {
              if (z.start_ns < graphicsGpuMin) graphicsGpuMin = z.start_ns;
              if (z.end_ns > graphicsGpuMax) graphicsGpuMax = z.end_ns;
            }
            if (z.start_ns < frameMin) frameMin = z.start_ns;
            if (z.end_ns > frameMax) frameMax = z.end_ns;
          }
        }
      }
    }
    if (frame.markers) {
      for (const m of frame.markers) {
        if (m.timestamp_ns < frameMin) frameMin = m.timestamp_ns;
        if (m.timestamp_ns > frameMax) frameMax = m.timestamp_ns;
      }
    }

    frame.cpuStartNs = cpuMin !== Infinity ? cpuMin : null;
    frame.cpuEndNs = cpuMax !== -Infinity ? cpuMax : null;
    frame.cpuDurationNs = (frame.cpuStartNs !== null && frame.cpuEndNs !== null) ? (frame.cpuEndNs - frame.cpuStartNs) : 0;

    const effectiveGpuMin = graphicsGpuMin !== Infinity ? graphicsGpuMin : gpuMin;
    const effectiveGpuMax = graphicsGpuMax !== -Infinity ? graphicsGpuMax : gpuMax;
    frame.gpuStartNs = effectiveGpuMin !== Infinity ? effectiveGpuMin : null;
    frame.gpuEndNs = effectiveGpuMax !== -Infinity ? effectiveGpuMax : null;
    frame.gpuDurationNs = (frame.gpuStartNs !== null && frame.gpuEndNs !== null) ? (frame.gpuEndNs - frame.gpuStartNs) : 0;

    if (frameMin !== Infinity && frameMax !== -Infinity) {
      if (state.baseTimeNs === null) {
        state.baseTimeNs = frameMin;
      }
      if (state.frames.length === 1 || state.minTimeNs === 0) {
        state.minTimeNs = frameMin;
        state.maxTimeNs = frameMax;
        state.viewStartNs = frameMin;
        state.viewEndNs = frameMax > frameMin ? frameMax : frameMin + 100000000;
      } else {
        state.minTimeNs = Math.min(state.minTimeNs, frameMin);
        state.maxTimeNs = Math.max(state.maxTimeNs, frameMax);
      }
    }

    return frameMin;
  }

  function recalculateTracksAndMetrics() {
    const trackMap = new Map(); // key: track_id -> track object
    const metricsMap = {};
    state.markers = [];

    for (const f of state.frames) {
      // CPU Tracks
      if (f.cpu_tracks) {
        for (const t of f.cpu_tracks) {
          const key = `cpu_${t.track_id}`;
          if (!trackMap.has(key)) {
            trackMap.set(key, {
              id: key,
              track_id: t.track_id,
              name: t.name || `Thread ${t.track_id}`,
              type: 'cpu',
              zones: [],
              maxDepth: 0,
            });
          }
          const tr = trackMap.get(key);
          if (t.name && (!tr.name || tr.name.startsWith('Thread '))) {
            tr.name = t.name;
          }
          if (t.zones) {
            for (const z of t.zones) {
              const zoneObj = {
                ...z,
                trackId: key,
                trackName: tr.name,
                category: 'cpu',
                frame_index: (z.frame_index !== undefined && z.frame_index !== null && z.frame_index !== 0) ? z.frame_index : f.frame_index,
                duration_ns: z.end_ns >= z.start_ns ? (z.end_ns - z.start_ns) : 0,
              };
              tr.zones.push(zoneObj);
              if (z.depth > tr.maxDepth) tr.maxDepth = z.depth;
            }
          }
        }
      }

      // GPU Tracks
      if (f.gpu_tracks) {
        for (const t of f.gpu_tracks) {
          const key = `gpu_${t.track_id}`;
          let cleanName = t.name || `Queue ${t.track_id}`;
          if (cleanName.startsWith('GPU: ')) {
            cleanName = cleanName.substring(5);
          } else if (cleanName.startsWith('GPU:')) {
            cleanName = cleanName.substring(4);
          }
          if (!trackMap.has(key)) {
            trackMap.set(key, {
              id: key,
              track_id: t.track_id,
              name: cleanName,
              type: 'gpu',
              zones: [],
              maxDepth: 0,
            });
          }
          const tr = trackMap.get(key);
          if (cleanName && (!tr.name || tr.name.startsWith('Queue '))) {
            tr.name = cleanName;
          }
          if (t.zones) {
            for (const z of t.zones) {
              const zoneObj = {
                ...z,
                trackId: key,
                trackName: tr.name,
                category: 'gpu',
                frame_index: (z.frame_index !== undefined && z.frame_index !== null && z.frame_index !== 0) ? z.frame_index : f.frame_index,
                duration_ns: z.end_ns >= z.start_ns ? (z.end_ns - z.start_ns) : 0,
              };
              tr.zones.push(zoneObj);
              if (z.depth > tr.maxDepth) tr.maxDepth = z.depth;
            }
          }
        }
      }

      // Global Markers
      if (f.markers) {
        for (const m of f.markers) {
          state.markers.push(m);
        }
      }

      // Global Metrics
      if (f.metrics) {
        for (const m of f.metrics) {
          if (!metricsMap[m.name]) {
            metricsMap[m.name] = [];
          }
          metricsMap[m.name].push({
            timestamp_ns: m.timestamp_ns || f.frame_index * 16666666,
            value: m.value,
            unit: m.unit,
          });
        }
      }
    }

    state.tracks = Array.from(trackMap.values()).sort((a, b) => {
      // 1. CPU tracks above GPU tracks
      if (a.type !== b.type) {
        return a.type === 'cpu' ? -1 : 1;
      }
      // 2. Main Thread first among CPU tracks
      if (a.type === 'cpu') {
        if (a.name === 'Main Thread') return -1;
        if (b.name === 'Main Thread') return 1;
      }
      // 3. Graphics Queue first among GPU tracks
      if (a.type === 'gpu') {
        if (a.name.includes('Graphics')) return -1;
        if (b.name.includes('Graphics')) return 1;
      }
      return a.track_id - b.track_id;
    });
    state.metrics = metricsMap;

    updateMetricCards();
  }

  function recalculateStats() {
    const statsMap = new Map();

    for (const track of state.tracks) {
      for (const z of track.zones) {
        if (!statsMap.has(z.name)) {
          statsMap.set(z.name, {
            name: z.name,
            category: z.category || track.type || 'cpu',
            count: 0,
            totalNs: 0,
            minNs: Infinity,
            maxNs: -Infinity,
            durations: [],
          });
        }
        const st = statsMap.get(z.name);
        const dur = z.duration_ns;
        st.count++;
        st.totalNs += dur;
        if (dur < st.minNs) st.minNs = dur;
        if (dur > st.maxNs) st.maxNs = dur;
        st.durations.push(dur);
      }
    }

    // Compute percentiles and stddev
    for (const [name, st] of statsMap.entries()) {
      st.durations.sort((a, b) => a - b);
      const N = st.durations.length;
      st.meanNs = N > 0 ? st.totalNs / N : 0;
      st.p50Ns = N > 0 ? st.durations[Math.floor(0.50 * (N - 1))] : 0;
      st.p90Ns = N > 0 ? st.durations[Math.floor(0.90 * (N - 1))] : 0;
      st.p95Ns = N > 0 ? st.durations[Math.floor(0.95 * (N - 1))] : 0;
      st.p99Ns = N > 0 ? st.durations[Math.floor(0.99 * (N - 1))] : 0;

      let varSum = 0;
      for (const d of st.durations) {
        const diff = d - st.meanNs;
        varSum += diff * diff;
      }
      st.stdDevNs = N > 0 ? Math.sqrt(varSum / N) : 0;
    }

    state.zoneStats = statsMap;
    renderStatsTable();
  }

  function updateStatsFromEngine(engineZones) {
    for (const ez of engineZones) {
      state.zoneStats.set(ez.name, {
        name: ez.name,
        category: 'cpu',
        count: ez.count,
        totalNs: ez.mean_ns * ez.count,
        minNs: ez.min_ns,
        maxNs: ez.max_ns,
        meanNs: ez.mean_ns,
        p50Ns: ez.p50_ns,
        p90Ns: ez.p90_ns,
        p95Ns: ez.p95_ns,
        p99Ns: ez.p99_ns,
        stdDevNs: ez.std_deviation_ns,
        durations: [],
      });
    }
    renderStatsTable();
  }

  // =========================================================================
  // Canvas Rendering (Timeline, Flame Chart, Overview)
  // =========================================================================

  function resizeCanvas(canvas, ctx) {
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    const width = Math.floor(rect.width);
    const height = Math.floor(rect.height);

    if (canvas.width !== width * dpr || canvas.height !== height * dpr) {
      canvas.width = width * dpr;
      canvas.height = height * dpr;
      ctx.scale(dpr, dpr);
    }
    return { width, height, dpr };
  }

  function render() {
    renderTimeline();
    renderOverview();
  }

  function renderTimeline() {
    const { width, height } = resizeCanvas(dom.timelineCanvas, timelineCtx);
    timelineCtx.clearRect(0, 0, width, height);

    if (width <= 0 || height <= 0) return;

    const visibleDuration = state.viewEndNs - state.viewStartNs;
    if (visibleDuration <= 0) return;

    const nsToX = (ns) => ((ns - state.viewStartNs) / visibleDuration) * width;
    const xToNs = (x) => state.viewStartNs + (x / width) * visibleDuration;

    // 1. Draw Grid Lines & Time Subdivisions
    renderGrid(timelineCtx, width, height, nsToX, visibleDuration);

    // 2. Draw Tracks & Zones
    let currentY = state.rulerHeight - state.scrollY;
    state.searchResultsCount = 0;

    // Global Markers Track (if any)
    if (state.markers.length > 0) {
      const markersTrackHeight = 22;
      if (currentY + markersTrackHeight > state.rulerHeight && currentY < height) {
        timelineCtx.fillStyle = '#161b22';
        timelineCtx.fillRect(0, currentY, width, markersTrackHeight);
        timelineCtx.fillStyle = '#d29922';
        timelineCtx.font = 'bold 10px monospace';
        timelineCtx.fillText('MARKERS', 8, currentY + 15);

        for (const m of state.markers) {
          const x = nsToX(m.timestamp_ns);
          if (x >= -10 && x <= width + 10) {
            // Draw marker pin / flag
            timelineCtx.fillStyle = '#d29922';
            timelineCtx.beginPath();
            timelineCtx.moveTo(x, currentY + 4);
            timelineCtx.lineTo(x + 5, currentY + 9);
            timelineCtx.lineTo(x - 5, currentY + 9);
            timelineCtx.closePath();
            timelineCtx.fill();

            // Vertical dotted line across canvas
            timelineCtx.strokeStyle = 'rgba(210, 153, 34, 0.4)';
            timelineCtx.setLineDash([2, 4]);
            timelineCtx.beginPath();
            timelineCtx.moveTo(x, currentY + 10);
            timelineCtx.lineTo(x, height);
            timelineCtx.stroke();
            timelineCtx.setLineDash([]);
          }
        }
      }
      currentY += markersTrackHeight + state.trackPaddingBottom;
    }

    // CPU & GPU Tracks
    for (const track of state.tracks) {
      const isCollapsed = state.collapsedTracks.has(track.id);
      const depthCount = isCollapsed ? 1 : (track.maxDepth + 1);
      const zonesAreaHeight = isCollapsed ? 0 : (depthCount * (state.zoneHeight + state.zoneSpacing) + 6);
      const trackHeight = state.trackHeaderHeight + state.frameHeaderHeight + zonesAreaHeight + state.trackPaddingBottom;

      // Track bounding box for hit-testing
      track.renderY = currentY;
      track.renderHeight = trackHeight;

      // Viewport culling for track
      if (currentY + trackHeight >= state.rulerHeight && currentY <= height) {
        const isGpu = track.type === 'gpu';

        // 1. Horizontal Separator between threads/queues
        timelineCtx.fillStyle = '#30363d';
        timelineCtx.fillRect(0, currentY, width, 1);

        // 2. Track Header Background (just below the separator)
        timelineCtx.fillStyle = isGpu ? 'rgba(57, 197, 187, 0.12)' : '#161b22';
        timelineCtx.fillRect(0, currentY + 1, width, state.trackHeaderHeight - 1);

        // Header bottom subtle divider
        timelineCtx.fillStyle = isGpu ? 'rgba(57, 197, 187, 0.25)' : '#21262d';
        timelineCtx.fillRect(0, currentY + state.trackHeaderHeight, width, 1);

        // Chevron
        timelineCtx.fillStyle = isGpu ? '#39c5bb' : '#8b949e';
        timelineCtx.font = '11px sans-serif';
        timelineCtx.fillText(isCollapsed ? '▶' : '▼', 8, currentY + 15);

        // Track Name & Type Badge
        timelineCtx.fillStyle = isGpu ? '#39c5bb' : '#58a6ff';
        timelineCtx.font = 'bold 11px monospace';
        let displayName = track.name;
        if (isGpu) {
          if (displayName.startsWith('GPU: ')) {
            displayName = displayName.substring(5);
          } else if (displayName.startsWith('GPU:')) {
            displayName = displayName.substring(4);
          }
        }
        const trackTitle = isGpu ? `[GPU] ${displayName}` : displayName.toUpperCase();
        timelineCtx.fillText(trackTitle, 24, currentY + 15);

        timelineCtx.fillStyle = isGpu ? '#7ee787' : '#6e7681';
        timelineCtx.font = '10px monospace';
        timelineCtx.fillText(`(${track.zones.length} zones)`, 24 + timelineCtx.measureText(trackTitle).width + 8, currentY + 15);

        // Frame header lane background (below track header, above call stacks)
        timelineCtx.fillStyle = isGpu ? 'rgba(57, 197, 187, 0.02)' : 'rgba(88, 166, 255, 0.02)';
        timelineCtx.fillRect(0, currentY + state.trackHeaderHeight + 1, width, state.frameHeaderHeight - 1);

        // 3. Zones: Start vertically beneath the frame header lane
        if (!isCollapsed) {
          let lastDrawnPixel = -1;
          const query = state.searchQuery.trim().toLowerCase();

          for (const zone of track.zones) {
            const zStartX = nsToX(zone.start_ns);
            const zEndX = nsToX(zone.end_ns);
            const zWidth = Math.max(0.5, zEndX - zStartX);

            // Frustum Culling
            if (zEndX < 0 || zStartX > width) continue;

            const rowY = currentY + state.trackHeaderHeight + state.frameHeaderHeight + 4 + zone.depth * (state.zoneHeight + state.zoneSpacing);

            // LOD Culling: If sub-pixel, merge to prevent millions of fillRect calls
            const pixelBucket = Math.floor(zStartX);
            if (zWidth < 0.8 && pixelBucket === lastDrawnPixel) {
              continue;
            }
            lastDrawnPixel = pixelBucket;

            // Search matching
            const isMatch = query && zone.name.toLowerCase().includes(query);
            if (isMatch) state.searchResultsCount++;

            const isSelected = state.selectedZone === zone;
            const isHovered = state.hoveredZone === zone;

            // Base Zone Color
            timelineCtx.fillStyle = getZoneColor(zone.name, zone.category || track.type);
            timelineCtx.fillRect(zStartX, rowY, Math.max(1, zWidth), state.zoneHeight);

            // Highlight overlay for search, select, hover
            if (isMatch) {
              timelineCtx.strokeStyle = '#d29922';
              timelineCtx.lineWidth = 2;
              timelineCtx.strokeRect(zStartX, rowY, Math.max(1, zWidth), state.zoneHeight);
            } else if (isSelected) {
              timelineCtx.strokeStyle = '#58a6ff';
              timelineCtx.lineWidth = 2;
              timelineCtx.strokeRect(zStartX, rowY, Math.max(1, zWidth), state.zoneHeight);
            } else if (isHovered) {
              timelineCtx.fillStyle = 'rgba(255, 255, 255, 0.2)';
              timelineCtx.fillRect(zStartX, rowY, Math.max(1, zWidth), state.zoneHeight);
            }

            // Zone Text Label
            if (zWidth > 32) {
              timelineCtx.save();
              timelineCtx.beginPath();
              timelineCtx.rect(zStartX, rowY, zWidth, state.zoneHeight);
              timelineCtx.clip();

              timelineCtx.fillStyle = '#ffffff';
              timelineCtx.font = '10px monospace';
              const label = `${zone.name} (${formatTime(zone.duration_ns)})`;
              timelineCtx.fillText(label, zStartX + 4, rowY + 12);
              timelineCtx.restore();
            }
          }
        }
      }

      currentY += trackHeight;
    }

    state.totalContentHeight = currentY + state.scrollY;

    // 3. Draw Frame Boundaries (CPU vs GPU frames & correlation)
    renderFrameBoundaries(timelineCtx, width, height, nsToX, visibleDuration);

    // 4. Draw Marquee Selection
    if (state.selectionRange) {
      const selStartX = nsToX(state.selectionRange.startNs);
      const selEndX = nsToX(state.selectionRange.endNs);
      const selX = Math.min(selStartX, selEndX);
      const selW = Math.abs(selEndX - selStartX);

      timelineCtx.fillStyle = 'rgba(130, 87, 230, 0.18)';
      timelineCtx.fillRect(selX, state.rulerHeight, selW, height - state.rulerHeight);

      timelineCtx.strokeStyle = '#8257e6';
      timelineCtx.lineWidth = 1.5;
      timelineCtx.strokeRect(selX, state.rulerHeight, selW, height - state.rulerHeight);
    }

    // 5. Draw Sticky Top Ruler
    renderRuler(timelineCtx, width, nsToX, visibleDuration);

    // Update search count badge
    if (state.searchQuery) {
      dom.searchCount.textContent = `${state.searchResultsCount} matches`;
    }
  }

  function renderFrameBoundaries(ctx, width, height, nsToX, visibleDuration) {
    if (!state.showFrameBoundaries || state.frames.length === 0) return;

    // Find vertical bounds for CPU and GPU tracks
    let cpuMinY = Infinity;
    let cpuMaxY = -Infinity;
    let gpuMinY = Infinity;
    let gpuMaxY = -Infinity;

    for (const track of state.tracks) {
      if (track.renderY === undefined || track.renderHeight === undefined) continue;
      if (track.type === 'gpu') {
        if (track.renderY < gpuMinY) gpuMinY = track.renderY;
        if (track.renderY + track.renderHeight > gpuMaxY) gpuMaxY = track.renderY + track.renderHeight;
      } else {
        if (track.renderY < cpuMinY) cpuMinY = track.renderY;
        if (track.renderY + track.renderHeight > cpuMaxY) cpuMaxY = track.renderY + track.renderHeight;
      }
    }

    if (cpuMinY === Infinity) {
      cpuMinY = state.rulerHeight;
      cpuMaxY = height / 2;
    }
    if (gpuMinY === Infinity) {
      gpuMinY = cpuMaxY;
      gpuMaxY = height;
    }

    // Clip to area below ruler
    ctx.save();
    ctx.beginPath();
    ctx.rect(0, state.rulerHeight, width, height - state.rulerHeight);
    ctx.clip();

    for (const frame of state.frames) {
      const isHovered = (state.hoveredFrameIndex === frame.frame_index);

      // 1. CPU Frame Boundary
      if (frame.cpuStartNs !== null && frame.cpuEndNs !== null) {
        const x0 = nsToX(frame.cpuStartNs);
        const x1 = nsToX(frame.cpuEndNs);
        const w = Math.max(1, x1 - x0);

        if (x1 >= 0 && x0 <= width) {
          ctx.fillStyle = isHovered ? 'rgba(136, 80, 255, 0.16)' : 'rgba(88, 166, 255, 0.04)';
          ctx.fillRect(x0, cpuMinY, w, Math.max(20, cpuMaxY - cpuMinY));

          ctx.strokeStyle = isHovered ? 'rgba(163, 113, 247, 0.95)' : 'rgba(88, 166, 255, 0.25)';
          ctx.lineWidth = isHovered ? 1.5 : 1;
          ctx.setLineDash(isHovered ? [] : [3, 3]);
          ctx.strokeRect(x0, cpuMinY, w, Math.max(20, cpuMaxY - cpuMinY));
          ctx.setLineDash([]);

          if (w > 20 || isHovered) {
            const tag = `CPU #${frame.frame_index} (${formatTime(frame.cpuDurationNs)})`;
            ctx.font = 'bold 9px monospace';
            const tagW = ctx.measureText(tag).width;
            const tagY = cpuMinY + state.trackHeaderHeight + 13;

            ctx.fillStyle = isHovered ? 'rgba(22, 27, 34, 0.95)' : 'rgba(22, 27, 34, 0.85)';
            ctx.fillRect(x0 + 2, tagY - 10, tagW + 6, 14);
            ctx.strokeStyle = isHovered ? '#a371f7' : 'rgba(88, 166, 255, 0.5)';
            ctx.lineWidth = 1;
            ctx.strokeRect(x0 + 2, tagY - 10, tagW + 6, 14);

            ctx.fillStyle = isHovered ? '#a371f7' : 'rgba(88, 166, 255, 0.95)';
            ctx.fillText(tag, x0 + 5, tagY);
          }
        }
      }

      // 2. GPU Frame Boundary
      if (frame.gpuStartNs !== null && frame.gpuEndNs !== null) {
        const gx0 = nsToX(frame.gpuStartNs);
        const gx1 = nsToX(frame.gpuEndNs);
        const gw = Math.max(1, gx1 - gx0);

        if (gx1 >= 0 && gx0 <= width) {
          ctx.fillStyle = isHovered ? 'rgba(57, 197, 187, 0.20)' : 'rgba(57, 197, 187, 0.05)';
          ctx.fillRect(gx0, gpuMinY, gw, Math.max(20, gpuMaxY - gpuMinY));

          ctx.strokeStyle = isHovered ? 'rgba(57, 197, 187, 1.0)' : 'rgba(57, 197, 187, 0.35)';
          ctx.lineWidth = isHovered ? 1.5 : 1;
          ctx.setLineDash(isHovered ? [] : [4, 3]);
          ctx.strokeRect(gx0, gpuMinY, gw, Math.max(20, gpuMaxY - gpuMinY));
          ctx.setLineDash([]);

          if (gw > 20 || isHovered) {
            const tag = `GPU #${frame.frame_index} (${formatTime(frame.gpuDurationNs)})`;
            ctx.font = 'bold 9px monospace';
            const tagW = ctx.measureText(tag).width;
            const gtagY = gpuMinY + state.trackHeaderHeight + 13;

            ctx.fillStyle = isHovered ? 'rgba(22, 27, 34, 0.95)' : 'rgba(22, 27, 34, 0.85)';
            ctx.fillRect(gx0 + 2, gtagY - 10, tagW + 6, 14);
            ctx.strokeStyle = isHovered ? '#39c5bb' : 'rgba(57, 197, 187, 0.5)';
            ctx.lineWidth = 1;
            ctx.strokeRect(gx0 + 2, gtagY - 10, tagW + 6, 14);

            ctx.fillStyle = isHovered ? '#39c5bb' : 'rgba(57, 197, 187, 0.95)';
            ctx.fillText(tag, gx0 + 5, gtagY);
          }
        }
      }

      // 3. Correlation Indicator for Hovered Frame
      if (isHovered && frame.cpuEndNs !== null && frame.gpuStartNs !== null) {
        const cpuEndX = nsToX(frame.cpuEndNs);
        const gpuStartX = nsToX(frame.gpuStartNs);
        const midY = (cpuMaxY + gpuMinY) / 2;

        ctx.strokeStyle = '#f0883e';
        ctx.lineWidth = 1.5;
        ctx.setLineDash([2, 2]);
        ctx.beginPath();
        ctx.moveTo(cpuEndX, cpuMaxY);
        ctx.lineTo(cpuEndX, midY);
        ctx.lineTo(gpuStartX, midY);
        ctx.lineTo(gpuStartX, gpuMinY);
        ctx.stroke();
        ctx.setLineDash([]);

        const latencyNs = frame.gpuStartNs - frame.cpuStartNs;
        ctx.font = 'bold 9px monospace';
        const latencyLabel = `Flight: ${formatTime(latencyNs)}`;
        const textW = ctx.measureText(latencyLabel).width;
        const textX = Math.min(cpuEndX, gpuStartX) + Math.abs(gpuStartX - cpuEndX) / 2 - textW / 2;

        ctx.fillStyle = 'rgba(22, 27, 34, 0.9)';
        ctx.fillRect(textX - 3, midY - 12, textW + 6, 14);
        ctx.strokeStyle = '#f0883e';
        ctx.lineWidth = 1;
        ctx.strokeRect(textX - 3, midY - 12, textW + 6, 14);

        ctx.fillStyle = '#f0883e';
        ctx.fillText(latencyLabel, textX, midY - 2);
      }
    }

    ctx.restore();
  }

  function renderGrid(ctx, width, height, nsToX, visibleDuration) {
    const stepNs = calculateNiceTimeStep(visibleDuration, width);
    const startStep = Math.floor(state.viewStartNs / stepNs) * stepNs;

    ctx.strokeStyle = 'rgba(48, 54, 61, 0.5)';
    ctx.lineWidth = 1;

    for (let ns = startStep; ns <= state.viewEndNs; ns += stepNs) {
      const x = nsToX(ns);
      if (x < 0 || x > width) continue;

      ctx.beginPath();
      ctx.moveTo(x, state.rulerHeight);
      ctx.lineTo(x, height);
      ctx.stroke();
    }
  }

  function renderRuler(ctx, width, nsToX, visibleDuration) {
    // Ruler Background
    ctx.fillStyle = '#161b22';
    ctx.fillRect(0, 0, width, state.rulerHeight);

    ctx.strokeStyle = '#30363d';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, state.rulerHeight);
    ctx.lineTo(width, state.rulerHeight);
    ctx.stroke();

    const stepNs = calculateNiceTimeStep(visibleDuration, width);
    const startStep = Math.floor(state.viewStartNs / stepNs) * stepNs;

    ctx.fillStyle = '#8b949e';
    ctx.font = '10px monospace';
    ctx.textBaseline = 'middle';

    for (let ns = startStep; ns <= state.viewEndNs; ns += stepNs) {
      const x = nsToX(ns);
      if (x < 0 || x > width) continue;

      // Tick mark
      ctx.strokeStyle = '#6e7681';
      ctx.beginPath();
      ctx.moveTo(x, state.rulerHeight - 6);
      ctx.lineTo(x, state.rulerHeight);
      ctx.stroke();

      // Label
      const timeStr = formatRelativeTime(ns);
      ctx.fillText(timeStr, x + 4, state.rulerHeight / 2);
    }
  }

  function calculateNiceTimeStep(visibleDurationNs, width) {
    const minPixelDistance = 80;
    const targetSteps = Math.max(2, Math.floor(width / minPixelDistance));
    const rawStep = visibleDurationNs / targetSteps;

    const niceSteps = [
      1, 2, 5, 10, 20, 50, 100, 200, 500,                    // ns
      1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, // µs
      1000000, 2000000, 5000000, 10000000, 20000000, 50000000, 100000000, 200000000, 500000000, // ms
      1000000000, 2000000000, 5000000000, 10000000000, 30000000000, 60000000000 // s
    ];

    for (let i = 0; i < niceSteps.length; i++) {
      if (niceSteps[i] >= rawStep) {
        return niceSteps[i];
      }
    }
    return niceSteps[niceSteps.length - 1];
  }

  function renderOverview() {
    const { width, height } = resizeCanvas(dom.overviewCanvas, overviewCtx);
    overviewCtx.clearRect(0, 0, width, height);

    if (width <= 0 || height <= 0) return;

    const totalDuration = state.maxTimeNs - state.minTimeNs;
    if (totalDuration <= 0) return;

    const fullNsToX = (ns) => ((ns - state.minTimeNs) / totalDuration) * width;

    // Background track density
    overviewCtx.fillStyle = '#21262d';
    for (const track of state.tracks) {
      for (const z of track.zones) {
        const x = fullNsToX(z.start_ns);
        const w = Math.max(1, fullNsToX(z.end_ns) - x);
        overviewCtx.fillStyle = getZoneColor(z.name, z.category);
        overviewCtx.fillRect(x, 2, w, height - 4);
      }
    }

    // Viewport Brush Window
    const brushStartX = Math.max(0, fullNsToX(state.viewStartNs));
    const brushEndX = Math.min(width, fullNsToX(state.viewEndNs));
    const brushWidth = Math.max(4, brushEndX - brushStartX);

    // Dim regions outside brush
    overviewCtx.fillStyle = 'rgba(13, 17, 23, 0.65)';
    overviewCtx.fillRect(0, 0, brushStartX, height);
    overviewCtx.fillRect(brushEndX, 0, width - brushEndX, height);

    // Brush Outline
    overviewCtx.strokeStyle = '#58a6ff';
    overviewCtx.lineWidth = 1.5;
    overviewCtx.strokeRect(brushStartX, 0, brushWidth, height);

    // Update Overview Info text
    dom.overviewTimeRange.textContent = `${formatRelativeTime(state.minTimeNs)} - ${formatRelativeTime(state.maxTimeNs)} (Total: ${formatTime(totalDuration)})`;
    let totalZonesCount = 0;
    for (const tr of state.tracks) totalZonesCount += tr.zones.length;
    dom.overviewFrameStats.textContent = `Frames: ${state.frames.length} | Tracks: ${state.tracks.length} | Zones: ${totalZonesCount}`;
  }

  // =========================================================================
  // Zone Detail Inspector & Self-Time Calculation
  // =========================================================================

  function findParentFrame(zone) {
    if (!zone) return null;
    if (zone.frame_index !== undefined && zone.frame_index !== null && zone.frame_index !== 0) {
      const found = state.frames.find(f => f.frame_index === zone.frame_index);
      if (found) return found;
    }
    let found = state.frames.find(f => {
      if (f.cpu_tracks) {
        for (const t of f.cpu_tracks) {
          if (t.zones && t.zones.some(pz => pz === zone || (pz.name === zone.name && pz.start_ns === zone.start_ns))) return true;
        }
      }
      if (f.gpu_tracks) {
        for (const t of f.gpu_tracks) {
          if (t.zones && t.zones.some(pz => pz === zone || (pz.name === zone.name && pz.start_ns === zone.start_ns))) return true;
        }
      }
      return false;
    });
    if (found) return found;

    return state.frames.find(f => {
      const inCpu = f.cpuStartNs !== null && f.cpuEndNs !== null && zone.start_ns >= f.cpuStartNs && zone.start_ns <= f.cpuEndNs;
      const inGpu = f.gpuStartNs !== null && f.gpuEndNs !== null && zone.start_ns >= f.gpuStartNs && zone.start_ns <= f.gpuEndNs;
      return inCpu || inGpu;
    }) || null;
  }

  function getFrameDurationForZone(zone, parentFrame) {
    if (parentFrame) {
      const isGpu = zone.category === 'gpu' || (zone.trackName && zone.trackName.toLowerCase().includes('gpu'));
      if (isGpu && parentFrame.gpuDurationNs > 0) {
        return parentFrame.gpuDurationNs;
      }
      if (!isGpu && parentFrame.cpuDurationNs > 0) {
        return parentFrame.cpuDurationNs;
      }
      if (parentFrame.cpuDurationNs > 0) return parentFrame.cpuDurationNs;
      if (parentFrame.gpuDurationNs > 0) return parentFrame.gpuDurationNs;
    }
    const track = state.tracks.find(t => t.id === zone.trackId);
    if (track && track.zones) {
      const rootZone = track.zones.find(z => z.depth === 0 && zone.start_ns >= z.start_ns && zone.end_ns <= z.end_ns);
      if (rootZone && rootZone.duration_ns > 0) {
        return rootZone.duration_ns;
      }
    }
    return 0;
  }

  function selectZone(zone) {
    state.selectedZone = zone;
    if (!zone) {
      dom.inspectorPlaceholder.style.display = 'flex';
      dom.inspectorDetails.style.display = 'none';
      render();
      return;
    }

    dom.inspectorPlaceholder.style.display = 'none';
    dom.inspectorDetails.style.display = 'flex';

    // Header
    dom.inspCategory.textContent = (zone.category || 'cpu').toUpperCase();
    dom.inspCategory.className = `category-pill ${zone.category || 'cpu'}`;
    dom.inspName.textContent = zone.name;
    dom.inspTrack.textContent = `${zone.trackName || 'Thread'} (Depth ${zone.depth})`;
    dom.inspLocation.textContent = zone.location ? `${zone.location.file || 'source.cpp'}:${zone.location.line || 0}` : 'native';

    // Duration & Self-Time
    const totalDur = zone.duration_ns;
    dom.inspDuration.textContent = formatTime(totalDur);

    // Calculate Self-Time: Total duration minus all direct child zones
    const selfTime = calculateSelfTime(zone);
    const selfPct = totalDur > 0 ? ((selfTime / totalDur) * 100).toFixed(1) : '100.0';
    dom.inspSelfTime.textContent = `${formatTime(selfTime)} (${selfPct}%)`;

    // Percentage of Frame
    const parentFrame = findParentFrame(zone);
    const frameDur = getFrameDurationForZone(zone, parentFrame);
    const framePct = frameDur > 0 ? ((totalDur / frameDur) * 100).toFixed(2) : '0.00';
    dom.inspFramePct.textContent = `${framePct}%`;

    dom.inspDepth.textContent = `Level ${zone.depth}`;

    // Attached Metrics & GPU Stats
    dom.inspMetricsList.innerHTML = '';
    if (zone.metrics && zone.metrics.length > 0) {
      for (const m of zone.metrics) {
        const tag = document.createElement('div');
        tag.className = 'metric-tag';
        tag.innerHTML = `<span class="tag-name">${m.name}:</span><span class="tag-value">${m.value.toLocaleString()}</span>`;
        dom.inspMetricsList.appendChild(tag);
      }
    } else {
      dom.inspMetricsList.innerHTML = '<span class="empty-text">No custom metrics attached to this zone.</span>';
    }

    // Build Call Tree (Parent and Direct Children)
    buildHierarchyTree(zone);

    render();
  }

  function calculateSelfTime(zone) {
    if (!zone) return 0;
    const track = state.tracks.find(t => t.id === zone.trackId);
    if (!track) return zone.duration_ns;

    let childrenDurSum = 0;
    for (const other of track.zones) {
      // Direct child: same track, depth = zone.depth + 1, completely inside zone time bounds
      if (other.depth === zone.depth + 1 && other.start_ns >= zone.start_ns && other.end_ns <= zone.end_ns) {
        childrenDurSum += other.duration_ns;
      }
    }

    return Math.max(0, zone.duration_ns - childrenDurSum);
  }

  function buildHierarchyTree(zone) {
    dom.inspHierarchyTree.innerHTML = '';
    const track = state.tracks.find(t => t.id === zone.trackId);
    if (!track) return;

    // Find parent zone
    let parentZone = null;
    if (zone.depth > 0) {
      for (const other of track.zones) {
        if (other.depth === zone.depth - 1 && other.start_ns <= zone.start_ns && other.end_ns >= zone.end_ns) {
          parentZone = other;
          break;
        }
      }
    }

    if (parentZone) {
      const pItem = document.createElement('div');
      pItem.className = 'tree-item';
      pItem.innerHTML = `<span class="tree-role">▲ Parent:</span><span class="tree-name">${parentZone.name}</span><span class="tree-dur">${formatTime(parentZone.duration_ns)}</span>`;
      pItem.onclick = () => selectZone(parentZone);
      dom.inspHierarchyTree.appendChild(pItem);
    }

    // Current zone
    const curItem = document.createElement('div');
    curItem.className = 'tree-item';
    curItem.style.background = 'rgba(130, 87, 230, 0.15)';
    curItem.innerHTML = `<span class="tree-role">● Self:</span><span class="tree-name" style="font-weight:bold;">${zone.name}</span><span class="tree-dur">${formatTime(zone.duration_ns)}</span>`;
    dom.inspHierarchyTree.appendChild(curItem);

    // Direct children
    const children = track.zones.filter(other =>
      other.depth === zone.depth + 1 && other.start_ns >= zone.start_ns && other.end_ns <= zone.end_ns
    );

    if (children.length > 0) {
      for (const c of children) {
        const cItem = document.createElement('div');
        cItem.className = 'tree-item';
        cItem.innerHTML = `<span class="tree-role">▼ Child:</span><span class="tree-name">${c.name}</span><span class="tree-dur">${formatTime(c.duration_ns)}</span>`;
        cItem.onclick = () => selectZone(c);
        dom.inspHierarchyTree.appendChild(cItem);
      }
    }
  }

  // =========================================================================
  // Statistical Table & Duration Histogram
  // =========================================================================

  function renderStatsTable() {
    const filter = dom.statsFilter.value.trim().toLowerCase();
    const rows = Array.from(state.zoneStats.values()).filter(s =>
      !filter || s.name.toLowerCase().includes(filter)
    );

    // Sort rows
    rows.sort((a, b) => {
      let valA = a[state.sortColumn] ?? 0;
      let valB = b[state.sortColumn] ?? 0;
      if (typeof valA === 'string') {
        return state.sortAscending ? valA.localeCompare(valB) : valB.localeCompare(valA);
      }
      return state.sortAscending ? (valA - valB) : (valB - valA);
    });

    dom.statsTotalCount.textContent = `${rows.length} unique zones`;

    if (rows.length === 0) {
      dom.statsTbody.innerHTML = '<tr><td colspan="12" class="empty-cell">No matching zones found.</td></tr>';
      return;
    }

    dom.statsTbody.innerHTML = '';
    for (const s of rows) {
      const tr = document.createElement('tr');
      if (state.selectedStatsRow === s.name) {
        tr.className = 'selected';
      }

      tr.innerHTML = `
        <td style="color: ${getZoneColor(s.name, s.category)}; font-weight: bold;">${s.name}</td>
        <td><span class="category-pill ${s.category}">${s.category.toUpperCase()}</span></td>
        <td class="num">${s.count.toLocaleString()}</td>
        <td class="num" style="color: var(--accent-cyan); font-weight: 600;">${formatTime(s.totalNs)}</td>
        <td class="num">${formatTime(s.meanNs)}</td>
        <td class="num">${formatTime(s.minNs)}</td>
        <td class="num">${formatTime(s.maxNs)}</td>
        <td class="num">${formatTime(s.p50Ns)}</td>
        <td class="num">${formatTime(s.p90Ns)}</td>
        <td class="num">${formatTime(s.p95Ns)}</td>
        <td class="num" style="color: var(--accent-yellow); font-weight: 600;">${formatTime(s.p99Ns)}</td>
        <td class="num">${formatTime(s.stdDevNs)}</td>
      `;

      tr.onclick = () => {
        state.selectedStatsRow = s.name;
        state.searchQuery = s.name;
        dom.searchInput.value = s.name;
        dom.searchClearBtn.style.display = 'block';
        renderStatsTable();
        renderHistogram(s);
        render();
      };

      dom.statsTbody.appendChild(tr);
    }
  }

  function renderHistogram(statsObj) {
    if (!statsObj || !statsObj.durations || statsObj.durations.length === 0) {
      dom.distTitle.textContent = 'Duration Distribution Histogram';
      dom.distSubtitle.textContent = 'No samples available for selected zone';
      const { width, height } = resizeCanvas(dom.histogramCanvas, histogramCtx);
      histogramCtx.clearRect(0, 0, width, height);
      return;
    }

    dom.distTitle.textContent = `Duration Distribution: ${statsObj.name}`;
    dom.distSubtitle.textContent = `${statsObj.durations.length} samples | Min: ${formatTime(statsObj.minNs)} | Mean: ${formatTime(statsObj.meanNs)} | Max: ${formatTime(statsObj.maxNs)}`;

    const { width, height } = resizeCanvas(dom.histogramCanvas, histogramCtx);
    histogramCtx.clearRect(0, 0, width, height);

    const bucketCount = Math.min(30, Math.max(10, Math.floor(width / 20)));
    const minVal = statsObj.minNs;
    const maxVal = Math.max(minVal + 1, statsObj.maxNs);
    const range = maxVal - minVal;
    const bucketSize = range / bucketCount;

    const buckets = new Array(bucketCount).fill(0);
    for (const d of statsObj.durations) {
      const idx = Math.min(bucketCount - 1, Math.floor((d - minVal) / bucketSize));
      buckets[idx]++;
    }

    const maxCount = Math.max(...buckets, 1);
    const barWidth = (width - 40) / bucketCount;

    histogramCtx.fillStyle = '#8257e6';
    for (let i = 0; i < bucketCount; i++) {
      const count = buckets[i];
      const barHeight = (count / maxCount) * (height - 24);
      const x = 30 + i * barWidth;
      const y = height - 16 - barHeight;

      histogramCtx.fillRect(x, y, Math.max(1, barWidth - 2), barHeight);
    }

    // Baseline & Axis
    histogramCtx.strokeStyle = '#30363d';
    histogramCtx.beginPath();
    histogramCtx.moveTo(28, height - 16);
    histogramCtx.lineTo(width, height - 16);
    histogramCtx.stroke();

    histogramCtx.fillStyle = '#8b949e';
    histogramCtx.font = '9px monospace';
    histogramCtx.fillText(formatTime(minVal), 30, height - 4);
    histogramCtx.fillText(formatTime(maxVal), width - 60, height - 4);
  }

  // =========================================================================
  // Metric Graphs
  // =========================================================================

  function updateMetricCards() {
    const renderMiniChart = (ctx, canvas, values, color, labelEl, unitStr) => {
      const { width, height } = resizeCanvas(canvas, ctx);
      ctx.clearRect(0, 0, width, height);

      if (!values || values.length === 0) {
        if (labelEl) labelEl.textContent = `-- ${unitStr}`;
        return;
      }

      const latest = values[values.length - 1];
      if (labelEl) labelEl.textContent = `${latest.toFixed(1)} ${unitStr}`;

      const min = Math.min(...values);
      const max = Math.max(...values, min + 1);
      const range = max - min;

      ctx.strokeStyle = color;
      ctx.lineWidth = 1.5;
      ctx.beginPath();

      for (let i = 0; i < values.length; i++) {
        const x = (i / (values.length - 1 || 1)) * width;
        const y = height - ((values[i] - min) / range) * (height - 10) - 5;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    };

    // Calculate synthetic or real series
    const frameTimes = state.frames.map(f => {
      let dur = 0;
      if (f.cpu_tracks && f.cpu_tracks.length > 0) {
        for (const t of f.cpu_tracks) {
          if (t.zones) {
            for (const z of t.zones) {
              const zdur = (z.end_ns >= z.start_ns) ? (z.end_ns - z.start_ns) : 0;
              if (zdur > dur) dur = zdur;
            }
          }
        }
      }
      return dur > 0 ? dur / 1000000 : 16.6;
    });

    const fpsSeries = frameTimes.map(ft => ft > 0 ? Math.min(240, 1000 / ft) : 60);

    const memorySeries = state.frames.map(f => {
      let vramMb = 0;
      if (f.metrics) {
        for (const m of f.metrics) {
          if (m.name && (m.name.toLowerCase().includes('vram') || m.name.toLowerCase().includes('memory') || m.name.toLowerCase().includes('bytes'))) {
            vramMb = Math.max(vramMb, m.value / (1024 * 1024));
          }
        }
      }
      return vramMb;
    });

    const gpuPassDurations = state.frames.map(f => {
      let totalGpuNs = 0;
      if (f.gpu_tracks && f.gpu_tracks.length > 0) {
        for (const t of f.gpu_tracks) {
          if (t.zones) {
            for (const z of t.zones) {
              const dur = (z.end_ns >= z.start_ns) ? (z.end_ns - z.start_ns) : 0;
              totalGpuNs += dur;
            }
          }
        }
      }
      return totalGpuNs / 1000000;
    });

    renderMiniChart(fpsCtx, dom.chartFps, fpsSeries, '#3fb950', dom.metricFpsVal, 'FPS');
    renderMiniChart(frametimeCtx, dom.chartFrametime, frameTimes, '#58a6ff', dom.metricFrametimeVal, 'ms');
    renderMiniChart(memoryCtx, dom.chartMemory, memorySeries, '#d29922', dom.metricMemoryVal, 'MB');
    renderMiniChart(gpuCtx, dom.chartGpu, gpuPassDurations, '#39c5bb', dom.metricGpuVal, 'ms');
  }

  function updateInfoMetadata() {
    dom.infoTotalFrames.textContent = state.frames.length.toString();
    let totalZones = 0;
    for (const tr of state.tracks) totalZones += tr.zones.length;
    dom.infoTotalZones.textContent = totalZones.toLocaleString();
    dom.infoTotalTracks.textContent = state.tracks.length.toString();
    dom.infoDuration.textContent = formatTime(state.maxTimeNs - state.minTimeNs);
    dom.infoTimeExtent.textContent = `${formatRelativeTime(state.minTimeNs)} - ${formatRelativeTime(state.maxTimeNs)}`;
  }

  // =========================================================================
  // User Input & Canvas Interaction Handlers
  // =========================================================================

  function initInteraction() {
    const canvas = dom.timelineCanvas;

    // Mouse Wheel Zoom
    canvas.addEventListener('wheel', (e) => {
      e.preventDefault();
      const rect = canvas.getBoundingClientRect();
      const mouseX = e.clientX - rect.left;
      const visibleDuration = state.viewEndNs - state.viewStartNs;
      const mouseTimeNs = state.viewStartNs + (mouseX / rect.width) * visibleDuration;

      const zoomFactor = e.deltaY < 0 ? 0.75 : 1.33;
      const newDuration = Math.max(100, Math.min(1e12, visibleDuration * zoomFactor));

      const mouseRatio = mouseX / rect.width;
      state.viewStartNs = mouseTimeNs - mouseRatio * newDuration;
      state.viewEndNs = state.viewStartNs + newDuration;

      // Pause live auto-scroll when user zooms
      if (state.isLive) {
        toggleLiveStreaming(false);
      }

      render();
    }, { passive: false });

    // Mouse Down
    canvas.addEventListener('mousedown', (e) => {
      const rect = canvas.getBoundingClientRect();
      const mouseX = e.clientX - rect.left;
      const mouseY = e.clientY - rect.top;

      state.isDragging = true;
      state.dragStartX = mouseX;
      state.dragStartY = mouseY;
      state.dragStartViewStart = state.viewStartNs;
      state.dragStartViewEnd = state.viewEndNs;
      state.dragStartScrollY = state.scrollY;

      if (e.shiftKey) {
        state.isSelectingRange = true;
        const clickNs = state.viewStartNs + (mouseX / rect.width) * (state.viewEndNs - state.viewStartNs);
        state.selectionRange = { startNs: clickNs, endNs: clickNs };
      } else {
        state.isSelectingRange = false;
      }
    });

    // Mouse Move
    window.addEventListener('mousemove', (e) => {
      const rect = canvas.getBoundingClientRect();
      const mouseX = e.clientX - rect.left;
      const mouseY = e.clientY - rect.top;

      if (state.isDragging) {
        const deltaX = mouseX - state.dragStartX;
        const deltaY = mouseY - state.dragStartY;

        if (state.isSelectingRange) {
          const curNs = state.viewStartNs + (mouseX / rect.width) * (state.viewEndNs - state.viewStartNs);
          state.selectionRange.endNs = curNs;
          updateSelectionBadge();
        } else {
          // Pan horizontally in time
          const visibleDuration = state.dragStartViewEnd - state.dragStartViewStart;
          const nsPerPixel = visibleDuration / rect.width;
          state.viewStartNs = state.dragStartViewStart - deltaX * nsPerPixel;
          state.viewEndNs = state.dragStartViewEnd - deltaX * nsPerPixel;

          // Pan vertically (scroll tracks)
          state.scrollY = Math.max(0, state.dragStartScrollY - deltaY);

          if (state.isLive && Math.abs(deltaX) > 5) {
            toggleLiveStreaming(false);
          }
        }
        render();
      } else {
        // Hover detection
        handleHover(mouseX, mouseY);
      }
    });

    // Mouse Up
    window.addEventListener('mouseup', (e) => {
      if (state.isDragging) {
        const rect = canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;
        const moveDist = Math.hypot(mouseX - state.dragStartX, mouseY - state.dragStartY);

        if (!state.isSelectingRange && moveDist < 4) {
          // Click event: Hit test zone or track header
          handleClick(mouseX, mouseY);
        }

        state.isDragging = false;
        render();
      }
    });

    // Overview Minimap Drag
    dom.overviewCanvas.addEventListener('mousedown', (e) => {
      state.isOverviewDragging = true;
      handleOverviewClick(e);
    });

    window.addEventListener('mousemove', (e) => {
      if (state.isOverviewDragging) {
        handleOverviewClick(e);
      }
    });

    window.addEventListener('mouseup', () => {
      state.isOverviewDragging = false;
    });

    function handleOverviewClick(e) {
      const rect = dom.overviewCanvas.getBoundingClientRect();
      const mouseX = Math.max(0, Math.min(rect.width, e.clientX - rect.left));
      const totalDuration = state.maxTimeNs - state.minTimeNs;
      if (totalDuration <= 0) return;

      const clickNs = state.minTimeNs + (mouseX / rect.width) * totalDuration;
      const curDur = state.viewEndNs - state.viewStartNs;

      state.viewStartNs = clickNs - curDur / 2;
      state.viewEndNs = state.viewStartNs + curDur;

      if (state.isLive) toggleLiveStreaming(false);
      render();
    }

    // Dock Resizing
    let isResizingDock = false;
    let resizeStartY = 0;
    let resizeStartHeight = 0;

    dom.dockResizeHandle.addEventListener('mousedown', (e) => {
      isResizingDock = true;
      resizeStartY = e.clientY;
      resizeStartHeight = dom.dockSection.offsetHeight;
      document.body.style.cursor = 'row-resize';
    });

    window.addEventListener('mousemove', (e) => {
      if (!isResizingDock) return;
      const deltaY = resizeStartY - e.clientY;
      const newHeight = Math.max(36, Math.min(window.innerHeight - 100, resizeStartHeight + deltaY));
      dom.dockSection.style.height = `${newHeight}px`;
    });

    window.addEventListener('mouseup', () => {
      if (isResizingDock) {
        isResizingDock = false;
        document.body.style.cursor = 'default';
      }
    });
  }

  function handleHover(mouseX, mouseY) {
    if (mouseY < state.rulerHeight) {
      dom.tooltip.style.display = 'none';
      state.hoveredZone = null;
      state.hoveredFrameIndex = null;
      render();
      return;
    }

    const hit = hitTest(mouseX, mouseY);
    if (hit && hit.type === 'zone') {
      state.hoveredZone = hit.zone;
      const z = hit.zone;

      // Find corresponding frame by exact frame_index or zone timestamp match
      const parentFrame = findParentFrame(z);
      state.hoveredFrameIndex = parentFrame ? parentFrame.frame_index : null;

      dom.tooltip.style.display = 'block';
      dom.tooltip.style.left = `${Math.min(window.innerWidth - 300, mouseX + 16)}px`;
      dom.tooltip.style.top = `${Math.min(window.innerHeight - 150, mouseY + 16)}px`;

      dom.tooltip.innerHTML = `
        <div class="tt-title">${z.name}</div>
        <div class="tt-row"><span>Duration:</span><span class="tt-val">${formatTime(z.duration_ns)}</span></div>
        <div class="tt-row"><span>Start Time:</span><span class="tt-val">${formatRelativeTime(z.start_ns)}</span></div>
        <div class="tt-row"><span>Track:</span><span>${z.trackName || 'Main'}</span></div>
        <div class="tt-row"><span>Depth:</span><span>${z.depth}</span></div>
        ${parentFrame ? `<div class="tt-row"><span>Frame:</span><span class="tt-val">#${parentFrame.frame_index}</span></div>` : ''}
      `;
    } else {
      // Check if mouseX is inside any frame boundary box in empty track space
      const width = dom.timelineCanvas.width / (window.devicePixelRatio || 1);
      const visibleDuration = state.viewEndNs - state.viewStartNs;
      const mouseNs = state.viewStartNs + (mouseX / width) * visibleDuration;

      const hoveredFrame = state.frames.find(f => {
        const inCpu = f.cpuStartNs !== null && f.cpuEndNs !== null && mouseNs >= f.cpuStartNs && mouseNs <= f.cpuEndNs;
        const inGpu = f.gpuStartNs !== null && f.gpuEndNs !== null && mouseNs >= f.gpuStartNs && mouseNs <= f.gpuEndNs;
        return inCpu || inGpu;
      });

      dom.tooltip.style.display = 'none';
      state.hoveredZone = null;
      state.hoveredFrameIndex = hoveredFrame ? hoveredFrame.frame_index : null;
    }
    render();
  }

  function handleClick(mouseX, mouseY) {
    const hit = hitTest(mouseX, mouseY);
    if (!hit) {
      selectZone(null);
      return;
    }

    if (hit.type === 'header') {
      // Toggle track collapse
      if (state.collapsedTracks.has(hit.track.id)) {
        state.collapsedTracks.delete(hit.track.id);
      } else {
        state.collapsedTracks.add(hit.track.id);
      }
      render();
    } else if (hit.type === 'zone') {
      selectZone(hit.zone);
      // Switch to Inspector tab
      switchTab('inspector');
    }
  }

  function hitTest(mouseX, mouseY) {
    const width = dom.timelineCanvas.width / (window.devicePixelRatio || 1);
    const visibleDuration = state.viewEndNs - state.viewStartNs;
    const nsToX = (ns) => ((ns - state.viewStartNs) / visibleDuration) * width;

    let currentY = state.rulerHeight - state.scrollY;

    // Markers track hit test
    if (state.markers.length > 0) {
      currentY += 22 + state.trackPaddingBottom;
    }

    for (const track of state.tracks) {
      const isCollapsed = state.collapsedTracks.has(track.id);
      const depthCount = isCollapsed ? 1 : (track.maxDepth + 1);
      const zonesAreaHeight = isCollapsed ? 0 : (depthCount * (state.zoneHeight + state.zoneSpacing) + 6);
      const trackHeight = state.trackHeaderHeight + state.frameHeaderHeight + zonesAreaHeight + state.trackPaddingBottom;

      // Check header hit
      if (mouseY >= currentY && mouseY < currentY + state.trackHeaderHeight) {
        return { type: 'header', track };
      }

      // Check zones hit
      if (!isCollapsed && mouseY >= currentY + state.trackHeaderHeight + state.frameHeaderHeight && mouseY < currentY + trackHeight) {
        for (const zone of track.zones) {
          const rowY = currentY + state.trackHeaderHeight + state.frameHeaderHeight + 4 + zone.depth * (state.zoneHeight + state.zoneSpacing);
          if (mouseY >= rowY && mouseY <= rowY + state.zoneHeight) {
            const zStartX = nsToX(zone.start_ns);
            const zEndX = nsToX(zone.end_ns);
            const zWidth = Math.max(1, zEndX - zStartX);

            if (mouseX >= zStartX && mouseX <= zStartX + zWidth) {
              return { type: 'zone', zone, track };
            }
          }
        }
      }

      currentY += trackHeight;
    }

    return null;
  }

  function updateSelectionBadge() {
    if (!state.selectionRange) {
      dom.selectionBadge.style.display = 'none';
      return;
    }

    const t1 = Math.min(state.selectionRange.startNs, state.selectionRange.endNs);
    const t2 = Math.max(state.selectionRange.startNs, state.selectionRange.endNs);
    const delta = t2 - t1;

    dom.selectionBadge.style.display = 'flex';
    dom.selectionDurationVal.textContent = formatTime(delta);
    dom.selectionRangeVal.textContent = `[${formatRelativeTime(t1)} - ${formatRelativeTime(t2)}]`;
  }

  // =========================================================================
  // Control Actions & Replay Engine
  // =========================================================================

  function toggleLiveStreaming(forceState) {
    state.isLive = forceState !== undefined ? forceState : !state.isLive;
    if (state.isLive) {
      dom.liveToggleBtn.className = 'btn btn-primary';
      dom.liveToggleText.textContent = 'Pause Live';
      updateStatus('Live', 'live');
    } else {
      dom.liveToggleBtn.className = 'btn btn-secondary';
      dom.liveToggleText.textContent = 'Go Live';
      updateStatus('Paused', 'paused');
    }
  }

  function toggleEngineCapture() {
    if (state.isEngineRecording) {
      sendCommand('stop_capture');
    } else {
      sendCommand('start_capture');
    }
  }

  function fitTimeline() {
    if (state.maxTimeNs > state.minTimeNs) {
      state.viewStartNs = state.minTimeNs;
      state.viewEndNs = state.maxTimeNs;
      state.scrollY = 0;
      render();
    }
  }

  function clearData() {
    state.frames = [];
    state.tracks = [];
    state.markers = [];
    state.metrics = {};
    state.zoneStats.clear();
    state.selectedZone = null;
    state.selectionRange = null;
    state.baseTimeNs = null;
    state.minTimeNs = 0;
    state.maxTimeNs = 0;
    state.viewStartNs = 0;
    state.viewEndNs = 100000000;
    selectZone(null);
    renderStatsTable();
    updateMetricCards();
    updateInfoMetadata();
    render();
  }

  function saveCaptureToFile() {
    const captureData = {
      traceEvents: [],
      displayTimeUnit: 'ns',
    };

    // Metadata
    captureData.traceEvents.push({
      name: 'process_name',
      ph: 'M',
      pid: 1,
      args: { name: 'Tempest Engine' },
    });

    for (const track of state.tracks) {
      captureData.traceEvents.push({
        name: 'thread_name',
        ph: 'M',
        pid: 1,
        tid: track.track_id,
        args: { name: track.name },
      });

      for (const z of track.zones) {
        captureData.traceEvents.push({
          name: z.name,
          cat: z.category || track.type,
          ph: 'X',
          ts: z.start_ns / 1000,
          dur: z.duration_ns / 1000,
          pid: 1,
          tid: track.track_id,
          args: z.metrics ? Object.fromEntries(z.metrics.map(m => [m.name, m.value])) : {},
        });
      }
    }

    const blob = new Blob([JSON.stringify(captureData, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `tempest_capture_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  }

  function handleFileDrop(file) {
    const reader = new FileReader();
    const isBinary = file.name.endsWith('.tprof');

    reader.onload = async (e) => {
      state.isOfflineReplay = true;
      updateStatus(`Replay: ${file.name}`, 'offline');
      clearData();

      if (isBinary) {
        parseBinaryTprof(e.target.result);
      } else {
        try {
          const text = e.target.result;
          const json = JSON.parse(text);
          if (json.traceEvents) {
            ingestChromeTrace(json);
          } else if (json.type === 'frame_data') {
            ingestTelemetryFrame(json);
          }
        } catch (err) {
          alert(`Failed to parse trace file: ${err.message}`);
        }
      }
    };

    if (isBinary) {
      reader.readAsArrayBuffer(file);
    } else {
      reader.readAsText(file);
    }
  }

  async function parseBinaryTprof(arrayBuffer) {
    try {
      const dataView = new DataView(arrayBuffer);
      // Header: 4 bytes 'TPRF'
      const magic = String.fromCharCode(
        dataView.getUint8(0), dataView.getUint8(1), dataView.getUint8(2), dataView.getUint8(3)
      );

      if (magic !== 'TPRF') {
        throw new Error('Invalid .tprof file magic header');
      }

      const compressedBytes = new Uint8Array(arrayBuffer, 28);
      let uncompressedBytes;

      // Decompress via browser native DecompressionStream if available
      if (typeof DecompressionStream !== 'undefined') {
        try {
          const ds = new DecompressionStream('deflate-raw');
          const writer = ds.writable.getWriter();
          writer.write(compressedBytes);
          writer.close();
          const response = new Response(ds.readable);
          uncompressedBytes = new Uint8Array(await response.arrayBuffer());
        } catch (e) {
          // Fallback to deflate with header
          const ds = new DecompressionStream('deflate');
          const writer = ds.writable.getWriter();
          writer.write(compressedBytes);
          writer.close();
          const response = new Response(ds.readable);
          uncompressedBytes = new Uint8Array(await response.arrayBuffer());
        }
      }

      if (uncompressedBytes) {
        readUncompressedBinaryData(uncompressedBytes.buffer);
      }
    } catch (e) {
      console.warn('Binary parse fallback/error:', e);
      alert(`Could not decompress .tprof file directly: ${e.message}`);
    }
  }

  function readUncompressedBinaryData(buffer) {
    const dv = new DataView(buffer);
    let cursor = 0;

    const startNs = Number(dv.getBigUint64(cursor, true)); cursor += 8;
    const endNs = Number(dv.getBigUint64(cursor, true)); cursor += 8;

    // String dictionary
    const strCount = dv.getUint32(cursor, true); cursor += 4;
    const stringTable = [];
    const textDecoder = new TextDecoder('utf-8');

    for (let i = 0; i < strCount; i++) {
      const strLen = dv.getUint32(cursor, true); cursor += 4;
      if (strLen > 0) {
        const strBytes = new Uint8Array(buffer, cursor, strLen);
        stringTable.push(textDecoder.decode(strBytes));
        cursor += strLen;
      } else {
        stringTable.push('');
      }
    }

    const getString = (id) => (id < stringTable.length ? stringTable[id] : '');

    // Tracks
    const trackCount = dv.getUint32(cursor, true); cursor += 4;
    const frameObj = { cpu_tracks: [], gpu_tracks: [], markers: [], metrics: [] };

    for (let i = 0; i < trackCount; i++) {
      const trackId = Number(dv.getBigUint64(cursor, true)); cursor += 8;
      const trackNameId = dv.getUint32(cursor, true); cursor += 4;
      const trackType = dv.getUint8(cursor); cursor += 1;
      const zoneCount = dv.getUint32(cursor, true); cursor += 4;

      const trackObj = {
        track_id: trackId,
        name: getString(trackNameId),
        zones: [],
      };

      for (let j = 0; j < zoneCount; j++) {
        const zStart = Number(dv.getBigUint64(cursor, true)); cursor += 8;
        const zEnd = Number(dv.getBigUint64(cursor, true)); cursor += 8;
        const zDepth = dv.getUint32(cursor, true); cursor += 4;
        const zNameId = dv.getUint32(cursor, true); cursor += 4;
        const zTaskId = Number(dv.getBigUint64(cursor, true)); cursor += 8;
        const metCount = dv.getUint8(cursor); cursor += 1;

        const metrics = [];
        for (let k = 0; k < metCount; k++) {
          const metTs = Number(dv.getBigUint64(cursor, true)); cursor += 8;
          const metNameId = dv.getUint32(cursor, true); cursor += 4;
          const metVal = dv.getFloat64(cursor, true); cursor += 8;
          const metUnit = dv.getUint8(cursor); cursor += 1;
          metrics.push({ name: getString(metNameId), value: metVal, unit: metUnit });
        }

        trackObj.zones.push({
          name: getString(zNameId),
          start_ns: zStart,
          end_ns: zEnd,
          depth: zDepth,
          metrics,
        });
      }

      // Markers
      const markerCount = dv.getUint32(cursor, true); cursor += 4;
      for (let m = 0; m < markerCount; m++) {
        const mTs = Number(dv.getBigUint64(cursor, true)); cursor += 8;
        const mNameId = dv.getUint32(cursor, true); cursor += 4;
        frameObj.markers.push({ name: getString(mNameId), timestamp_ns: mTs });
      }

      if (trackType === 1) {
        frameObj.gpu_tracks.push(trackObj);
      } else {
        frameObj.cpu_tracks.push(trackObj);
      }
    }

    ingestTelemetryFrame(frameObj);
    fitTimeline();
  }

  function ingestChromeTrace(traceJson) {
    const events = traceJson.traceEvents || [];
    const threadMap = new Map();
    const frameObj = { cpu_tracks: [], gpu_tracks: [], markers: [], metrics: [] };

    // Pass 1: Thread names
    for (const ev of events) {
      if (ev.ph === 'M' && ev.name === 'thread_name') {
        threadMap.set(ev.tid, ev.args?.name || `Thread ${ev.tid}`);
      }
    }

    // Pass 2: Zones & Markers
    const tracksById = new Map();

    for (const ev of events) {
      if (ev.ph === 'X') {
        const tid = ev.tid || 1;
        if (!tracksById.has(tid)) {
          tracksById.set(tid, {
            track_id: tid,
            name: threadMap.get(tid) || `Thread ${tid}`,
            zones: [],
          });
        }
        const tr = tracksById.get(tid);
        const startNs = Math.floor((ev.ts || 0) * 1000);
        const durNs = Math.floor((ev.dur || 0) * 1000);

        tr.zones.push({
          name: ev.name,
          start_ns: startNs,
          end_ns: startNs + durNs,
          depth: 0,
          category: ev.cat || 'cpu',
          metrics: ev.args ? Object.entries(ev.args).map(([k, v]) => ({ name: k, value: Number(v) })) : [],
        });
      } else if (ev.ph === 'i') {
        frameObj.markers.push({
          name: ev.name,
          timestamp_ns: Math.floor((ev.ts || 0) * 1000),
        });
      }
    }

    // Compute nested depths
    for (const tr of tracksById.values()) {
      tr.zones.sort((a, b) => a.start_ns - b.start_ns || b.end_ns - a.end_ns);
      const stack = [];
      for (const z of tr.zones) {
        while (stack.length > 0 && stack[stack.length - 1].end_ns <= z.start_ns) {
          stack.pop();
        }
        z.depth = stack.length;
        stack.push(z);
      }

      if (tr.name.toLowerCase().includes('gpu') || tr.name.toLowerCase().includes('queue')) {
        frameObj.gpu_tracks.push(tr);
      } else {
        frameObj.cpu_tracks.push(tr);
      }
    }

    ingestTelemetryFrame(frameObj);
    fitTimeline();
  }

  // =========================================================================
  // UI Tabs, Search, and Event Listeners Setup
  // =========================================================================

  function switchTab(tabId) {
    dom.tabButtons.forEach(btn => {
      btn.classList.toggle('active', btn.dataset.tab === tabId);
    });
    dom.tabPanes.forEach(pane => {
      pane.classList.toggle('active', pane.id === `pane-${tabId}`);
    });
  }

  function initUI() {
    // Dock Tab Buttons
    dom.tabButtons.forEach(btn => {
      btn.addEventListener('click', () => switchTab(btn.dataset.tab));
    });

    // Toggle Dock Collapse
    dom.toggleDockBtn.addEventListener('click', () => {
      dom.dockSection.classList.toggle('collapsed');
      dom.toggleDockBtn.textContent = dom.dockSection.classList.contains('collapsed') ? '▴' : '▾';
    });

    // Live Streaming Toggle
    dom.liveToggleBtn.addEventListener('click', () => toggleLiveStreaming());

    // Backend Engine Capture
    dom.engineCaptureBtn.addEventListener('click', () => toggleEngineCapture());

    // Query Stats
    dom.queryStatsBtn.addEventListener('click', () => sendCommand('query_stats'));

    // Snapshot
    dom.snapshotBtn.addEventListener('click', () => sendCommand('get_snapshot'));

    // Export Trace
    dom.saveTraceBtn.addEventListener('click', () => saveCaptureToFile());

    // Open File Picker
    dom.openFileBtn.addEventListener('click', () => dom.fileInput.click());
    dom.fileInput.addEventListener('change', (e) => {
      if (e.target.files && e.target.files[0]) {
        handleFileDrop(e.target.files[0]);
      }
    });

    // Clear Button
    dom.clearBtn.addEventListener('click', () => clearData());

    // View Controls
    dom.fitViewBtn.addEventListener('click', () => fitTimeline());
    dom.zoomInBtn.addEventListener('click', () => {
      const curDur = state.viewEndNs - state.viewStartNs;
      const center = state.viewStartNs + curDur / 2;
      const newDur = curDur * 0.75;
      state.viewStartNs = center - newDur / 2;
      state.viewEndNs = center + newDur / 2;
      render();
    });
    dom.zoomOutBtn.addEventListener('click', () => {
      const curDur = state.viewEndNs - state.viewStartNs;
      const center = state.viewStartNs + curDur / 2;
      const newDur = curDur * 1.33;
      state.viewStartNs = center - newDur / 2;
      state.viewEndNs = center + newDur / 2;
      render();
    });

    dom.selectTimeUnit.addEventListener('change', (e) => {
      state.timeUnit = e.target.value;
      render();
      renderStatsTable();
    });

    // Shortcuts Modal
    dom.shortcutsBtn.addEventListener('click', () => dom.shortcutsModal.style.display = 'flex');
    dom.modalCloseBtn.addEventListener('click', () => dom.shortcutsModal.style.display = 'none');
    dom.shortcutsModal.addEventListener('click', (e) => {
      if (e.target === dom.shortcutsModal) dom.shortcutsModal.style.display = 'none';
    });

    // Search Input
    dom.searchInput.addEventListener('input', (e) => {
      state.searchQuery = e.target.value;
      dom.searchClearBtn.style.display = state.searchQuery ? 'block' : 'none';
      render();
      renderStatsTable();
    });

    dom.searchClearBtn.addEventListener('click', () => {
      state.searchQuery = '';
      dom.searchInput.value = '';
      dom.searchClearBtn.style.display = 'none';
      dom.searchCount.textContent = '';
      render();
      renderStatsTable();
    });

    // Stats Table Filter
    dom.statsFilter.addEventListener('input', () => renderStatsTable());

    // Stats Table Column Sort Headers
    dom.statsTable.querySelectorAll('th[data-sort]').forEach(th => {
      th.addEventListener('click', () => {
        const col = th.dataset.sort;
        if (state.sortColumn === col) {
          state.sortAscending = !state.sortAscending;
        } else {
          state.sortColumn = col;
          state.sortAscending = false;
        }
        renderStatsTable();
      });
    });

    // Stats CSV Export
    dom.exportStatsCsvBtn.addEventListener('click', () => {
      let csv = 'Zone Name,Category,Count,Total Time (ns),Mean (ns),Min (ns),Max (ns),P50 (ns),P90 (ns),P95 (ns),P99 (ns),StdDev (ns)\n';
      for (const s of state.zoneStats.values()) {
        csv += `"${s.name}","${s.category}",${s.count},${s.totalNs},${s.meanNs.toFixed(2)},${s.minNs.toFixed(2)},${s.maxNs.toFixed(2)},${s.p50Ns.toFixed(2)},${s.p90Ns.toFixed(2)},${s.p95Ns.toFixed(2)},${s.p99Ns.toFixed(2)},${s.stdDevNs.toFixed(2)}\n`;
      }
      const blob = new Blob([csv], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `tempest_stats_${Date.now()}.csv`;
      a.click();
      URL.revokeObjectURL(url);
    });

    // Global Drag and Drop
    window.addEventListener('dragover', (e) => {
      e.preventDefault();
      dom.dropOverlay.classList.add('active');
    });

    window.addEventListener('dragleave', (e) => {
      if (e.relatedTarget === null) {
        dom.dropOverlay.classList.remove('active');
      }
    });

    window.addEventListener('drop', (e) => {
      e.preventDefault();
      dom.dropOverlay.classList.remove('active');
      if (e.dataTransfer.files && e.dataTransfer.files[0]) {
        handleFileDrop(e.dataTransfer.files[0]);
      }
    });

    // Global Keyboard Shortcuts
    window.addEventListener('keydown', (e) => {
      if (e.target.tagName === 'INPUT') return;

      if (e.code === 'Space') {
        e.preventDefault();
        toggleLiveStreaming();
      } else if (e.code === 'KeyF') {
        fitTimeline();
      } else if (e.code === 'Escape') {
        selectZone(null);
        state.selectionRange = null;
        state.searchQuery = '';
        dom.searchInput.value = '';
        dom.searchClearBtn.style.display = 'none';
        dom.selectionBadge.style.display = 'none';
        render();
        renderStatsTable();
      } else if (e.code === 'Home') {
        state.viewStartNs = state.minTimeNs;
        render();
      } else if (e.code === 'End') {
        state.viewEndNs = state.maxTimeNs;
        render();
      }
    });

    // Window Resize
    window.addEventListener('resize', () => render());
  }

  // =========================================================================
  // Initialization Entrypoint
  // =========================================================================

  function init() {
    initUI();
    initInteraction();
    initWebSocket();
    render();
  }

  // Start when DOM is ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

})();
