INSERT INTO students (name, student_no) VALUES
  ('Daniel Sanjaya',   '2024001'),
  ('Andi Makmur',      '2024002'),
  ('Budi Santoso',     '2024003'),
  ('Citra Lestari',    '2024004'),
  ('Dewi Anggraini',   '2024005'),
  ('Eko Prasetyo',     '2024006'),
  ('Fajar Nugroho',    '2024007'),
  ('Gita Permata',     '2024008'),
  ('Hendra Wijaya',    '2024009'),
  ('Indah Sari',       '2024010'),
  ('Joko Susilo',      '2024011'),
  ('Kartika Putri',    '2024012'),
  ('Lukman Hakim',     '2024013'),
  ('Maya Kusuma',      '2024014'),
  ('Nanda Pratama',    '2024015'),
  ('Olivia Tan',       '2024016'),
  ('Putu Ariana',      '2024017'),
  ('Rizky Ramadhan',   '2024018'),
  ('Sinta Maharani',   '2024019'),
  ('Tegar Saputra',    '2024020'),
  ('Umar Fauzi',       '2024021'),
  ('Vina Oktaviani',   '2024022'),
  ('Wahyu Hidayat',    '2024023'),
  ('Yuni Astuti',      '2024024'),
  ('Zaki Firmansyah',  '2024025'),
  ('Aditya Rahman',    '2024026'),
  ('Bella Safitri',    '2024027'),
  ('Cahyo Wibowo',     '2024028'),
  ('Dian Puspitasari', '2024029'),
  ('Fikri Alamsyah',   '2024030'),
  ('Ghina Nurhaliza',  '2024031'),
  ('Haris Kurniawan',  '2024032');

INSERT INTO cards (uid, student_id) VALUES
  ('01020304', 1),
  ('11223344', 2),
  ('3F8A21C7', 3),
  ('7B04E9D2', 4),
  ('A92C5F18', 5),
  ('5D13B7E0', 6),
  ('C846A39B', 7),
  ('2E7F90A4', 8),
  ('91B5D6C3', 9),
  ('E40A78F5', 10),
  ('68C2B1D9', 11),
  ('0F9E43A6', 12),
  ('B3D7258E', 13),
  ('4A61F8C0', 14),
  ('D95B0E72', 15),
  ('1C8A6F3B', 16),
  ('8E27D4A9', 17),
  ('F60B93C1', 18),
  ('37D5A8E4', 19),
  ('9A4C1B60', 20),
  ('6F82E3D7', 21),
  ('C1B9047A', 22),
  ('52E6D8F3', 23),
  ('AD03719C', 24),
  ('7C95B2E8', 25),
  ('08F4A6D1', 26),
  ('E7219C5B', 27),
  ('4B8D03F6', 28),
  ('93A7E1C2', 29),
  ('2D60B98E', 30),
  ('B58F4C17', 31),
  ('6A13D7E5', 32);

WITH days AS (
  SELECT (current_date - n) AS day
  FROM generate_series(1, 28) AS n
  WHERE extract(isodow FROM current_date - n) < 6      -- Mon-Fri only
),
rolls AS (
  SELECT s.id AS student_id, d.day,
         random() AS r_absent,
         random() AS r_late,
         random() AS r_min
  FROM students s
  CROSS JOIN days d
),
decided AS (
  SELECT student_id, day, r_min,
         (r_late < 0.05 + (student_id % 4) * 0.08) AS is_late
  FROM rolls
  WHERE r_absent >= 0.03 + (student_id % 5) * 0.03     -- survivors are present
)
INSERT INTO attendance (student_id, device, tapped_at, attendance_date, status)
SELECT
  student_id,
  'door-1',
  (day + CASE
           WHEN is_late THEN TIME '07:31' + (r_min * 39) * interval '1 minute'
           ELSE              TIME '06:45' + (r_min * 44) * interval '1 minute'
         END) AT TIME ZONE 'Asia/Jakarta',
  day,
  CASE WHEN is_late THEN 'late' ELSE 'on_time' END
FROM decided
ON CONFLICT (student_id, attendance_date) DO NOTHING;

INSERT INTO unknown (uid, device, tapped_at)
SELECT upper(substr(md5(random()::text), 1, 8)),
       'door-1',
       now() - random() * interval '28 days'
FROM generate_series(1, 12);