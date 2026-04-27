# HomeDevice Server (FastAPI)

## 1. Run

```powershell
cd Server
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
```

Default login:

- username: `admin`
- password: `123456`

## 2. API

- `POST /api/device/upload`
- `GET /api/device/cmd?device_id=HD-001`
- `POST /api/app/login`
- `GET /api/app/status?device_id=HD-001`
- `POST /api/app/control`
- `GET /api/app/history`
- `WS /ws/alarm`

## 3. Device Upload Example

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

## 4. App Control Example

```json
{
  "device_id": "HD-001",
  "command_type": "SET_MODE",
  "payload": "ARM"
}
```

