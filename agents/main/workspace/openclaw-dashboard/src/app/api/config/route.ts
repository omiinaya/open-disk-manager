import { NextResponse } from 'next/server'

const API_BASE = process.env.OPENCLAW_API_URL || 'http://localhost:18789'

export async function GET() {
  try {
    const response = await fetch(`${API_BASE}/config`)
    const data = await response.json()
    return NextResponse.json(data)
  } catch (error) {
    return NextResponse.json({ error: 'Failed to fetch config' }, { status: 500 })
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json()
    const response = await fetch(`${API_BASE}/config/patch`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    })
    const data = await response.json()
    return NextResponse.json(data)
  } catch (error) {
    return NextResponse.json({ error: 'Failed to patch config' }, { status: 500 })
  }
}
