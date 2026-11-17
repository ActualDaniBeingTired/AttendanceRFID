import csv
import io
import os
from datetime import date, datetime, timedelta
from typing import Optional

import psycopg2
from psycopg2.extras import RealDictCursor
from dotenv import load_dotenv
from fastapi import FastAPI, HTTPException, Query, Header
from pydantic import BaseModel, Field
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles

load_dotenv()
DATABASE_URL = os.getenv("DATABASE_URL")
APP_TIMEZONE = os.getenv("APP_TIMEZONE", "Asia/Jakarta")
API_KEY = os.getenv("API_KEY", "")
LATE_AFTER = os.getenv("LATE_AFTER", "07:30")

app = FastAPI(title="Attendance RFID Dashboard", version="1.0.0")
app.mount("/static", StaticFiles(directory="static"), name="static")


def db():
    if not DATABASE_URL:
        raise HTTPException(500, "DATABASE_URL belum diatur. Salin .env.example menjadi .env dan isi koneksi PostgreSQL.")
    try:
        return psycopg2.connect(DATABASE_URL, cursor_factory=RealDictCursor)
    except Exception as exc:
        raise HTTPException(503, f"Tidak dapat terhubung ke PostgreSQL: {exc}")


def query(sql, params=()):
    conn = db()
    try:
        with conn, conn.cursor() as cur:
            cur.execute(sql, params)
            try:
                return [dict(row) for row in cur.fetchall()]
            except psycopg2.ProgrammingError:
                return []
    finally:
        conn.close()


@app.get("/")
def home():
    return FileResponse("static/index.html")



class TapPayload(BaseModel):
    uid: str = Field(min_length=1, max_length=32)
    device: str = Field(default="door-1", min_length=1, max_length=80)
    date: date
    time: str


@app.post("/api/tap")
def receive_tap(payload: TapPayload, x_api_key: Optional[str] = Header(default=None)):
    # Request/response contract follows sketch.ino.
    if API_KEY and x_api_key != API_KEY:
        raise HTTPException(401, "Invalid API key")
    try:
        tap_time = datetime.strptime(payload.time, "%H:%M:%S").time()
    except ValueError:
        try:
            tap_time = datetime.strptime(payload.time, "%H:%M").time()
        except ValueError:
            raise HTTPException(422, "time must use HH:MM or HH:MM:SS")

    conn = db()
    try:
        with conn:
            with conn.cursor() as cur:
                cur.execute("""
                    SELECT s.id, s.name FROM cards c
                    JOIN students s ON s.id = c.student_id
                    WHERE upper(c.uid) = upper(%s) LIMIT 1
                """, (payload.uid,))
                student = cur.fetchone()
                if not student:
                    cur.execute("""
                        INSERT INTO unknown (uid, device, tapped_at)
                        VALUES (upper(%s), %s, ((%s || ' ' || %s)::timestamp AT TIME ZONE 'Asia/Jakarta'))
                    """, (payload.uid, payload.device, payload.date.isoformat(), tap_time.strftime("%H:%M:%S")))
                    return {"status": "unknown", "name": ""}

                cur.execute("""
                    SELECT 1 FROM attendance
                    WHERE student_id = %s AND attendance_date = %s LIMIT 1
                """, (student["id"], payload.date))
                if cur.fetchone():
                    return {"status": "duplicate", "name": student["name"]}

                cutoff = datetime.strptime(LATE_AFTER, "%H:%M").time()
                attendance_status = "late" if tap_time > cutoff else "on_time"
                cur.execute("""
                    INSERT INTO attendance (student_id, device, tapped_at, attendance_date, status)
                    VALUES (%s, %s, ((%s || ' ' || %s)::timestamp AT TIME ZONE 'Asia/Jakarta'), %s, %s)
                    ON CONFLICT (student_id, attendance_date) DO NOTHING
                    RETURNING student_id
                """, (student["id"], payload.device, payload.date.isoformat(),
                      tap_time.strftime("%H:%M:%S"), payload.date, attendance_status))
                if not cur.fetchone():
                    return {"status": "duplicate", "name": student["name"]}
                return {"status": "ok", "name": student["name"]}
    finally:
        conn.close()


@app.get("/api/health")
def health():
    rows = query("SELECT current_database() AS database, now() AS server_time")
    return {"status": "ok", **rows[0]}


@app.get("/api/dashboard")
def dashboard(day: Optional[date] = None):
    if day is None:
        chosen = query("""
            SELECT COALESCE(
              (SELECT MAX(attendance_date) FROM attendance WHERE attendance_date = CURRENT_DATE),
              (SELECT MAX(attendance_date) FROM attendance),
              CURRENT_DATE
            ) AS day
        """)
        day = chosen[0]["day"]
    rows = query("""
        SELECT
          (SELECT COUNT(*) FROM students) AS total_students,
          COUNT(*) FILTER (WHERE a.status = 'on_time') AS on_time,
          COUNT(*) FILTER (WHERE a.status = 'late') AS late,
          COUNT(a.id) AS present
        FROM attendance a
        WHERE a.attendance_date = %s
    """, (day,))
    s = rows[0]
    s["absent"] = max(int(s["total_students"]) - int(s["present"]), 0)
    s["attendance_rate"] = round((s["present"] / s["total_students"] * 100), 1) if s["total_students"] else 0
    s["date"] = day.isoformat()

    recent = query("""
        SELECT a.id, st.name, st.student_no, c.uid,
               a.device, a.tapped_at, a.attendance_date, a.status
        FROM attendance a
        JOIN students st ON st.id = a.student_id
        LEFT JOIN cards c ON c.student_id = st.id
        WHERE a.attendance_date = %s
        ORDER BY a.tapped_at DESC
        LIMIT 8
    """, (day,))
    return {"summary": s, "recent": recent}


@app.get("/api/attendance")
def attendance(
    start: Optional[date] = None,
    end: Optional[date] = None,
    status: Optional[str] = None,
    search: Optional[str] = None,
    limit: int = Query(300, ge=1, le=1000),
):
    start = start or (date.today() - timedelta(days=30))
    end = end or date.today()
    if start > end:
        raise HTTPException(400, "Tanggal mulai tidak boleh melewati tanggal akhir.")
    sql = """
      SELECT a.id, st.name, st.student_no, c.uid, a.device,
             a.tapped_at, a.attendance_date, a.status
      FROM attendance a
      JOIN students st ON st.id = a.student_id
      LEFT JOIN cards c ON c.student_id = st.id
      WHERE a.attendance_date BETWEEN %s AND %s
    """
    params = [start, end]
    if status in ("on_time", "late"):
        sql += " AND a.status = %s"
        params.append(status)
    if search:
        sql += " AND (st.name ILIKE %s OR st.student_no ILIKE %s OR c.uid ILIKE %s)"
        term = f"%{search}%"
        params.extend([term, term, term])
    sql += " ORDER BY a.tapped_at DESC LIMIT %s"
    params.append(limit)
    return query(sql, tuple(params))


@app.get("/api/students")
def students(search: Optional[str] = None):
    sql = """
      SELECT s.id, s.name, s.student_no, c.uid,
             COUNT(a.id) AS attendance_count,
             COUNT(a.id) FILTER (WHERE a.status = 'late') AS late_count
      FROM students s
      LEFT JOIN cards c ON c.student_id = s.id
      LEFT JOIN attendance a ON a.student_id = s.id
    """
    params = []
    if search:
        sql += " WHERE s.name ILIKE %s OR s.student_no ILIKE %s OR c.uid ILIKE %s"
        term = f"%{search}%"
        params = [term, term, term]
    sql += " GROUP BY s.id, s.name, s.student_no, c.uid ORDER BY s.name"
    return query(sql, tuple(params))


@app.get("/api/unknown")
def unknown(start: Optional[date] = None, end: Optional[date] = None):
    start = start or (date.today() - timedelta(days=30))
    end = end or date.today()
    return query("""
      SELECT id, uid, device, tapped_at
      FROM unknown
      WHERE tapped_at::date BETWEEN %s AND %s
      ORDER BY tapped_at DESC
      LIMIT 300
    """, (start, end))


@app.get("/api/trends")
def trends(days: int = Query(14, ge=7, le=90)):
    return query("""
      SELECT attendance_date AS day,
             COUNT(*) AS present,
             COUNT(*) FILTER (WHERE status = 'late') AS late,
             COUNT(*) FILTER (WHERE status = 'on_time') AS on_time
      FROM attendance
      WHERE attendance_date >= CURRENT_DATE - (%s * INTERVAL '1 day')
      GROUP BY attendance_date
      ORDER BY attendance_date
    """, (days,))


@app.get("/api/export.csv")
def export_csv(start: Optional[date] = None, end: Optional[date] = None):
    start = start or (date.today() - timedelta(days=30))
    end = end or date.today()
    rows = query("""
      SELECT st.student_no, st.name, c.uid, a.attendance_date,
             a.tapped_at, a.status, a.device
      FROM attendance a
      JOIN students st ON st.id = a.student_id
      LEFT JOIN cards c ON c.student_id = st.id
      WHERE a.attendance_date BETWEEN %s AND %s
      ORDER BY a.attendance_date DESC, a.tapped_at DESC
    """, (start, end))
    output = io.StringIO()
    writer = csv.DictWriter(output, fieldnames=[
        "student_no", "name", "uid", "attendance_date", "tapped_at", "status", "device"
    ])
    writer.writeheader()
    for row in rows:
        writer.writerow(row)
    output.seek(0)
    return StreamingResponse(
        iter([output.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename=attendance-{start}-{end}.csv"}
    )
