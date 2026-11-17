# Attendance RFID Dashboard

Dashboard web menggunakan FastAPI + PostgreSQL + HTML/CSS/Vanilla JavaScript.

## Fitur
- Overview: total mahasiswa, hadir, terlambat, tidak hadir, attendance rate.
- Trend kehadiran harian.
- Attendance records dengan filter tanggal/status/pencarian.
- Student directory dan status kartu RFID.
- Unknown RFID scans.
- Export CSV.
- Status koneksi PostgreSQL.
- Endpoint `POST /api/tap` kompatibel dengan payload dari `sketch.ino`.

## Struktur database yang diasumsikan
- `students(id, name, student_no)`
- `cards(id, uid, student_id)`
- `attendance(id, student_id, device, tapped_at, attendance_date, status)`
- `unknown(id, uid, device, tapped_at)`

Kolom `id` pada `attendance` dan `unknown` diasumsikan ada. Jika schema.sql milik repository memakai nama/tipe berbeda, sesuaikan query di `main.py`.

## Menjalankan di Windows PowerShell
1. Buat virtual environment:
   ```powershell
   py -m venv .venv
   .venv\Scripts\Activate.ps1
   ```
2. Install dependency:
   ```powershell
   pip install -r requirements.txt
   ```
3. Salin `.env.example` menjadi `.env`, lalu sesuaikan password/database:
   ```env
   DATABASE_URL=postgresql://postgres:password@localhost:5432/AttendanceRFID
   APP_TIMEZONE=Asia/Jakarta
   ```
4. Pastikan schema dan data seed sudah dijalankan di database PostgreSQL.
5. Jalankan server:
   ```powershell
   uvicorn main:app --reload
   ```
6. Buka http://127.0.0.1:8000 dan dokumentasi API http://127.0.0.1:8000/docs.

## Integrasi ESP32
`sketch.ino` saat ini menggunakan `USE_SERVER false`. Untuk mengirim scan ke FastAPI:
1. Ubah `USE_SERVER` menjadi `true`.
2. Atur `SERVER_URL` ke alamat backend HTTPS yang dapat dijangkau ESP32. Firmware saat ini memakai `WiFiClientSecure`; untuk pengujian LAN dengan HTTP biasa, ubah firmware agar memakai `WiFiClient` dan URL `http://IP-KOMPUTER:8000/api/tap`.
3. Samakan `API_KEY` di `.env` dengan `API_KEY` pada firmware. `client.setInsecure()` hanya layak untuk pengujian; gunakan sertifikat CA untuk penggunaan serius.
4. Pastikan ESP32 dan komputer server berada di jaringan yang saling terhubung.

Endpoint menerima `uid`, `device`, `date` (YYYY-MM-DD), dan `time` (HH:MM atau HH:MM:SS). UID terdaftar dicatat sekali per mahasiswa per tanggal; UID tidak terdaftar masuk ke tabel `unknown`. Batas terlambat default pukul 07:30 dan dapat diubah melalui `LATE_AFTER`.

## Catatan penting
- Dashboard membaca database yang sama dengan sistem ESP32.
- Dashboard ini belum menerima scan langsung dari ESP32. Firmware harus mengirim HTTP request ke endpoint backend dan endpoint ingest perlu ditambahkan sesuai payload aktual dari `sketch.ino`.
- Data `fakedataForAttendance.sql` menghasilkan data historis untuk hari kerja sebelumnya, bukan presensi real-time hari ini.
- Status absent dihitung sebagai total mahasiswa dikurangi jumlah mahasiswa dengan attendance pada tanggal terpilih. Ini mengasumsikan semua mahasiswa wajib hadir pada tanggal tersebut; belum ada tabel jadwal/perkuliahan.
- Unknown cards hanya ditampilkan untuk review; belum ada fungsi assign UID ke mahasiswa.
