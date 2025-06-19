-- Tạo bảng roles (vai trò người dùng)
CREATE TABLE roles (
  id SERIAL PRIMARY KEY,
  name TEXT UNIQUE NOT NULL
);

-- Tạo bảng departments (lớp/phòng ban)
CREATE TABLE departments (
  id SERIAL PRIMARY KEY,
  name TEXT UNIQUE NOT NULL
);

-- Tạo bảng users (người dùng)
CREATE TABLE users (
  id SERIAL PRIMARY KEY,
  name TEXT NOT NULL,
  email TEXT UNIQUE NOT NULL,
  role_id INTEGER REFERENCES roles(id),
  department_id INTEGER REFERENCES departments(id),
  fingerprint_template BYTEA  -- chỉ cần nếu bạn lưu template
);

-- Tạo bảng checkin_logs (lịch sử chấm công)
CREATE TABLE checkin_logs (
  id SERIAL PRIMARY KEY,
  user_id INTEGER REFERENCES users(id),
  check_type TEXT CHECK (check_type IN ('checkin', 'checkout')),
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
