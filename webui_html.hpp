#pragma once

#include <string>

inline const std::string WEBUI_HTML = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AutoDL 实例 GPU 监控控制台</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --card-hover: #26354a;
      --accent: #38bdf8;
      --accent-hover: #0ea5e9;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --border: #334155;
      --green: #22c55e;
      --red: #ef4444;
      --yellow: #eab308;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
    }
    header {
      background: rgba(30, 41, 59, 0.8);
      backdrop-filter: blur(12px);
      border-bottom: 1px solid var(--border);
      padding: 16px 32px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      position: sticky;
      top: 0;
      z-index: 50;
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .brand-icon {
      width: 32px;
      height: 32px;
      background: linear-gradient(135deg, #38bdf8, #6366f1);
      border-radius: 8px;
      display: flex;
      align-items: center;
      justify-content: center;
      font-weight: bold;
      color: white;
      font-size: 18px;
    }
    .brand h1 { font-size: 18px; font-weight: 600; letter-spacing: -0.5px; }
    .brand p { font-size: 12px; color: var(--text-muted); }
    .top-actions { display: flex; gap: 12px; align-items: center; }
    .btn {
      background: var(--accent);
      color: #0f172a;
      border: none;
      padding: 8px 18px;
      border-radius: 8px;
      font-size: 14px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s ease;
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }
    .btn:hover { background: var(--accent-hover); transform: translateY(-1px); }
    .btn-danger {
      background: transparent;
      color: var(--red);
      border: 1px solid rgba(239, 68, 68, 0.3);
      padding: 6px 12px;
    }
    .btn-danger:hover { background: rgba(239, 68, 68, 0.15); }
    main {
      flex: 1;
      max-width: 1200px;
      width: 100%;
      margin: 0 auto;
      padding: 32px 24px;
    }
    .stats-bar {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 16px;
      margin-bottom: 28px;
    }
    .stat-card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 16px 20px;
    }
    .stat-card .label { font-size: 13px; color: var(--text-muted); margin-bottom: 6px; }
    .stat-card .val { font-size: 24px; font-weight: 700; color: var(--text); }
    .instance-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(360px, 1fr));
      gap: 20px;
    }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 20px;
      display: flex;
      flex-direction: column;
      gap: 16px;
      transition: all 0.2s ease;
      position: relative;
    }
    .card:hover { border-color: rgba(56, 189, 248, 0.4); box-shadow: 0 10px 25px -5px rgba(0,0,0,0.3); }
    .card-header {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 12px;
    }
    .card-title { font-size: 17px; font-weight: 600; color: #fff; line-height: 1.3; }
    .card-uuid {
      font-family: monospace;
      font-size: 12px;
      color: var(--text-muted);
      background: rgba(15, 23, 42, 0.6);
      padding: 4px 8px;
      border-radius: 6px;
      margin-top: 6px;
      word-break: break-all;
      user-select: all;
    }
    .switch-container {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 13px;
      color: var(--text-muted);
    }
    .switch {
      position: relative;
      display: inline-block;
      width: 44px;
      height: 24px;
    }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
      background-color: #475569;
      transition: .3s;
      border-radius: 24px;
    }
    .slider:before {
      position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px;
      background-color: white;
      transition: .3s;
      border-radius: 50%;
    }
    input:checked + .slider { background-color: var(--green); }
    input:checked + .slider:before { transform: translateX(20px); }
    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 6px 14px;
      border-radius: 20px;
      font-size: 13px;
      font-weight: 600;
      width: fit-content;
    }
    .status-badge.available {
      background: rgba(34, 197, 94, 0.15);
      color: #4ade80;
      border: 1px solid rgba(34, 197, 94, 0.3);
    }
    .status-badge.empty {
      background: rgba(239, 68, 68, 0.12);
      color: #f87171;
      border: 1px solid rgba(239, 68, 68, 0.25);
    }
    .status-badge.disabled {
      background: rgba(100, 116, 139, 0.15);
      color: #94a3b8;
      border: 1px solid rgba(100, 116, 139, 0.25);
    }
    .pulse-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: currentColor;
      box-shadow: 0 0 8px currentColor;
    }
    .meta-list {
      display: flex;
      flex-direction: column;
      gap: 6px;
      font-size: 12px;
      color: var(--text-muted);
      background: rgba(15, 23, 42, 0.4);
      padding: 10px 14px;
      border-radius: 10px;
    }
    .meta-item { display: flex; justify-content: space-between; }
    .card-footer {
      display: flex;
      justify-content: flex-end;
      align-items: center;
      padding-top: 10px;
      border-top: 1px solid var(--border);
    }
    .modal-overlay {
      display: none;
      position: fixed;
      inset: 0;
      background: rgba(15, 23, 42, 0.75);
      backdrop-filter: blur(6px);
      z-index: 100;
      align-items: center;
      justify-content: center;
    }
    .modal {
      background: var(--card-bg);
      border: 1px solid var(--border);
      border-radius: 16px;
      max-width: 500px;
      width: 90%;
      padding: 24px;
      box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
    }
    .modal h2 { font-size: 18px; margin-bottom: 16px; }
    .form-group { margin-bottom: 16px; }
    .form-group label { display: block; font-size: 13px; color: var(--text-muted); margin-bottom: 6px; }
    .form-group input {
      width: 100%;
      background: #0f172a;
      border: 1px solid var(--border);
      color: var(--text);
      padding: 10px 12px;
      border-radius: 8px;
      font-size: 14px;
      outline: none;
    }
    .form-group input:focus { border-color: var(--accent); }
    .modal-actions { display: flex; justify-content: flex-end; gap: 12px; margin-top: 20px; }
    .btn-secondary { background: var(--border); color: var(--text); }
    .btn-secondary:hover { background: #475569; }
    .empty-state {
      grid-column: 1 / -1;
      text-align: center;
      padding: 60px 20px;
      background: var(--card-bg);
      border: 1px dashed var(--border);
      border-radius: 16px;
    }
    .empty-state p { color: var(--text-muted); margin-bottom: 16px; }
  </style>
</head>
<body>
  <header>
    <div class="brand">
      <div class="brand-icon">G</div>
      <div>
        <h1>AutoDL 实例 GPU 监控面板</h1>
        <p>开机抢卡守护器 (端口 5178)</p>
      </div>
    </div>
    <div class="top-actions">
      <button class="btn" onclick="openModal()">+ 添加监控实例</button>
    </div>
  </header>

  <main>
    <div class="stats-bar">
      <div class="stat-card">
        <div class="label">监控实例总数</div>
        <div class="val" id="stat-total">0</div>
      </div>
      <div class="stat-card">
        <div class="label">已启用监控</div>
        <div class="val" id="stat-enabled" style="color: #38bdf8;">0</div>
      </div>
      <div class="stat-card">
        <div class="label">当前有空闲卡</div>
        <div class="val" id="stat-available" style="color: #22c55e;">0</div>
      </div>
    </div>

    <div class="instance-grid" id="instance-list">
      <!-- 动态加载卡片 -->
    </div>
  </main>

  <div class="modal-overlay" id="add-modal">
    <div class="modal">
      <h2>添加监控实例</h2>
      <div class="form-group">
        <label>实例 UUID (在 AutoDL 控制台复制)</label>
        <input type="text" id="input-uuid" placeholder="例如: 3b14a2f8-xxxx-xxxx-xxxx-xxxxxxxxxxxx" />
      </div>
      <div class="form-group">
        <label>备注名称</label>
        <input type="text" id="input-alias" placeholder="例如: 毕业论文-RTX4090训练机" />
      </div>
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick="closeModal()">取消</button>
        <button class="btn" onclick="submitAdd()">立即添加</button>
      </div>
    </div>
  </div>

  <script>
    async function loadInstances() {
      try {
        const res = await fetch('/api/instances');
        const data = await res.json();
        render(data);
      } catch (err) {
        console.error('加载实例失败:', err);
      }
    }

    function render(list) {
      const container = document.getElementById('instance-list');
      const total = list.length;
      const enabled = list.filter(i => i.enabled).length;
      const available = list.filter(i => i.enabled && i.current_idle_num > 0).length;

      document.getElementById('stat-total').innerText = total;
      document.getElementById('stat-enabled').innerText = enabled;
      document.getElementById('stat-available').innerText = available;

      if (list.length === 0) {
        container.innerHTML = `
          <div class="empty-state">
            <p>当前暂未添加任何监控实例</p>
            <button class="btn" onclick="openModal()">立即添加第一台实例</button>
          </div>
        `;
        return;
      }

      container.innerHTML = list.map(item => {
        let statusBadge = '';
        if (!item.enabled) {
          statusBadge = '<span class="status-badge disabled"><span class="pulse-dot"></span>监控已停用</span>';
        } else if (item.current_idle_num > 0) {
          statusBadge = `<span class="status-badge available"><span class="pulse-dot"></span>🎉 有空闲卡: ${item.current_idle_num} 台</span>`;
        } else {
          statusBadge = '<span class="status-badge empty"><span class="pulse-dot"></span>无空闲卡 (等待释放)</span>';
        }

        return `
          <div class="card">
            <div class="card-header">
              <div>
                <div class="card-title">${escapeHtml(item.alias || '未命名实例')}</div>
                <div class="card-uuid">${item.uuid}</div>
              </div>
              <div class="switch-container">
                <label class="switch">
                  <input type="checkbox" ${item.enabled ? 'checked' : ''} onchange="toggleMonitor('${item.uuid}', this.checked)">
                  <span class="slider"></span>
                </label>
              </div>
            </div>

            <div>${statusBadge}</div>

            <div class="meta-list">
              <div class="meta-item"><span>GPU 型号</span><span>${item.gpu_name || '检测中...'}</span></div>
              <div class="meta-item"><span>物理机ID</span><span>${item.machine_id || '检测中...'}</span></div>
              <div class="meta-item"><span>最后检测</span><span>${item.last_check_time || '-'}</span></div>
            </div>

            <div class="card-footer">
              <button class="btn btn-danger" onclick="deleteInstance('${item.uuid}')">删除监控</button>
            </div>
          </div>
        `;
      }).join('');
    }

    async function toggleMonitor(uuid, enabled) {
      await fetch('/api/instances/toggle', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ uuid, enabled })
      });
      loadInstances();
    }

    async function deleteInstance(uuid) {
      if (!confirm('确定停止并删除对此实例的监控吗？')) return;
      await fetch('/api/instances/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ uuid })
      });
      loadInstances();
    }

    function openModal() {
      document.getElementById('input-uuid').value = '';
      document.getElementById('input-alias').value = '';
      document.getElementById('add-modal').style.display = 'flex';
    }

    function closeModal() {
      document.getElementById('add-modal').style.display = 'none';
    }

    async function submitAdd() {
      const uuid = document.getElementById('input-uuid').value.trim();
      const alias = document.getElementById('input-alias').value.trim();
      if (!uuid) {
        alert('实例 UUID 不能为空！');
        return;
      }
      await fetch('/api/instances', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ uuid, alias })
      });
      closeModal();
      loadInstances();
    }

    function escapeHtml(text) {
      const div = document.createElement('div');
      div.innerText = text;
      return div.innerHTML;
    }

    // 初始化并每 3 秒自动拉取最新状态
    loadInstances();
    setInterval(loadInstances, 3000);
  </script>
</body>
</html>
)rawhtml";
