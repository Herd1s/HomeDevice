import sqlite3
import threading
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .config import settings
from .security import sha256_text


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


class DB:
    def __init__(self, db_path: str) -> None:
        self.db_path = Path(db_path)
        self.lock = threading.Lock()

    def _conn(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path.as_posix(), check_same_thread=False)
        conn.row_factory = sqlite3.Row
        return conn

    def init(self) -> None:
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.executescript(
                """
                CREATE TABLE IF NOT EXISTS users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    username TEXT UNIQUE NOT NULL,
                    password_hash TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS devices (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    device_id TEXT UNIQUE NOT NULL,
                    updated_at TEXT NOT NULL,
                    temperature REAL DEFAULT 0,
                    humidity REAL DEFAULT 0,
                    smoke INTEGER DEFAULT 0,
                    flame INTEGER DEFAULT 0,
                    intrusion INTEGER DEFAULT 0,
                    mode TEXT DEFAULT 'DISARM',
                    relay INTEGER DEFAULT 0,
                    buzzer INTEGER DEFAULT 0,
                    network INTEGER DEFAULT 0,
                    air780e INTEGER DEFAULT 0
                );

                CREATE TABLE IF NOT EXISTS alarms (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    device_id TEXT NOT NULL,
                    alarm_type TEXT NOT NULL,
                    level TEXT NOT NULL,
                    message TEXT NOT NULL,
                    created_at TEXT NOT NULL
                );

                CREATE TABLE IF NOT EXISTS commands (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    device_id TEXT NOT NULL,
                    command_type TEXT NOT NULL,
                    payload TEXT NOT NULL,
                    status TEXT NOT NULL DEFAULT 'PENDING',
                    created_at TEXT NOT NULL,
                    executed_at TEXT
                );
                """
            )
            conn.commit()
            conn.close()

        self._seed_default_data()

    def _seed_default_data(self) -> None:
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute("SELECT id FROM users WHERE username = ?", (settings.default_username,))
            if cur.fetchone() is None:
                cur.execute(
                    "INSERT INTO users(username, password_hash) VALUES(?, ?)",
                    (settings.default_username, sha256_text(settings.default_password)),
                )

            cur.execute("SELECT id FROM devices WHERE device_id = ?", (settings.default_device_id,))
            if cur.fetchone() is None:
                cur.execute(
                    """
                    INSERT INTO devices(
                        device_id, updated_at, temperature, humidity, smoke,
                        flame, intrusion, mode, relay, buzzer, network, air780e
                    ) VALUES(?, ?, 0, 0, 0, 0, 0, 'DISARM', 0, 0, 0, 0)
                    """,
                    (settings.default_device_id, utc_now_iso()),
                )

            conn.commit()
            conn.close()

    def verify_user(self, username: str, password: str) -> bool:
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute("SELECT password_hash FROM users WHERE username = ?", (username,))
            row = cur.fetchone()
            conn.close()
        if row is None:
            return False
        return row["password_hash"] == sha256_text(password)

    def upsert_device_status(self, device_id: str, payload: dict[str, Any]) -> None:
        now = utc_now_iso()
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute("SELECT id FROM devices WHERE device_id = ?", (device_id,))
            row = cur.fetchone()
            if row is None:
                cur.execute(
                    """
                    INSERT INTO devices(
                        device_id, updated_at, temperature, humidity, smoke, flame,
                        intrusion, mode, relay, buzzer, network, air780e
                    ) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        device_id,
                        now,
                        payload.get("temperature", 0),
                        payload.get("humidity", 0),
                        payload.get("smoke", 0),
                        payload.get("flame", 0),
                        payload.get("intrusion", 0),
                        payload.get("mode", "DISARM"),
                        payload.get("relay", 0),
                        payload.get("buzzer", 0),
                        payload.get("network", 1),
                        payload.get("air780e", 1),
                    ),
                )
            else:
                cur.execute(
                    """
                    UPDATE devices SET
                        updated_at = ?,
                        temperature = ?,
                        humidity = ?,
                        smoke = ?,
                        flame = ?,
                        intrusion = ?,
                        mode = ?,
                        relay = ?,
                        buzzer = ?,
                        network = ?,
                        air780e = ?
                    WHERE device_id = ?
                    """,
                    (
                        now,
                        payload.get("temperature", 0),
                        payload.get("humidity", 0),
                        payload.get("smoke", 0),
                        payload.get("flame", 0),
                        payload.get("intrusion", 0),
                        payload.get("mode", "DISARM"),
                        payload.get("relay", 0),
                        payload.get("buzzer", 0),
                        payload.get("network", 1),
                        payload.get("air780e", 1),
                        device_id,
                    ),
                )
            conn.commit()
            conn.close()

    def add_alarm(self, device_id: str, alarm_type: str, level: str, message: str) -> dict[str, Any]:
        now = utc_now_iso()
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute(
                """
                INSERT INTO alarms(device_id, alarm_type, level, message, created_at)
                VALUES(?, ?, ?, ?, ?)
                """,
                (device_id, alarm_type, level, message, now),
            )
            alarm_id = cur.lastrowid
            conn.commit()
            conn.close()
        return {
            "id": alarm_id,
            "device_id": device_id,
            "alarm_type": alarm_type,
            "level": level,
            "message": message,
            "created_at": now,
        }

    def list_alarms(self, limit: int, offset: int) -> list[dict[str, Any]]:
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute(
                """
                SELECT id, device_id, alarm_type, level, message, created_at
                FROM alarms
                ORDER BY id DESC
                LIMIT ? OFFSET ?
                """,
                (limit, offset),
            )
            rows = cur.fetchall()
            conn.close()
        return [dict(row) for row in rows]

    def add_command(self, device_id: str, command_type: str, payload: str) -> int:
        now = utc_now_iso()
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute(
                """
                INSERT INTO commands(device_id, command_type, payload, status, created_at)
                VALUES(?, ?, ?, 'PENDING', ?)
                """,
                (device_id, command_type, payload, now),
            )
            cmd_id = int(cur.lastrowid)
            conn.commit()
            conn.close()
        return cmd_id

    def pop_next_pending_command(self, device_id: str) -> dict[str, Any] | None:
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute(
                """
                SELECT id, device_id, command_type, payload, status, created_at
                FROM commands
                WHERE device_id = ? AND status = 'PENDING'
                ORDER BY id ASC
                LIMIT 1
                """,
                (device_id,),
            )
            row = cur.fetchone()
            if row is None:
                conn.close()
                return None

            cur.execute(
                "UPDATE commands SET status = 'SENT', executed_at = ? WHERE id = ?",
                (utc_now_iso(), row["id"]),
            )
            conn.commit()
            conn.close()
        return dict(row)

    def mark_command_done(self, command_id: int) -> None:
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute(
                "UPDATE commands SET status = 'DONE', executed_at = ? WHERE id = ?",
                (utc_now_iso(), command_id),
            )
            conn.commit()
            conn.close()

    def get_device_status(self, device_id: str) -> dict[str, Any] | None:
        with self.lock:
            conn = self._conn()
            cur = conn.cursor()
            cur.execute(
                """
                SELECT device_id, updated_at, temperature, humidity, smoke, flame, intrusion,
                       mode, relay, buzzer, network, air780e
                FROM devices
                WHERE device_id = ?
                """,
                (device_id,),
            )
            row = cur.fetchone()
            conn.close()
        return dict(row) if row else None


db = DB(settings.db_path)

