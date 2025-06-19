import { NextResponse } from 'next/server';
import { Pool } from 'pg';

const pool = new Pool({
  host: process.env.PGHOST,
  user: process.env.PGUSER,
  password: process.env.PGPASSWORD,
  database: process.env.PGDATABASE,
  port: process.env.PGPORT,
});

// GET: Lấy danh sách users (có role, department)
export async function GET(req) {
  const { pathname } = new URL(req?.url || '', 'http://localhost');
  // Nếu gọi /api/users/roles thì trả về roles
  if (pathname.endsWith('/users/roles')) {
    try {
      const result = await pool.query('SELECT * FROM roles ORDER BY id');
      return NextResponse.json(result.rows);
    } catch (err) {
      return NextResponse.json({ error: err.message }, { status: 500 });
    }
  }
  // Mặc định trả về users
  try {
    const result = await pool.query(`
      SELECT u.id, u.name, u.email, u.role_id, r.name AS role, u.department_id, d.name AS department
      FROM users u
      LEFT JOIN roles r ON u.role_id = r.id
      LEFT JOIN departments d ON u.department_id = d.id
      ORDER BY u.id
    `);
    return NextResponse.json(result.rows);
  } catch (err) {
    return NextResponse.json({ error: err.message }, { status: 500 });
  }
}

// POST: Thêm user mới
export async function POST(req) {
  try {
    const body = await req.json();
    const { name, email, role_id, department_id } = body;
    const result = await pool.query(
      'INSERT INTO users (name, email, role_id, department_id) VALUES ($1, $2, $3, $4) RETURNING *',
      [name, email, role_id, department_id]
    );
    return NextResponse.json(result.rows[0]);
  } catch (err) {
    return NextResponse.json({ error: err.message }, { status: 500 });
  }
}

// PUT: Sửa user
export async function PUT(req) {
  try {
    const body = await req.json();
    const { id, name, email, role_id, department_id } = body;
    const result = await pool.query(
      'UPDATE users SET name=$1, email=$2, role_id=$3, department_id=$4 WHERE id=$5 RETURNING *',
      [name, email, role_id, department_id, id]
    );
    return NextResponse.json(result.rows[0]);
  } catch (err) {
    return NextResponse.json({ error: err.message }, { status: 500 });
  }
}

// DELETE: Xóa user
export async function DELETE(req) {
  try {
    const { searchParams } = new URL(req.url);
    const id = searchParams.get('id');
    await pool.query('DELETE FROM users WHERE id=$1', [id]);
    return NextResponse.json({ success: true });
  } catch (err) {
    return NextResponse.json({ error: err.message }, { status: 500 });
  }
}
