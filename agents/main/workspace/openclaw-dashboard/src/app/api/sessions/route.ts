import { NextResponse } from 'next/server'

const API_BASE = process.env.OPENCLAW_API_URL || 'http://localhost:18789'

export async function GET() {
  try {
    const response = await fetch(`${API_BASE}/sessions/list?messageLimit=1`)
    const data = await response.json()
    return NextResponse.json(data)
  } catch (error) {
    return NextResponse.json({ error: 'Failed to fetch sessions' }, { status: 500 })
  }
}
