"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"
import { ScrollArea } from "@/components/ui/scroll-area"
import { Bot, Clock, MessageSquare, Activity, AlertCircle, CheckCircle2, Layers } from "lucide-react"
import { getAgents, getCronJobs, getSessions, getConfig } from "@/lib/api"

interface StatCardProps {
  title: string
  value: string | number
  icon: React.ElementType
  description?: string
}

function StatCard({ title, value, icon: Icon, description }: StatCardProps) {
  return (
    <Card className="bg-zinc-900 border-zinc-800">
      <CardHeader className="flex flex-row items-center justify-between pb-2">
        <CardTitle className="text-sm font-medium text-zinc-400">{title}</CardTitle>
        <Icon className="w-4 h-4 text-zinc-500" />
      </CardHeader>
      <CardContent>
        <div className="text-2xl font-bold text-zinc-100">{value}</div>
        {description && (
          <p className="text-xs text-zinc-500 mt-1">{description}</p>
        )}
      </CardContent>
    </Card>
  )
}

export default function HomePage() {
  const [agents, setAgents] = useState<any[]>([])
  const [cronJobs, setCronJobs] = useState<any[]>([])
  const [sessions, setSessions] = useState<any[]>([])
  const [config, setConfig] = useState<any>(null)
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    async function loadData() {
      try {
        const [agentsData, cronData, sessionsData, configData] = await Promise.all([
          getAgents(),
          getCronJobs(),
          getSessions(),
          getConfig(),
        ])
        setAgents(agentsData)
        setCronJobs(cronData)
        setSessions(sessionsData)
        setConfig(configData)
      } catch (error) {
        console.error('Failed to load data:', error)
      } finally {
        setLoading(false)
      }
    }
    loadData()
  }, [])

  if (loading) {
    return (
      <div className="p-6 flex items-center justify-center h-full">
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-violet-500" />
      </div>
    )
  }

  const activeSessions = sessions.filter(s => Date.now() - s.updatedAt < 300000)
  const enabledCrons = cronJobs.filter(c => c.enabled)
  const failedCrons = cronJobs.filter(c => c.state?.lastStatus === 'error')

  return (
    <div className="p-6 space-y-6">
      <div>
        <h2 className="text-2xl font-bold text-zinc-100">Dashboard Overview</h2>
        <p className="text-zinc-400">Welcome to your OpenClaw control center</p>
      </div>

      {/* Stats Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <StatCard title="Total Agents" value={agents.length} icon={Bot} />
        <StatCard 
          title="Active Sessions" 
          value={activeSessions.length} 
          icon={Activity}
          description={`${sessions.length} total sessions`}
        />
        <StatCard 
          title="Cron Jobs" 
          value={`${enabledCrons.length}/${cronJobs.length}`} 
          icon={Clock}
        />
        <StatCard 
          title="Providers" 
          value={Object.keys(config?.parsed?.models?.providers || {}).length} 
          icon={Layers}
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Active Sessions */}
        <Card className="bg-zinc-900 border-zinc-800">
          <CardHeader>
            <CardTitle className="text-zinc-100 flex items-center gap-2">
              <Activity className="w-5 h-5" />
              Active Sessions
            </CardTitle>
          </CardHeader>
          <CardContent>
            <ScrollArea className="h-[300px]">
              <div className="space-y-3">
                {sessions.length === 0 ? (
                  <p className="text-zinc-500 text-sm">No active sessions</p>
                ) : (
                  sessions.map((session) => (
                    <div
                      key={session.sessionId}
                      className="flex items-center justify-between p-3 rounded-lg bg-zinc-800/50"
                    >
                      <div className="flex items-center gap-3">
                        <div className="w-2 h-2 rounded-full bg-green-500" />
                        <div>
                          <p className="text-sm font-medium text-zinc-200">
                            {session.label || session.displayName || session.key}
                          </p>
                          <p className="text-xs text-zinc-500">
                            {session.channel} • {session.model}
                          </p>
                        </div>
                      </div>
                      <Badge variant="secondary" className="bg-zinc-800 text-zinc-400">
                        {session.totalTokens.toLocaleString()} tokens
                      </Badge>
                    </div>
                  ))
                )}
              </div>
            </ScrollArea>
          </CardContent>
        </Card>

        {/* Cron Jobs */}
        <Card className="bg-zinc-900 border-zinc-800">
          <CardHeader>
            <CardTitle className="text-zinc-100 flex items-center gap-2">
              <Clock className="w-5 h-5" />
              Scheduled Jobs
            </CardTitle>
          </CardHeader>
          <CardContent>
            <ScrollArea className="h-[300px]">
              <div className="space-y-3">
                {cronJobs.map((job) => (
                  <div
                    key={job.id}
                    className="flex items-center justify-between p-3 rounded-lg bg-zinc-800/50"
                  >
                    <div className="flex items-center gap-3">
                      {job.state?.lastStatus === 'error' ? (
                        <AlertCircle className="w-4 h-4 text-red-500" />
                      ) : job.state?.lastStatus === 'ok' ? (
                        <CheckCircle2 className="w-4 h-4 text-green-500" />
                      ) : (
                        <Clock className="w-4 h-4 text-zinc-500" />
                      )}
                      <div>
                        <p className="text-sm font-medium text-zinc-200">{job.name}</p>
                        <p className="text-xs text-zinc-500">
                          {job.schedule.expr || `Every ${job.schedule.everyMs}ms`}
                        </p>
                      </div>
                    </div>
                    <Badge variant={job.enabled ? "default" : "secondary"} className={job.enabled ? "bg-green-600" : ""}>
                      {job.enabled ? "Active" : "Disabled"}
                    </Badge>
                  </div>
                ))}
              </div>
            </ScrollArea>
          </CardContent>
        </Card>
      </div>

      {/* Agents Grid */}
      <Card className="bg-zinc-900 border-zinc-800">
        <CardHeader>
          <CardTitle className="text-zinc-100 flex items-center gap-2">
            <Bot className="w-5 h-5" />
            Configured Agents
          </CardTitle>
        </CardHeader>
        <CardContent>
          <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-6 gap-3">
            {agents.map((agent) => (
              <div
                key={agent.id}
                className="p-4 rounded-lg bg-zinc-800/50 border border-zinc-700/50 hover:border-zinc-600 transition-colors"
              >
                <div className="flex items-center gap-2 mb-2">
                  <Bot className="w-5 h-5 text-violet-500" />
                  <span className="font-medium text-zinc-200">{agent.id}</span>
                </div>
                <p className="text-xs text-zinc-500 truncate">
                  {agent.workspace}
                </p>
                {agent.subagents?.allowAgents?.length > 0 && (
                  <div className="mt-2 flex flex-wrap gap-1">
                    {agent.subagents.allowAgents.map((sub: string) => (
                      <Badge key={sub} variant="outline" className="text-[10px]">
                        {sub}
                      </Badge>
                    ))}
                  </div>
                )}
              </div>
            ))}
          </div>
        </CardContent>
      </Card>
    </div>
  )
}
