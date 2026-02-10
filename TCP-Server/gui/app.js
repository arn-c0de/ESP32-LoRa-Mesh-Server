const { useEffect, useMemo, useRef, useState } = React;

const DEFAULT_BASE = localStorage.getItem("hs_api_base") || "http://localhost:8000";
const DEFAULT_KEY = localStorage.getItem("hs_api_key") || "";

const makeWsUrl = (base, apiKey) => {
  if (!base) return "";
  const url = new URL(base);
  url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
  url.pathname = "/ws";
  if (apiKey) url.searchParams.set("api_key", apiKey);
  return url.toString();
};

  const fmtTime = (iso) => {
    if (!iso) return "--";
    const d = new Date(iso);
    return d.toLocaleString();
  };

  const fmtDateLabel = (iso) => {
    if (!iso) return "--";
    const d = new Date(iso);
    return d.toLocaleDateString(undefined, { year: "numeric", month: "short", day: "numeric" });
  };

const clamp = (v, min, max) => Math.min(max, Math.max(min, v));

function App() {
  const [theme, setTheme] = useState(localStorage.getItem("hs_theme") || "light");
  const [accessOpen, setAccessOpen] = useState(false);
  const [apiBase, setApiBase] = useState(DEFAULT_BASE);
  const [apiKey, setApiKey] = useState(DEFAULT_KEY);
  const [status, setStatus] = useState("disconnected");
  const [wsStatus, setWsStatus] = useState("idle");
  const [stats, setStats] = useState(null);
  const [nodes, setNodes] = useState([]);
  const [feed, setFeed] = useState([]);
  const [history, setHistory] = useState([]);
  const [historyOffset, setHistoryOffset] = useState(0);
  const [filterFrom, setFilterFrom] = useState("");
  const [filterTo, setFilterTo] = useState("");
  const [filterSince, setFilterSince] = useState("");
  const [filterText, setFilterText] = useState("");
  const [localNodeId, setLocalNodeId] = useState(localStorage.getItem("hs_local_node") || "");
  const [cmdTarget, setCmdTarget] = useState("0");
  const [cmdText, setCmdText] = useState("");
  const [cmdLog, setCmdLog] = useState([]);
  const [toast, setToast] = useState("");
  const wsRef = useRef(null);
  const reconnectRef = useRef({ attempts: 0, timer: null });
  const feedRef = useRef(null);

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    localStorage.setItem("hs_theme", theme);
  }, [theme]);

  const headers = useMemo(() => {
    const h = { "Content-Type": "application/json" };
    if (apiKey) h["X-API-Key"] = apiKey;
    return h;
  }, [apiKey]);

  const showToast = (msg) => {
    setToast(msg);
    setTimeout(() => setToast(""), 3500);
  };

  const nodeHue = (id) => {
    const num = Number(id) || 0;
    return (num * 47) % 360;
  };

  const apiFetch = async (path) => {
    if (!apiBase) throw new Error("API base URL missing");
    const url = new URL(path, apiBase);
    const res = await fetch(url.toString(), { headers });
    if (!res.ok) {
      const text = await res.text();
      throw new Error(text || `HTTP ${res.status}`);
    }
    return res.json();
  };

  const connectWs = () => {
    const wsUrl = makeWsUrl(apiBase, apiKey);
    if (!wsUrl) return;
    if (wsRef.current) wsRef.current.close();

    setWsStatus("connecting");
    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      reconnectRef.current.attempts = 0;
      setWsStatus("open");
    };

    ws.onmessage = (evt) => {
      try {
        const msg = JSON.parse(evt.data);
        if (msg.type === "message") {
          setFeed((prev) => [msg, ...prev].slice(0, 200));
        }
      } catch (e) {
        showToast("WS parse error");
      }
    };

    ws.onclose = () => {
      setWsStatus("closed");
      scheduleReconnect();
    };

    ws.onerror = () => {
      setWsStatus("error");
      ws.close();
    };
  };

  const scheduleReconnect = () => {
    if (reconnectRef.current.timer) return;
    const attempts = reconnectRef.current.attempts + 1;
    reconnectRef.current.attempts = attempts;
    const delay = clamp(1000 * Math.pow(2, attempts), 1000, 15000);
    reconnectRef.current.timer = setTimeout(() => {
      reconnectRef.current.timer = null;
      connectWs();
    }, delay);
  };

  const loadStats = async () => {
    const data = await apiFetch("/api/v1/stats");
    setStats(data);
  };

  const loadNodes = async () => {
    const data = await apiFetch("/api/v1/nodes");
    setNodes(data);
  };

  const loadHistory = async (reset = false) => {
    const limit = 50;
    const offset = reset ? 0 : historyOffset;
    const params = new URLSearchParams({ limit, offset });
    if (filterFrom) params.set("from_id", filterFrom);
    if (filterTo) params.set("to_id", filterTo);
    if (filterSince) params.set("since", new Date(filterSince).toISOString());

    const data = await apiFetch(`/api/v1/messages?${params.toString()}`);
    const filtered = filterText
      ? data.filter((m) => (m.data || "").toLowerCase().includes(filterText.toLowerCase()))
      : data;

    setHistory((prev) => (reset ? filtered : [...prev, ...filtered]));
    setHistoryOffset(offset + limit);
  };

  const handleConnect = async () => {
    try {
      localStorage.setItem("hs_api_base", apiBase);
      localStorage.setItem("hs_api_key", apiKey);
      setStatus("connecting");
      await loadStats();
      await loadNodes();
      await loadHistory(true);
      connectWs();
      setStatus("connected");
      showToast("Connected");
    } catch (e) {
      setStatus("error");
      showToast(`Connect failed: ${e.message}`);
    }
  };

  const handleRefresh = async () => {
    try {
      await loadStats();
      await loadNodes();
      showToast("Refreshed");
    } catch (e) {
      showToast(`Refresh failed: ${e.message}`);
    }
  };

  const handleSendCommand = async () => {
    if (!cmdText) {
      showToast("Command is empty");
      return;
    }
    try {
      const payload = { target_id: Number(cmdTarget || 0), command: cmdText };
      const res = await fetch(new URL("/api/v1/command", apiBase), {
        method: "POST",
        headers,
        body: JSON.stringify(payload),
      });
      if (!res.ok) {
        const text = await res.text();
        throw new Error(text || `HTTP ${res.status}`);
      }
      setCmdLog((prev) => [{ ts: new Date().toISOString(), cmd: cmdText, target: cmdTarget }, ...prev]);
      setCmdText("");
      showToast("Command sent");
    } catch (e) {
      showToast(`Command failed: ${e.message}`);
    }
  };

  useEffect(() => {
    if (feedRef.current) {
      feedRef.current.scrollTop = 0;
    }
  }, [feed]);

  useEffect(() => {
    if (apiBase) {
      handleConnect();
    }
  }, []);

  return (
    <div className="app">
      <div className="bg" aria-hidden="true"></div>
      <header className="topbar">
        <div className="brand">
          <div className="logo">HS</div>
          <div>
            <div className="title">HomeServer Console</div>
            <div className="subtitle">LoRa Mesh · Live · History · Control</div>
          </div>
        </div>
        <div className="actions">
          <button className="btn ghost" onClick={() => setTheme(theme === "dark" ? "light" : "dark")}>
            Toggle Theme
          </button>
          <button className="btn" onClick={handleRefresh}>Refresh</button>
        </div>
      </header>

      <main className="grid">
        <section className={`card auth ${accessOpen ? "open" : "closed"}`}>
          <div className="card-header">
            <h2>Access</h2>
            <div className="row">
              <div className={`pill ${status}`}>{status}</div>
              <button className="icon-btn" onClick={() => setAccessOpen(!accessOpen)} aria-label="Toggle access settings">
                <svg viewBox="0 0 24 24" width="18" height="18" aria-hidden="true">
                  <path d="M12 8.6a3.4 3.4 0 1 0 0 6.8 3.4 3.4 0 0 0 0-6.8Zm9 3.4a7.4 7.4 0 0 0-.1-1.1l2-1.6-2-3.5-2.4.8a7.7 7.7 0 0 0-2-1.2L14.9 2h-4l-.7 2.4a7.7 7.7 0 0 0-2 1.2l-2.4-.8-2 3.5 2 1.6a7.4 7.4 0 0 0 0 2.2l-2 1.6 2 3.5 2.4-.8a7.7 7.7 0 0 0 2 1.2l.7 2.4h4l.7-2.4a7.7 7.7 0 0 0 2-1.2l2.4.8 2-3.5-2-1.6c.1-.4.1-.7.1-1.1Z" fill="currentColor"/>
                </svg>
              </button>
            </div>
          </div>
          <div className="access-summary">
            <div className="meta">Base: {apiBase || "not set"}</div>
            <div className="meta">Local Node: {localNodeId || "not set"}</div>
          </div>
          {accessOpen && (
            <>
              <div className="field">
                <label>API Base URL</label>
                <input value={apiBase} onChange={(e) => setApiBase(e.target.value)} placeholder="http://localhost:8000" />
              </div>
              <div className="field">
                <label>API Key</label>
                <input type="password" value={apiKey} onChange={(e) => setApiKey(e.target.value)} placeholder="X-API-Key" />
              </div>
              <div className="field">
                <label>Local Node ID (for chat alignment)</label>
                <input
                  type="number"
                  min="0"
                  value={localNodeId}
                  onChange={(e) => {
                    setLocalNodeId(e.target.value);
                    localStorage.setItem("hs_local_node", e.target.value);
                  }}
                  placeholder="e.g. 1"
                />
              </div>
              <div className="row">
                <button className="btn primary" onClick={handleConnect}>Connect</button>
                <button className="btn ghost" onClick={() => { setApiKey(""); localStorage.removeItem("hs_api_key"); }}>Clear Key</button>
              </div>
              <p className="hint">API key is stored locally in this browser.</p>
            </>
          )}
        </section>

        <section className="card stats">
          <div className="card-header">
            <h2>Network Snapshot</h2>
            <div className="meta">WS: {wsStatus}</div>
          </div>
          <div className="stats-grid">
            <div className="stat">
              <div className="stat-label">Total Messages</div>
              <div className="stat-value">{stats?.total_messages ?? "--"}</div>
            </div>
            <div className="stat">
              <div className="stat-label">Nodes</div>
              <div className="stat-value">{stats?.total_nodes ?? "--"}</div>
            </div>
            <div className="stat">
              <div className="stat-label">Online</div>
              <div className="stat-value">{stats?.online_nodes ?? "--"}</div>
            </div>
            <div className="stat">
              <div className="stat-label">Last Hour</div>
              <div className="stat-value">{stats?.messages_last_hour ?? "--"}</div>
            </div>
          </div>
          <div className="meta">Uptime: {stats ? `${Math.round(stats.uptime_seconds)}s` : "--"}</div>
        </section>

        <section className="card feed">
          <div className="card-header">
            <h2>Live Feed</h2>
            <div className="meta">Chat-style · Newest first</div>
          </div>
          <div className="feed-list chat" ref={feedRef}>
            {feed.length === 0 && <div className="empty">No live messages yet.</div>}
            {feed.map((m, idx) => {
              const bubbleType = m.to_id === 0 ? "broadcast" : "direct";
              const isLocal = localNodeId && Number(localNodeId) === m.from_id;
              const align = bubbleType === "broadcast" ? "center" : (isLocal ? "out" : "in");
              const hue = nodeHue(m.from_id);
              const prev = feed[idx + 1];
              const showDate =
                !prev || fmtDateLabel(prev.timestamp) !== fmtDateLabel(m.timestamp);
              const text = (m.decrypted_data || m.data || "").trim();
              const isShort = text.length > 0 && text.length <= 24;
              return (
                <React.Fragment key={`feed-${m.id}`}>
                  {showDate && (
                    <div className="date-sep">
                      <span>{fmtDateLabel(m.timestamp)}</span>
                    </div>
                  )}
                  <div className={`chat-row ${align}`}>
                    <div
                      className={`chat-bubble ${bubbleType} ${isShort ? "short" : ""} ${m.is_encrypted ? "encrypted" : ""}`}
                      style={{ "--node-hue": hue }}
                    >
                    <div className="chat-head">
                      <span className="avatar" aria-hidden="true">{m.from_id}</span>
                      <span className="chip">Node {m.from_id}</span>
                      {m.to_id !== 0 && <span className="chip ghost">To {m.to_id}</span>}
                      <span className="chip mono">H{m.hop_count}</span>
                      {m.is_encrypted && <span className="chip warn">Encrypted</span>}
                    </div>
                    <div className="chat-text">{m.decrypted_data || m.data}</div>
                    <div className="chat-meta">{fmtTime(m.timestamp)}</div>
                    </div>
                  </div>
                </React.Fragment>
              );
            })}
          </div>
        </section>

        <section className="card history">
          <div className="card-header">
            <h2>History & Search</h2>
            <div className="meta">Server-side pagination</div>
          </div>
          <div className="filters">
            <div className="field">
              <label>From Node</label>
              <input type="number" min="0" value={filterFrom} onChange={(e) => setFilterFrom(e.target.value)} />
            </div>
            <div className="field">
              <label>To Node</label>
              <input type="number" min="0" value={filterTo} onChange={(e) => setFilterTo(e.target.value)} />
            </div>
            <div className="field">
              <label>Since (UTC)</label>
              <input type="datetime-local" value={filterSince} onChange={(e) => setFilterSince(e.target.value)} />
            </div>
            <div className="field">
              <label>Text Contains</label>
              <input type="text" value={filterText} onChange={(e) => setFilterText(e.target.value)} />
            </div>
          </div>
          <div className="row">
            <button className="btn" onClick={() => { setHistoryOffset(0); loadHistory(true); }}>Search</button>
            <button className="btn ghost" onClick={() => loadHistory(false)}>Load More</button>
          </div>
          <div className="history-list">
            {history.length === 0 && <div className="empty">No messages found.</div>}
            {history.map((m) => (
              <div className="history-item" key={`hist-${m.id}`}>
                <div className="history-left">
                  <div className="badge">From {m.from_id}</div>
                  <div className="badge ghost">To {m.to_id}</div>
                  <div className="badge mono">H{m.hop_count}</div>
                </div>
                <div className="history-body">
                  <div className="history-text">{m.decrypted_data || m.data}</div>
                  <div className="history-meta">{fmtTime(m.timestamp)}</div>
                </div>
              </div>
            ))}
          </div>
        </section>

        <section className="card nodes">
          <div className="card-header">
            <h2>Nodes</h2>
            <div className="meta">{nodes.length} nodes</div>
          </div>
          <div className="node-list">
            {nodes.length === 0 && <div className="empty">No nodes yet.</div>}
            {nodes.map((n) => (
              <div className={`node ${n.is_online ? "online" : "offline"}`} key={`node-${n.id}`}>
                <div className="node-head">
                  <span className="badge">Node {n.id}</span>
                  <span className={`pill ${n.is_online ? "connected" : "disconnected"}`}>{n.is_online ? "online" : "offline"}</span>
                </div>
                <div className="node-body">
                  <div className="node-name">{n.device_name || "(unnamed)"}</div>
                  <div className="node-meta">Last seen: {fmtTime(n.last_seen)}</div>
                  <div className="node-meta">IP: {n.ip_address || "-"}</div>
                </div>
              </div>
            ))}
          </div>
        </section>

        <section className="card command">
          <div className="card-header">
            <h2>Remote Command</h2>
            <div className="meta">Requires backend command endpoint</div>
          </div>
          <div className="field">
            <label>Target Node ID (0 = broadcast)</label>
            <input type="number" min="0" value={cmdTarget} onChange={(e) => setCmdTarget(e.target.value)} />
          </div>
          <div className="field">
            <label>Command</label>
            <input type="text" value={cmdText} onChange={(e) => setCmdText(e.target.value)} placeholder="/PING or /TX:Hello" />
          </div>
          <div className="row">
            <button className="btn primary" onClick={handleSendCommand}>Send Command</button>
          </div>
          <div className="command-log">
            {cmdLog.length === 0 && <div className="empty">No commands sent yet.</div>}
            {cmdLog.map((c, i) => (
              <div className="command-item" key={`cmd-${i}`}>
                <span className="badge mono">{fmtTime(c.ts)}</span>
                <span className="badge ghost">Target {c.target}</span>
                <span className="command-text">{c.cmd}</span>
              </div>
            ))}
          </div>
        </section>
      </main>

      <div className={`toast ${toast ? "show" : ""}`}>{toast}</div>
    </div>
  );
}

const root = ReactDOM.createRoot(document.getElementById("root"));
root.render(<App />);
