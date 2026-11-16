CREATE TABLE students (
  id          INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  name        TEXT NOT NULL,
  student_no  TEXT UNIQUE
);

CREATE TABLE cards (
  uid         TEXT PRIMARY KEY,                       
  student_id  INT NOT NULL REFERENCES students(id),
  active      BOOLEAN NOT NULL DEFAULT TRUE           
);

CREATE TABLE attendance (
  id               INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  student_id       INT NOT NULL REFERENCES students(id),
  device           TEXT NOT NULL,
  tapped_at        TIMESTAMPTZ NOT NULL DEFAULT now(),
  attendance_date  DATE NOT NULL,                     
  status           TEXT NOT NULL CHECK (status IN ('on_time', 'late')),
  UNIQUE (student_id, attendance_date)                
);

CREATE TABLE unknown (
  id         INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  uid        TEXT NOT NULL,
  device     TEXT,
  tapped_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);