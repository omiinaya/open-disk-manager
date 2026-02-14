import { NextResponse } from 'next/server'
import { readFileSync } from 'fs'

export async function GET() {
  try {
    const configPath = '/root/.openclaw/openclaw.json'
    const raw = readFileSync(configPath, 'utf-8')
    const config = JSON.parse(raw)
    const agents = config.agents?.list || []
    return NextResponse.json({ agents })
  } catch (error) {
    console.error('Failed to read agents:', error)
    return NextResponse.json({ agents: [] }, { status: 200 })
  }
}
