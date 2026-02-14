import { NextResponse } from 'next/server'
import { readdirSync, statSync } from 'fs'
import { join } from 'path'

export async function GET() {
  try {
    const skillsDir = '/root/.openclaw/agents/main/workspace/skills'
    const skills: any[] = []
    
    const entries = readdirSync(skillsDir)
    for (const entry of entries) {
      const fullPath = join(skillsDir, entry)
      const stat = statSync(fullPath)
      if (stat.isDirectory()) {
        skills.push({
          name: entry,
          description: `Skill: ${entry}`,
          location: fullPath,
        })
      }
    }
    
    return NextResponse.json(skills)
  } catch (error) {
    return NextResponse.json([], { status: 200 })
  }
}
