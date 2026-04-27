from pydantic import BaseModel, Field


class LoginRequest(BaseModel):
    username: str
    password: str


class LoginResponse(BaseModel):
    ok: bool
    token: str | None = None


class DeviceUploadRequest(BaseModel):
    device_id: str = Field(default="HD-001")
    temperature: float
    humidity: float
    smoke: int
    flame: int
    intrusion: int
    mode: str
    relay: int
    buzzer: int
    network: int = 1
    air780e: int = 1
    alarm_type: str | None = None
    alarm_level: str | None = None
    alarm_message: str | None = None
    ack_command_id: int | None = None


class ControlRequest(BaseModel):
    device_id: str = Field(default="HD-001")
    command_type: str
    payload: str

