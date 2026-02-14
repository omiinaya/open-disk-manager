"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Input } from "@/components/ui/input"
import { Textarea } from "@/components/ui/textarea"
import { Label } from "@/components/ui/label"
import { Dialog, DialogContent, DialogHeader, DialogTitle } from "@/components/ui/dialog"
import { Key, Plus, Edit, Trash2, Check, X, ExternalLink } from "lucide-react"
import { getConfig, patchConfig } from "@/lib/api"

interface Provider {
  baseUrl: string
  apiKey: string
  auth: string
  api: string
  models: any[]
}

export default function ProvidersPage() {
  const [config, setConfig] = useState<any>(null)
  const [providers, setProviders] = useState<Record<string, Provider>>({})
  const [loading, setLoading] = useState(true)
  const [showAddDialog, setShowAddDialog] = useState(false)
  const [editingProvider, setEditingProvider] = useState<string | null>(null)
  const [newProvider, setNewProvider] = useState({
    name: '',
    baseUrl: '',
    apiKey: '',
    auth: 'api-key',
    api: 'openai-completions',
  })

  useEffect(() => {
    loadConfig()
  }, [])

  const loadConfig = async () => {
    try {
      const data = await getConfig()
      setConfig(data)
      setProviders(data.parsed?.models?.providers || {})
    } catch (error) {
      console.error('Failed to load config:', error)
    } finally {
      setLoading(false)
    }
  }

  const addProvider = async () => {
    if (!newProvider.name || !newProvider.baseUrl) return
    
    const updatedProviders: Record<string, Provider> = {
      ...providers,
      [newProvider.name]: {
        baseUrl: newProvider.baseUrl,
        apiKey: newProvider.apiKey.startsWith('${') ? newProvider.apiKey : `\${${newProvider.name.toUpperCase()}_API_KEY}`,
        auth: newProvider.auth,
        api: newProvider.api,
        models: [],
      }
    }

    try {
      await patchConfig({
        models: { providers: updatedProviders }
      })
      setProviders(updatedProviders)
      setNewProvider({ name: '', baseUrl: '', apiKey: '', auth: 'api-key', api: 'openai-completions' })
      setShowAddDialog(false)
    } catch (error) {
      console.error('Failed to add provider:', error)
    }
  }

  const deleteProvider = async (name: string) => {
    if (!confirm(`Delete provider "${name}"?`)) return
    const rest: Record<string, Provider> = { ...providers }
    delete rest[name]
    try {
      await patchConfig({ models: { providers: rest } })
      setProviders(rest)
    } catch (error) {
      console.error('Failed to delete provider:', error)
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
          <h2 className="text-2xl font-bold text-zinc-100">Providers</h2>
          <p className="text-zinc-400">Configure AI providers with endpoints and API keys</p>
        </div>
        <Button onClick={() => setShowAddDialog(true)} className="bg-violet-600 hover:bg-violet-700">
          <Plus className="w-4 h-4 mr-2" />
          Add Provider
        </Button>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {Object.entries(providers).map(([name, provider]) => (
          <Card key={name} className="bg-zinc-900 border-zinc-800">
            <CardHeader className="pb-3">
              <div className="flex items-center justify-between">
                <CardTitle className="text-zinc-100 flex items-center gap-2">
                  <Key className="w-5 h-5 text-violet-500" />
                  {name}
                </CardTitle>
                <div className="flex gap-1">
                  <Button variant="ghost" size="icon" onClick={() => deleteProvider(name)}>
                    <Trash2 className="w-4 h-4 text-red-400" />
                  </Button>
                </div>
              </div>
            </CardHeader>
            <CardContent className="space-y-3">
              <div>
                <Label className="text-zinc-500 text-xs">Base URL</Label>
                <p className="text-sm text-zinc-300 font-mono truncate">{provider.baseUrl}</p>
              </div>
              <div>
                <Label className="text-zinc-500 text-xs">Auth Method</Label>
                <p className="text-sm text-zinc-300">{provider.auth}</p>
              </div>
              <div>
                <Label className="text-zinc-500 text-xs">Models</Label>
                <div className="flex flex-wrap gap-1 mt-1">
                  {provider.models?.length > 0 ? (
                    provider.models.map((model: any) => (
                      <Badge key={model.id} variant="secondary" className="bg-zinc-800">
                        {model.id.split('/').pop()}
                      </Badge>
                    ))
                  ) : (
                    <span className="text-sm text-zinc-500">No models configured</span>
                  )}
                </div>
              </div>
            </CardContent>
          </Card>
        ))}
      </div>

      {Object.keys(providers).length === 0 && (
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-8 text-center text-zinc-500">
            No providers configured. Add one to get started.
          </CardContent>
        </Card>
      )}

      {/* Add Provider Dialog */}
      <Dialog open={showAddDialog} onOpenChange={setShowAddDialog}>
        <DialogContent className="bg-zinc-900 border-zinc-800">
          <DialogHeader>
            <DialogTitle className="text-zinc-100">Add Provider</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <div>
              <Label className="text-zinc-400">Name</Label>
              <Input
                value={newProvider.name}
                onChange={(e) => setNewProvider({...newProvider, name: e.target.value})}
                placeholder="e.g., openai"
                className="bg-zinc-950 border-zinc-800"
              />
            </div>
            <div>
              <Label className="text-zinc-400">Base URL</Label>
              <Input
                value={newProvider.baseUrl}
                onChange={(e) => setNewProvider({...newProvider, baseUrl: e.target.value})}
                placeholder="https://api.example.com/v1"
                className="bg-zinc-950 border-zinc-800"
              />
            </div>
            <div>
              <Label className="text-zinc-400">API Key (optional)</Label>
              <Input
                type="password"
                value={newProvider.apiKey}
                onChange={(e) => setNewProvider({...newProvider, apiKey: e.target.value})}
                placeholder="Will use env var if empty"
                className="bg-zinc-950 border-zinc-800"
              />
            </div>
            <div className="flex gap-2">
              <Button onClick={() => setShowAddDialog(false)} variant="outline">Cancel</Button>
              <Button onClick={addProvider} className="bg-violet-600">Add Provider</Button>
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  )
}
