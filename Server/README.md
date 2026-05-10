# HomeDevice Server (FastAPI)

## 1. Run

```powershell
cd Server
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

## 2. Deploy to the new server

Target server:

- IP: `42.192.113.88`
- Public API base URL: `http://42.192.113.88:8000`

On your local machine, upload the backend code:

```powershell
ssh root@42.192.113.88 "mkdir -p /opt/HomeDevice"
scp -r Server root@42.192.113.88:/opt/HomeDevice/
```

On the server:

```bash
ssh root@42.192.113.88
cd /opt/HomeDevice/Server
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Start it manually for a first test:

```bash
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

Open this URL to verify:

```text
http://42.192.113.88:8000/health
```

For long-running deployment, create `/etc/systemd/system/home-device.service`:

```ini
[Unit]
Description=HomeDevice FastAPI Server
After=network.target

[Service]
WorkingDirectory=/opt/HomeDevice/Server
ExecStart=/opt/HomeDevice/Server/.venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

Enable and start the service:

```bash
systemctl daemon-reload
systemctl enable --now home-device
systemctl status home-device
```

Make sure the cloud security group and server firewall allow TCP `8000`.

Default login:

- username: `admin`
- password: `123456`

## 3. API

- `POST /api/device/upload`
- `GET /api/device/cmd?device_id=HD-001`
- `POST /api/app/login`
- `GET /api/app/status?device_id=HD-001`
- `POST /api/app/control`
- `GET /api/app/history`
- `WS /ws/alarm`

## 4. Device Upload Example

```json
{
  "device_id": "HD-001",
  "temperature": 25.1,
  "humidity": 55.3,
  "smoke": 420,
  "flame": 0,
  "intrusion": 0,
  "mode": "ARM",
  "relay": 0,
  "buzzer": 0,
  "network": 1,
  "air780e": 1
}
```

## 5. App Control Example

```json
{
  "device_id": "HD-001",
  "command_type": "SET_MODE",
  "payload": "ARM"
}
```
