from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.core.db import db
from app.routers.app_api import router as app_router
from app.routers.device_api import router as device_router
from app.routers.ws_api import router as ws_router

app = FastAPI(title="HomeDevice Server", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.on_event("startup")
def startup_event() -> None:
    db.init()


@app.get("/health")
def health() -> dict:
    return {"ok": True}


app.include_router(device_router)
app.include_router(app_router)
app.include_router(ws_router)

