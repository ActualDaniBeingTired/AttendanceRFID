SELECT * FROM students;
SELECT * FROM cards;
SELECT * FROM attendance;
SELECT * FROM unknown;

SELECT
  (SELECT count(*) FROM students)     AS TotalStudents,
  (SELECT count(*) FROM cards)        AS TotalCards,
  (SELECT count(*) FROM attendance)   AS AttendanceRows,
  (SELECT count(*) FROM unknown) 	  AS UnknownTaps;

SELECT attendance_date,
       count(*) FILTER (WHERE status = 'on_time') AS on_time,
       count(*) FILTER (WHERE status = 'late')    AS late,
       count(*)                                   AS present
FROM attendance
GROUP BY attendance_date
ORDER BY attendance_date;

SELECT s.name,
       count(a.id) AS days_present,
       count(a.id) FILTER (WHERE a.status = 'late') AS days_late
FROM students s
LEFT JOIN attendance a ON a.student_id = s.id
GROUP BY s.id, s.name
ORDER BY days_present;

  

 
