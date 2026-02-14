"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { ScrollArea } from "@/components/ui/scroll-area"
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from "@/components/ui/dialog"
import { Textarea } from "@/components/ui/textarea"
import { Label } from "@/components/ui/label"
import { Input } from "@/components/ui/input"
import { Bot, Plus, Edit, FileText, Check, X } from "lucide-react"
import { getAgents, getConfig, patchConfig } from "@/lib/api"

export default function AgentsPage() {
  const [agents, setAgents] = useState<any[]>([])
  const [config, setConfig] = useState<any>(null)
  const [loading, setLoading] = useState(true)
  const [selectedAgent, setSelectedAgent] = useState<any>(null)
  const [configContent, setConfigContent] = useState("")
  const [saving, setSaving] = useState(false)

  useEffect(() => {
    async function loadData() {
      try {
        const [agentsData, configData] = await Promise.all([
          getAgents(),
          getConfig(),
        ])
        setAgents(agentsData)
        setConfig(configData)
      } catch (error) {
        console.error('Failed to load data:', error)
      } finally {
        setLoading(false)
      }
    }
    loadData()
  }, [])

  const openConfigEditor = (agent: any) => {
    setSelectedAgent(agent)
    setConfigContent(JSON.stringify({
      id: agent.id,
      workspace: agent.workspace,
      subagents: agent.subagents,
    }, null, 2))
  }

  const saveAgentConfig = async () => {
    if (!selectedAgent) return
    setSaving(true)
    try {
      const parsed = JSON.parse(configContent)
      const currentList = config.parsed.agents.list || []
      const newList = currentList.map((a: any) => 
        a.id === selectedAgent.id ? { ...a, ...parsed } : a
      )
      await patchConfig({ agents: { list: newList } })
      setAgents(newList)
      setSelectedAgent(null)
    } catch (error) {
      console.error('Failed to save config:', error)
      alert('Invalid JSON')
    } finally {
      setSaving(false)
    }
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
          <h2 className="text-2xl font-bold text-zinc-100">Agents</h2>
          <p className="text-zinc-400">Manage and configure your OpenClaw agents</p>
        </div>
        <Button className="bg-violet-600 hover:bg-violet-700">
          <Plus className="w-4 h-4 mr-2" />
          Add Agent
        </Button>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {agents.map((agent) => (
          <Card key={agent.id} className="bg-zinc-900 border-zinc-800">
            <CardHeader className="pb-3">
              <div className="flex items-center justify-between">
                <CardTitle className="text-zinc-100 flex items-center gap-2">
                  <Bot className="w-5 h-5 text-violet-500" />
                  {agent.id}
                </CardTitle>
                <Badge variant="outline" className="bg-green-900/30 text-green-400 border-green-800">
                  Active
                </Badge>
              </div>
            </CardHeader>
            <CardContent className="space-y-4">
              <div>
                <Label className="text-zinc-500 text-xs">Workspace</Label>
                <p className="text-sm text-zinc-300 font-mono truncate">{agent.workspace}</p>
              </div>
              
              {agent.subagents?.allowAgents?.length > 0 && (
                <div>
                  <Label className="text-zinc-500 text-xs">Subagents</Label>
                  <div className="flex flex-wrap gap-1 mt-1">
                    {agent.subagents.allowAgents.map((sub: string) => (
                      <Badge key={sub} variant="secondary" className="bg-zinc-800">
                        {sub}
                      </Badge>
                    ))}
                  </div>
                </div>
              )}

              <div className="flex gap-2">
                <Button 
                  variant="outline" 
                  size="sm" 
                  className="flex-1"
                  onClick={() => openConfigEditor(agent)}
                >
                  <Edit className="w-3 h-3 mr-1" />
                  Config
                </Button>
                <Button variant="outline" size="sm" className="flex-1">
                  <FileText className="w-3 h-3 mr-1" />
                  Files
                </Button>
              </div>
            </CardContent>
          </Card>
        ))}
      </div>

      {/* Config Editor Dialog */}
      <Dialog open={!!selectedAgent} onOpenChange={() => setSelectedAgent(null)}>
        <DialogContent className="bg-zinc-900 border-zinc-800 max-w-2xl">
          <DialogHeader>
            <DialogTitle className="text-zinc-100">Edit Agent: {selectedAgent?.id}</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <Textarea
              value={configContent}
              onChange={(e) => setConfigContent(e.target.value)}
              className="h-[400px] font-mono bg-zinc-950 border-zinc-800 text-zinc-300"
            />
            <div className="flex justify-end gap-2">
              <Button variant="outline" onClick={() => setSelectedAgent(null)}>
                <X className="w-4 h-4 mr-1" />
                Cancel
              </Button>
              <Button onClick={saveAgentConfig} disabled={saving} className="bg-violet-600">
                <Check className="w-4 h-4 mr-1" />
                {saving ? "Saving..." : "Save"}
              </Button>
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  )
}
