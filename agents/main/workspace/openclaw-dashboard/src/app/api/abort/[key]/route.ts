import { NextResponse, NextRequest } from 'next/server'

const API_BASE = process.env.OPENCLAW_API_URL || 'http://localhost:18789'

export async function POST(
  request: NextRequest,
  { params }: { params: Promise<{ key: string }> }
) {
  try {
    const { key } = await params
    const decodedKey = decodeURIComponent(key)
    const response = await fetch(`${API_BASE}/sessions/${encodeURIComponent(decodedKey)}/abort`, {
      method: 'POST',
    })
    const data = await response.json()
    return NextResponse.json(data)
  } catch (error) {
    return NextResponse.json({ error: 'Failed to abort session' }, { status: 500 })
  }
}
