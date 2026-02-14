import { NextResponse } from 'next/server'
import { readFileSync } from 'fs'

export async function GET() {
  try {
    const cronPath = '/root/.openclaw/cron/jobs.json'
    const raw = readFileSync(cronPath, 'utf-8')
    const data = JSON.parse(raw)
    // The file has { version, jobs: [...] } structure
    const jobs = data.jobs || []
    return NextResponse.json({ jobs })
  } catch (error) {
    console.error('Failed to read cron jobs:', error)
    return NextResponse.json({ jobs: [] }, { status: 200 })
  }
}
