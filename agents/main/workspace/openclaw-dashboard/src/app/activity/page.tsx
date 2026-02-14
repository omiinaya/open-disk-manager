"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { ScrollArea } from "@/components/ui/scroll-area"
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from "@/components/ui/dialog"
import { Progress } from "@/components/ui/progress"
import { Activity, StopCircle, RefreshCw, MessageSquare, Clock, Zap } from "lucide-react"
import { getSessions, abortSession } from "@/lib/api"
import { formatDistanceToNow } from "date-fns"

interface SessionDetail {
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
  lastChannel?: string
  transcriptPath?: string
}

export default function ActivityPage() {
  const [sessions, setSessions] = useState<SessionDetail[]>([])
  const [loading, setLoading] = useState(true)
  const [aborting, setAborting] = useState<string | null>(null)
  const [selectedSession, setSelectedSession] = useState<SessionDetail | null>(null)

  const loadSessions = async () => {
    try {
      const data = await getSessions()
      setSessions(data)
    } catch (error) {
      console.error('Failed to load sessions:', error)
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    loadSessions()
    const interval = setInterval(loadSessions, 5000) // Refresh every 5s
    return () => clearInterval(interval)
  }, [])

  const handleAbort = async (sessionKey: string) => {
    if (!confirm('Are you sure you want to abort this session?')) return
    
    setAborting(sessionKey)
    try {
      await abortSession(sessionKey)
      await loadSessions()
    } catch (error) {
      console.error('Failed to abort session:', error)
      alert('Failed to abort session')
    } finally {
      setAborting(null)
    }
  }

  const getTimeSinceUpdate = (timestamp: number) => {
    return formatDistanceToNow(new Date(timestamp), { addSuffix: true })
  }

  const getActivityLevel = (updatedAt: number): 'active' | 'idle' | 'stale' => {
    const now = Date.now()
    const diff = now - updatedAt
    if (diff < 60000) return 'active' // < 1 min
    if (diff < 300000) return 'idle' // < 5 min
    return 'stale'
  }

  if (loading) {
    return (
      <div className="p-6 flex items-center justify-center h-full">
        <div className="animate-spin rounded-full h-8 w-8 border-b-2 border-violet-500" />
      </div>
    )
  }

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-zinc-100">Agent Activity</h2>
          <p className="text-zinc-400">Monitor what each agent is doing in real-time</p>
        </div>
        <Button variant="outline" onClick={loadSessions}>
          <RefreshCw className="w-4 h-4 mr-2" />
          Refresh
        </Button>
      </div>

      {/* Stats */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <div className="p-2 rounded-lg bg-green-900/30">
                <Activity className="w-5 h-5 text-green-500" />
              </div>
              <div>
                <p className="text-2xl font-bold text-zinc-100">
                  {sessions.filter(s => getActivityLevel(s.updatedAt) === 'active').length}
                </p>
                <p className="text-sm text-zinc-500">Active</p>
              </div>
            </div>
          </CardContent>
        </Card>
        
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <div className="p-2 rounded-lg bg-yellow-900/30">
                <Clock className="w-5 h-5 text-yellow-500" />
              </div>
              <div>
                <p className="text-2xl font-bold text-zinc-100">
                  {sessions.filter(s => getActivityLevel(s.updatedAt) === 'idle').length}
                </p>
                <p className="text-sm text-zinc-500">Idle</p>
              </div>
            </div>
          </CardContent>
        </Card>
        
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <div className="p-2 rounded-lg bg-red-900/30">
                <StopCircle className="w-5 h-5 text-red-500" />
              </div>
              <div>
                <p className="text-2xl font-bold text-zinc-100">
                  {sessions.filter(s => getActivityLevel(s.updatedAt) === 'stale').length}
                </p>
                <p className="text-sm text-zinc-500">Stale</p>
              </div>
            </div>
          </CardContent>
        </Card>
        
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <div className="p-2 rounded-lg bg-violet-900/30">
                <Zap className="w-5 h-5 text-violet-500" />
              </div>
              <div>
                <p className="text-2xl font-bold text-zinc-100">
                  {sessions.reduce((sum, s) => sum + s.totalTokens, 0).toLocaleString()}
                </p>
                <p className="text-sm text-zinc-500">Total Tokens</p>
              </div>
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Sessions List */}
      <div className="space-y-3">
        {sessions.map((session) => {
          const activityLevel = getActivityLevel(session.updatedAt)
          return (
            <Card 
              key={session.sessionId} 
              className={`bg-zinc-900 border-zinc-800 ${
                session.abortedLastRun ? 'border-red-800' : ''
              }`}
            >
              <CardContent className="p-4">
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-4">
                    {/* Status Indicator */}
                    <div className="relative">
                      <div className={`w-3 h-3 rounded-full ${
                        activityLevel === 'active' ? 'bg-green-500 animate-pulse' :
                        activityLevel === 'idle' ? 'bg-yellow-500' :
                        'bg-red-500'
                      }`} />
                    </div>
                    
                    {/* Session Info */}
                    <div>
                      <div className="flex items-center gap-2">
                        <h3 className="font-medium text-zinc-100">
                          {session.label || session.displayName || session.key.split(':').slice(0, 2).join(':')}
                        </h3>
                        <Badge variant="outline" className="bg-zinc-800 text-zinc-400">
                          {session.channel}
                        </Badge>
                        {session.abortedLastRun && (
                          <Badge variant="destructive" className="bg-red-900">
                            Aborted
                          </Badge>
                        )}
                      </div>
                      <div className="flex items-center gap-3 mt-1 text-sm text-zinc-500">
                        <span className="flex items-center gap-1">
                          <MessageSquare className="w-3 h-3" />
                          {session.model}
                        </span>
                        <span>{getTimeSinceUpdate(session.updatedAt)}</span>
                      </div>
                    </div>
                  </div>

                  {/* Token Stats */}
                  <div className="flex items-center gap-6">
                    <div className="text-right">
                      <p className="text-sm font-medium text-zinc-300">
                        {session.contextTokens.toLocaleString()}
                      </p>
                      <p className="text-xs text-zinc-500">Context</p>
                    </div>
                    <div className="text-right">
                      <p className="text-sm font-medium text-zinc-300">
                        {session.totalTokens.toLocaleString()}
                      </p>
                      <p className="text-xs text-zinc-500">Total</p>
                    </div>
                    
                    <div className="flex gap-2">
                      <Dialog>
                        <DialogTrigger asChild>
                          <Button variant="outline" size="sm">
                            View Details
                          </Button>
                        </DialogTrigger>
                        <DialogContent className="bg-zinc-900 border-zinc-800 max-w-2xl">
                          <DialogHeader>
                            <DialogTitle className="text-zinc-100">
                              Session Details
                            </DialogTitle>
                          </DialogHeader>
                          <div className="space-y-4">
                            <div className="grid grid-cols-2 gap-4">
                              <div>
                                <Label className="text-zinc-500">Session Key</Label>
                                <p className="font-mono text-sm text-zinc-300">{session.key}</p>
                              </div>
                              <div>
                                <Label className="text-zinc-500">Session ID</Label>
                                <p className="font-mono text-sm text-zinc-300">{session.sessionId}</p>
                              </div>
                              <div>
                                <Label className="text-zinc-500">Model</Label>
                                <p className="text-zinc-300">{session.model}</p>
                              </div>
                              <div>
                                <Label className="text-zinc-500">Channel</Label>
                                <p className="text-zinc-300">{session.channel}</p>
                              </div>
                            </div>
                          </div>
                        </DialogContent>
                      </Dialog>
                      
                      <Button 
                        variant="outline" 
                        size="sm"
                        className="text-red-400 border-red-800 hover:bg-red-900/30"
                        onClick={() => handleAbort(session.key)}
                        disabled={aborting === session.key}
                      >
                        {aborting === session.key ? (
                          <RefreshCw className="w-4 h-4 animate-spin" />
                        ) : (
                          <StopCircle className="w-4 h-4" />
                        )}
                      </Button>
                    </div>
                  </div>
                </div>
              </CardContent>
            </Card>
          )
        })}

        {sessions.length === 0 && (
          <Card className="bg-zinc-900 border-zinc-800">
            <CardContent className="p-8 text-center text-zinc-500">
              No active sessions
            </CardContent>
          </Card>
        )}
      </div>
    </div>
  )
}

function Label({ children, className }: { children: React.ReactNode; className?: string }) {
  return <p className={`text-xs text-zinc-500 mb-1 ${className}`}>{children}</p>
}
