import { NextResponse } from 'next/server'
import { readdirSync, statSync, existsSync, readFileSync } from 'fs'
import { join, basename } from 'path'

interface SessionEntry {
  key: string
  kind?: string
  channel?: string
  label?: string
  displayName?: string
  updatedAt: number
  sessionId: string
  model?: string
  contextTokens?: number
  totalTokens?: number
}

export async function GET() {
  try {
    const sessions: SessionEntry[] = []
    const agentsDir = '/root/.openclaw/agents'
    
    // Read sessions from each agent's sessions directory
    const agentDirs = readdirSync(agentsDir)
    for (const agentId of agentDirs) {
      const sessionsDir = join(agentsDir, agentId, 'sessions')
      if (!existsSync(sessionsDir)) continue
      
      try {
        const files = readdirSync(sessionsDir)
        for (const file of files) {
          // Skip deleted sessions
          if (file.includes('.deleted.')) continue
          if (!file.endsWith('.jsonl')) continue
          
          const filePath = join(sessionsDir, file)
          const stat = statSync(filePath)
          const sessionId = file.replace('.jsonl', '')
          
          // Try to get model info from first line
          let model = 'unknown'
          let totalTokens = 0
          try {
            const content = readFileSync(filePath, 'utf-8')
            const lines = content.trim().split('\n')
            totalTokens = lines.length * 100 // Rough estimate
            
            // Try to find model in first few lines
            for (let i = 0; i < Math.min(5, lines.length); i++) {
              try {
                const line = JSON.parse(lines[i])
                if (line.model) model = line.model
                if (line.usage) totalTokens = (line.usage.total_tokens || 0)
              } catch {}
            }
          } catch {}
          
          sessions.push({
            key: `agent:${agentId}:${sessionId}`,
            sessionId,
            updatedAt: stat.mtimeMs,
            channel: 'webchat',
            model,
            totalTokens,
            contextTokens: totalTokens,
          })
        }
      } catch (e) {
        // Skip invalid directories
      }
    }
    
    // Sort by updatedAt descending
    sessions.sort((a, b) => b.updatedAt - a.updatedAt)
    
    return NextResponse.json({ sessions: sessions.slice(0, 50) })
  } catch (error) {
    console.error('Failed to read sessions:', error)
    return NextResponse.json({ sessions: [] }, { status: 200 })
  }
}
