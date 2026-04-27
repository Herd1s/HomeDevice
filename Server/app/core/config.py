from pydantic import BaseModel


class Settings(BaseModel):
    db_path: str = "server.db"
    default_device_id: str = "HD-001"
    default_username: str = "admin"
    default_password: str = "123456"


settings = Settings()

