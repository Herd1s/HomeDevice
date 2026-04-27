from fastapi import APIRouter

from app.core.db import db
from app.core.ws_manager import ws_manager
from app.models.schemas import DeviceUploadRequest

router = APIRouter(prefix="/api/device", tags=["device"])


@router.post("/upload")
async def upload_data(payload: DeviceUploadRequest) -> dict:
    db.upsert_device_status(
        payload.device_id,
        {
            "temperature": payload.temperature,
            "humidity": payload.humidity,
            "smoke": payload.smoke,
            "flame": payload.flame,
            "intrusion": payload.intrusion,
            "mode": payload.mode,
            "relay": payload.relay,
            "buzzer": payload.buzzer,
            "network": payload.network,
            "air780e": payload.air780e,
        },
    )

    if payload.ack_command_id is not None:
        db.mark_command_done(payload.ack_command_id)

    alarm = None
    if payload.alarm_type and payload.alarm_level and payload.alarm_message:
        alarm = db.add_alarm(
            device_id=payload.device_id,
            alarm_type=payload.alarm_type,
            level=payload.alarm_level,
            message=payload.alarm_message,
        )
        await ws_manager.broadcast_json({"event": "alarm", "data": alarm})

    status = db.get_device_status(payload.device_id)
    if status is not None:
        await ws_manager.broadcast_json({"event": "status", "data": status})

    return {"ok": True, "alarm_saved": alarm is not None}


@router.get("/cmd")
def get_command(device_id: str = "HD-001") -> dict:
    cmd = db.pop_next_pending_command(device_id)
    if cmd is None:
        return {"ok": True, "has_command": False}
    return {"ok": True, "has_command": True, "command": cmd}

