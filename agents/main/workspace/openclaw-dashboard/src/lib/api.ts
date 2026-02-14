// OpenClaw API Client - uses local API routes

async function fetchApi<T>(endpoint: string, options?: RequestInit): Promise<T> {
  const response = await fetch(`/api${endpoint}`, {
    ...options,
    headers: {
      'Content-Type': 'application/json',
      ...options?.headers,
    },
  })
  if (!response.ok) {
    throw new Error(`API Error: ${response.status} ${response.statusText}`)
  }
  return response.json()
}

// Types
export interface Agent {
  id: string
  workspace: string
  subagents?: {
    allowAgents: string[]
  }
}

export interface CronJob {
  id: string
  agentId: string
  name: string
  enabled: boolean
  schedule: {
    kind: 'cron' | 'every' | 'at'
    expr?: string
    everyMs?: number
    tz?: string
  }
  sessionTarget: 'main' | 'isolated'
  payload: {
    kind: 'systemEvent' | 'agentTurn'
    message?: string
    text?: string
  }
  state?: {
    nextRunAtMs: number
    lastRunAtMs?: number
    lastStatus?: string
    lastDurationMs?: number
    lastError?: string
  }
}

export interface Session {
  key: string
  kind: string
  channel: string
  label?: string
  displayName?: string
  updatedAt: number
  sessionId: string
  model: string
  contextTokens: number
  totalTokens: number
  abortedLastRun?: boolean
}

export interface Skill {
  name: string
  description: string
  location: string
}

// API Functions
export async function getAgents(): Promise<Agent[]> {
  try {
    const result = await fetchApi<{ agents?: Agent[] }>('/agents')
    return result.agents || []
  } catch {
    return []
  }
}

export async function getCronJobs(): Promise<CronJob[]> {
  try {
    const result = await fetchApi<{ jobs: CronJob[] }>('/cron')
    return result.jobs || []
  } catch {
    return []
  }
}

export async function getSessions(): Promise<Session[]> {
  try {
    const result = await fetchApi<{ sessions: Session[] }>('/sessions')
    return result.sessions || []
  } catch {
    return []
  }
}

export async function getConfig() {
  return fetchApi<any>('/config')
}

export async function patchConfig(patch: any) {
  return fetchApi<any>('/config', {
    method: 'POST',
    body: JSON.stringify(patch),
  })
}

export async function getSkills(): Promise<Skill[]> {
  try {
    return await fetchApi<Skill[]>('/skills')
  } catch {
    return []
  }
}

export async function abortSession(sessionKey: string) {
  return fetchApi(`/abort/${encodeURIComponent(sessionKey)}`, {
    method: 'POST',
  })
}

export async function runCronJob(jobId: string) {
  return fetchApi(`/cron/${jobId}/run`, {
    method: 'POST',
  })
}
