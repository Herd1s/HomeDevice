from fastapi import APIRouter, HTTPException

from app.core.db import db
from app.models.schemas import ControlRequest, LoginRequest, LoginResponse

router = APIRouter(prefix="/api/app", tags=["app"])


@router.post("/login", response_model=LoginResponse)
def login(payload: LoginRequest) -> LoginResponse:
    ok = db.verify_user(payload.username, payload.password)
    token = f"demo-token-{payload.username}" if ok else None
    return LoginResponse(ok=ok, token=token)


@router.get("/status")
def get_status(device_id: str = "HD-001") -> dict:
    status = db.get_device_status(device_id)
    if status is None:
        raise HTTPException(status_code=404, detail="device not found")
    return {"ok": True, "data": status}


@router.post("/control")
def control(payload: ControlRequest) -> dict:
    cmd_id = db.add_command(payload.device_id, payload.command_type, payload.payload)
    return {"ok": True, "command_id": cmd_id}


@router.get("/history")
def history(limit: int = 50, offset: int = 0) -> dict:
    limit = min(max(limit, 1), 200)
    offset = max(offset, 0)
    rows = db.list_alarms(limit=limit, offset=offset)
    return {"ok": True, "data": rows}

