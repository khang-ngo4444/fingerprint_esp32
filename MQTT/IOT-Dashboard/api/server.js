require('dotenv').config({ path: '../.env' });
const express = require('express');
const { Pool } = require('pg');
const cors = require('cors');

const app = express();
const port = 3001;

app.use(cors());
app.use(express.json());

const pool = new Pool({
  host: process.env.PGHOST,
  user: process.env.PGUSER,
  password: process.env.PGPASSWORD,
  database: process.env.PGDATABASE,
  port: process.env.PGPORT,
});

// Get all users with role & department
app.get('/users', async (req, res) => {
  try {
    const result = await pool.query(`
      SELECT u.id, u.name, u.email, r.name AS role, d.name AS department
      FROM users u
      LEFT JOIN roles r ON u.role_id = r.id
      LEFT JOIN departments d ON u.department_id = d.id
      ORDER BY u.id
    `);
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Get all roles
app.get('/roles', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM roles ORDER BY id');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Get all departments
app.get('/departments', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM departments ORDER BY id');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Get all checkin logs
app.get('/checkin_logs', async (req, res) => {
  try {
    const result = await pool.query(`
      SELECT l.id, l.user_id, u.name, l.check_type, l.timestamp
      FROM checkin_logs l
      LEFT JOIN users u ON l.user_id = u.id
      ORDER BY l.timestamp DESC
    `);
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

app.listen(port, () => {
  console.log(`API server listening at http://localhost:${port}`);
});
