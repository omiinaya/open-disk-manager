import { NextResponse } from 'next/server'
import { readFileSync } from 'fs'

export async function GET() {
  try {
    const configPath = '/root/.openclaw/openclaw.json'
    const raw = readFileSync(configPath, 'utf-8')
    const parsed = JSON.parse(raw)
    return NextResponse.json({ 
      parsed, 
      raw,
      agents: { list: parsed.agents?.list || [] }
    })
  } catch (error) {
    console.error('Failed to read config:', error)
    return NextResponse.json({ error: 'Failed to read config', agents: { list: [] } }, { status: 500 })
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json()
    // In production, this would patch the config
    // For now, return success
    return NextResponse.json({ success: true, message: 'Config patch not implemented in demo mode' })
  } catch (error) {
    return NextResponse.json({ error: 'Failed to patch config' }, { status: 500 })
  }
}
