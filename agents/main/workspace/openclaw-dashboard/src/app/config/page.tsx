"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Textarea } from "@/components/ui/textarea"
import { ScrollArea } from "@/components/ui/scroll-area"
import { Settings, Save, RefreshCw, AlertTriangle, Check } from "lucide-react"
import { getConfig, patchConfig } from "@/lib/api"

export default function ConfigPage() {
  const [config, setConfig] = useState<any>(null)
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState(false)
  const [rawConfig, setRawConfig] = useState("")
  const [hasChanges, setHasChanges] = useState(false)

  useEffect(() => {
    loadConfig()
  }, [])

  const loadConfig = async () => {
    try {
      const data = await getConfig()
      setConfig(data)
      setRawConfig(JSON.stringify(data.parsed, null, 2))
    } catch (error) {
      console.error('Failed to load config:', error)
    } finally {
      setLoading(false)
    }
  }

  const handleConfigChange = (value: string) => {
    setRawConfig(value)
    try {
      JSON.parse(value)
      setHasChanges(true)
    } catch {
      setHasChanges(false)
    }
  }

  const saveConfig = async () => {
    setSaving(true)
    try {
      const parsed = JSON.parse(rawConfig)
      await patchConfig(parsed)
      setHasChanges(false)
      alert('Config saved successfully!')
    } catch (error) {
      console.error('Failed to save config:', error)
      alert('Failed to save config. Check for syntax errors.')
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
          <h2 className="text-2xl font-bold text-zinc-100">Configuration</h2>
          <p className="text-zinc-400">Edit config-extended.json directly</p>
        </div>
        <div className="flex items-center gap-2">
          {hasChanges && (
            <Badge variant="outline" className="bg-yellow-900/30 text-yellow-400 border-yellow-800">
              <AlertTriangle className="w-3 h-3 mr-1" />
              Unsaved Changes
            </Badge>
          )}
          <Button variant="outline" onClick={loadConfig}>
            <RefreshCw className="w-4 h-4 mr-2" />
            Reload
          </Button>
          <Button 
            onClick={saveConfig} 
            disabled={!hasChanges || saving}
            className="bg-violet-600 hover:bg-violet-700"
          >
            <Save className="w-4 h-4 mr-2" />
            {saving ? 'Saving...' : 'Save'}
          </Button>
        </div>
      </div>

      {/* Warning */}
      <Card className="bg-yellow-900/20 border-yellow-800">
        <CardContent className="p-4 flex items-start gap-3">
          <AlertTriangle className="w-5 h-5 text-yellow-500 mt-0.5" />
          <div>
            <p className="font-medium text-yellow-400">Warning</p>
            <p className="text-sm text-yellow-300/70">
              Direct config editing is experimental. Incorrect values may cause the gateway to fail. 
              Always backup your config before making changes.
            </p>
          </div>
        </CardContent>
      </Card>

      {/* Config Editor */}
      <Card className="bg-zinc-900 border-zinc-800">
        <CardHeader>
          <CardTitle className="text-zinc-100 flex items-center gap-2">
            <Settings className="w-5 h-5" />
            config-extended.json
          </CardTitle>
        </CardHeader>
        <CardContent>
          <Textarea
            value={rawConfig}
            onChange={(e) => handleConfigChange(e.target.value)}
            className="h-[600px] font-mono text-sm bg-zinc-950 border-zinc-800 text-zinc-300"
          />
        </CardContent>
      </Card>
    </div>
  )
}
