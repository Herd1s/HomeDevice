from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from app.core.ws_manager import ws_manager

router = APIRouter(tags=["ws"])


@router.websocket("/ws/alarm")
async def ws_alarm(ws: WebSocket) -> None:
    await ws_manager.connect(ws)
    try:
        while True:
            # Keep-alive: read client messages to keep connection active.
            await ws.receive_text()
    except WebSocketDisconnect:
        ws_manager.disconnect(ws)
    except Exception:
        ws_manager.disconnect(ws)

