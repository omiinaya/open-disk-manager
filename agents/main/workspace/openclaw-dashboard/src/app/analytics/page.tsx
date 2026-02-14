"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select"
import { BarChart3, TrendingUp, Clock, DollarSign, Activity } from "lucide-react"
import { getSessions, getConfig } from "@/lib/api"
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, LineChart, Line, PieChart, Pie, Cell } from "recharts"

export default function AnalyticsPage() {
  const [sessions, setSessions] = useState<any[]>([])
  const [config, setConfig] = useState<any>(null)
  const [loading, setLoading] = useState(true)
  const [timeRange, setTimeRange] = useState("24h")

  useEffect(() => {
    async function loadData() {
      try {
        const [sessionsData, configData] = await Promise.all([
          getSessions(),
          getConfig(),
        ])
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

  // Calculate stats
  const totalTokens = sessions.reduce((sum, s) => sum + s.totalTokens, 0)
  const avgTokensPerSession = sessions.length > 0 ? Math.round(totalTokens / sessions.length) : 0
  const modelUsage: Record<string, number> = {}
  sessions.forEach(s => {
    modelUsage[s.model] = (modelUsage[s.model] || 0) + s.totalTokens
  })

  const chartData = Object.entries(modelUsage).map(([model, tokens]) => ({
    model: model.split('/').pop() || model,
    tokens,
  }))

  const responseTimeData = [
    { label: '<100ms', count: 12 },
    { label: '100-500ms', count: 45 },
    { label: '500ms-1s', count: 28 },
    { label: '1-3s', count: 15 },
    { label: '>3s', count: 3 },
  ]

  const colors = ['#8b5cf6', '#ec4899', '#3b82f6', '#10b981', '#f59e0b', '#ef4444']

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
          <h2 className="text-2xl font-bold text-zinc-100">Analytics</h2>
          <p className="text-zinc-400">Model usage, tokens, response times, and performance metrics</p>
        </div>
        <Select value={timeRange} onValueChange={setTimeRange}>
          <SelectTrigger className="w-32 bg-zinc-900">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="1h">1 Hour</SelectItem>
            <SelectItem value="24h">24 Hours</SelectItem>
            <SelectItem value="7d">7 Days</SelectItem>
            <SelectItem value="30d">30 Days</SelectItem>
          </SelectContent>
        </Select>
      </div>

      {/* Stats Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-2xl font-bold text-zinc-100">{totalTokens.toLocaleString()}</p>
                <p className="text-sm text-zinc-500">Total Tokens</p>
              </div>
              <div className="p-2 rounded-lg bg-violet-900/30">
                <TrendingUp className="w-5 h-5 text-violet-500" />
              </div>
            </div>
          </CardContent>
        </Card>

        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-2xl font-bold text-zinc-100">{avgTokensPerSession}</p>
                <p className="text-sm text-zinc-500">Avg Tokens/Session</p>
              </div>
              <div className="p-2 rounded-lg bg-blue-900/30">
                <BarChart3 className="w-5 h-5 text-blue-500" />
              </div>
            </div>
          </CardContent>
        </Card>

        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-2xl font-bold text-zinc-100">{sessions.length}</p>
                <p className="text-sm text-zinc-500">Active Sessions</p>
              </div>
              <div className="p-2 rounded-lg bg-green-900/30">
                <Activity className="w-5 h-5 text-green-500" />
              </div>
            </div>
          </CardContent>
        </Card>

        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-2xl font-bold text-zinc-100">$0.00</p>
                <p className="text-sm text-zinc-500">Est. Cost</p>
              </div>
              <div className="p-2 rounded-lg bg-yellow-900/30">
                <DollarSign className="w-5 h-5 text-yellow-500" />
              </div>
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Charts */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Token Usage by Model */}
        <Card className="bg-zinc-900 border-zinc-800">
          <CardHeader>
            <CardTitle className="text-zinc-100">Token Usage by Model</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="h-[300px]">
              <ResponsiveContainer width="100%" height="100%">
                <BarChart data={chartData}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#333" />
                  <XAxis dataKey="model" stroke="#666" />
                  <YAxis stroke="#666" />
                  <Tooltip 
                    contentStyle={{ backgroundColor: '#18181b', border: '1px solid #27272a' }}
                    labelStyle={{ color: '#a1a1aa' }}
                  />
                  <Bar dataKey="tokens" fill="#8b5cf6" radius={[4, 4, 0, 0]} />
                </BarChart>
              </ResponsiveContainer>
            </div>
          </CardContent>
        </Card>

        {/* Response Time Distribution */}
        <Card className="bg-zinc-900 border-zinc-800">
          <CardHeader>
            <CardTitle className="text-zinc-100">Response Times</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="h-[300px]">
              <ResponsiveContainer width="100%" height="100%">
                <LineChart data={responseTimeData}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#333" />
                  <XAxis dataKey="label" stroke="#666" />
                  <YAxis stroke="#666" />
                  <Tooltip 
                    contentStyle={{ backgroundColor: '#18181b', border: '1px solid #27272a' }}
                    labelStyle={{ color: '#a1a1aa' }}
                  />
                  <Line type="monotone" dataKey="count" stroke="#3b82f6" strokeWidth={2} />
                </LineChart>
              </ResponsiveContainer>
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Model Breakdown */}
      <Card className="bg-zinc-900 border-zinc-800">
        <CardHeader>
          <CardTitle className="text-zinc-100">Model Breakdown</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            {Object.entries(modelUsage).map(([model, tokens], i) => (
              <div key={model} className="flex items-center justify-between p-4 rounded-lg bg-zinc-800/50">
                <div className="flex items-center gap-3">
                  <div className="w-3 h-3 rounded-full" style={{ backgroundColor: colors[i % colors.length] }} />
                  <span className="font-medium text-zinc-200">{model}</span>
                </div>
                <div className="text-right">
                  <p className="font-medium text-zinc-100">{tokens.toLocaleString()}</p>
                  <p className="text-xs text-zinc-500">tokens</p>
                </div>
              </div>
            ))}
          </div>
        </CardContent>
      </Card>
    </div>
  )
}