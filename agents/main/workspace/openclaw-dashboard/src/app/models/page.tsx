"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Input } from "@/components/ui/input"
import { Switch } from "@/components/ui/switch"
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table"
import { Layers, RefreshCw, Check, Brain, Image, MessageSquare } from "lucide-react"
import { getConfig, patchConfig } from "@/lib/api"

interface Model {
  id: string
  name: string
  reasoning: boolean
  input: string[]
  cost: { input: number; output: number }
  contextWindow: number
  maxTokens: number
  enabled?: boolean
}

export default function ModelsPage() {
  const [config, setConfig] = useState<any>(null)
  const [providers, setProviders] = useState<Record<string, any>>({})
  const [selectedProvider, setSelectedProvider] = useState<string>("")
  const [models, setModels] = useState<Model[]>([])
  const [loading, setLoading] = useState(true)
  const [fetching, setFetching] = useState(false)
  const [searchQuery, setSearchQuery] = useState("")

  useEffect(() => {
    loadConfig()
  }, [])

  useEffect(() => {
    if (selectedProvider && providers[selectedProvider]) {
      const providerModels = providers[selectedProvider].models || []
      setModels(providerModels)
    }
  }, [selectedProvider, providers])

  const loadConfig = async () => {
    try {
      const data = await getConfig()
      setConfig(data)
      setProviders(data.parsed?.models?.providers || {})
      const firstProvider = Object.keys(data.parsed?.models?.providers || {})[0]
      if (firstProvider) {
        setSelectedProvider(firstProvider)
      }
    } catch (error) {
      console.error('Failed to load config:', error)
    } finally {
      setLoading(false)
    }
  }

  const fetchModels = async () => {
    if (!selectedProvider) return
    setFetching(true)
    try {
      // In production, this would fetch from the provider's API
      // For now, we'll use the existing models
      alert('In production, this fetches models from the provider API')
    } catch (error) {
      console.error('Failed to fetch models:', error)
    } finally {
      setFetching(false)
    }
  }

  const toggleModel = async (modelId: string) => {
    if (!selectedProvider) return
    
    const provider = providers[selectedProvider]
    const updatedModels = provider.models.map((m: Model) =>
      m.id === modelId ? { ...m, enabled: !m.enabled } : m
    )

    const updatedProviders = {
      ...providers,
      [selectedProvider]: {
        ...provider,
        models: updatedModels
      }
    }

    try {
      await patchConfig({ models: { providers: updatedProviders } })
      setProviders(updatedProviders)
      setModels(updatedModels)
    } catch (error) {
      console.error('Failed to toggle model:', error)
    }
  }

  const filteredModels = models.filter(m =>
    m.id.toLowerCase().includes(searchQuery.toLowerCase()) ||
    m.name.toLowerCase().includes(searchQuery.toLowerCase())
  )

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
          <h2 className="text-2xl font-bold text-zinc-100">Models</h2>
          <p className="text-zinc-400">Configure and enable/disable models from your providers</p>
        </div>
      </div>

      {/* Provider Selection */}
      <div className="flex gap-4 items-center">
        <select
          value={selectedProvider}
          onChange={(e) => setSelectedProvider(e.target.value)}
          className="bg-zinc-900 border border-zinc-800 rounded-lg px-3 py-2 text-zinc-100"
        >
          <option value="">Select Provider</option>
          {Object.keys(providers).map(name => (
            <option key={name} value={name}>{name}</option>
          ))}
        </select>
        <Button onClick={fetchModels} disabled={fetching || !selectedProvider}>
          <RefreshCw className={`w-4 h-4 mr-2 ${fetching ? 'animate-spin' : ''}`} />
          Fetch Models
        </Button>
      </div>

      {/* Search */}
      {selectedProvider && (
        <Input
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          placeholder="Search models..."
          className="bg-zinc-900 border-zinc-800"
        />
      )}

      {/* Models Table */}
      {selectedProvider && (
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-0">
            <Table>
              <TableHeader>
                <TableRow className="border-zinc-800">
                  <TableHead className="text-zinc-400">Enabled</TableHead>
                  <TableHead className="text-zinc-400">Model</TableHead>
                  <TableHead className="text-zinc-400">Features</TableHead>
                  <TableHead className="text-zinc-400">Context</TableHead>
                  <TableHead className="text-zinc-400">Max Tokens</TableHead>
                  <TableHead className="text-zinc-400">Cost</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {filteredModels.map((model) => (
                  <TableRow key={model.id} className="border-zinc-800">
                    <TableCell>
                      <Switch
                        checked={model.enabled !== false}
                        onCheckedChange={() => toggleModel(model.id)}
                      />
                    </TableCell>
                    <TableCell>
                      <div>
                        <p className="font-medium text-zinc-200">{model.name || model.id}</p>
                        <p className="text-xs text-zinc-500 font-mono">{model.id}</p>
                      </div>
                    </TableCell>
                    <TableCell>
                      <div className="flex gap-2">
                        {model.reasoning && (
                          <Badge variant="outline" className="bg-purple-900/30 text-purple-400 border-purple-800">
                            <Brain className="w-3 h-3 mr-1" />
                            Reasoning
                          </Badge>
                        )}
                        {model.input?.includes('image') && (
                          <Badge variant="outline" className="bg-green-900/30 text-green-400 border-green-800">
                            <Image className="w-3 h-3 mr-1" />
                            Vision
                          </Badge>
                        )}
                      </div>
                    </TableCell>
                    <TableCell className="text-zinc-300">
                      {model.contextWindow?.toLocaleString()}
                    </TableCell>
                    <TableCell className="text-zinc-300">
                      {model.maxTokens?.toLocaleString()}
                    </TableCell>
                    <TableCell>
                      <div className="text-xs text-zinc-500">
                        ${model.cost?.input}/1K in
                      </div>
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>

            {filteredModels.length === 0 && (
              <div className="p-8 text-center text-zinc-500">
                No models found
              </div>
            )}
          </CardContent>
        </Card>
      )}

      {!selectedProvider && (
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-8 text-center text-zinc-500">
            Select a provider to view its models
          </CardContent>
        </Card>
      )}
    </div>
  )
}
