"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Input } from "@/components/ui/input"
import { ScrollArea } from "@/components/ui/scroll-area"
import { Layers, Search, FileText, Database, RefreshCw } from "lucide-react"

interface KnowledgeItem {
  id: string
  name: string
  type: 'memory' | 'document' | 'config' | 'session'
  size: string
  updatedAt: number
}

export default function KnowledgePage() {
  const [items, setItems] = useState<KnowledgeItem[]>([
    { id: '1', name: 'SOUL.md', type: 'config', size: '3.5KB', updatedAt: Date.now() - 3600000 },
    { id: '2', name: 'USER.md', type: 'config', size: '116B', updatedAt: Date.now() - 86400000 },
    { id: '3', name: 'MEMORY.md', type: 'memory', size: '14KB', updatedAt: Date.now() - 1800000 },
    { id: '4', name: '2026-02-14.md', type: 'memory', size: '2.1KB', updatedAt: Date.now() - 600000 },
    { id: '5', name: 'AGENTS.md', type: 'config', size: '356B', updatedAt: Date.now() - 172800000 },
  ])
  const [searchQuery, setSearchQuery] = useState("")
  const [selectedType, setSelectedType] = useState<string>("all")

  const filteredItems = items.filter(item => {
    const matchesSearch = item.name.toLowerCase().includes(searchQuery.toLowerCase())
    const matchesType = selectedType === "all" || item.type === selectedType
    return matchesSearch && matchesType
  })

  const typeColors = {
    memory: 'bg-blue-900/50 text-blue-400 border-blue-800',
    document: 'bg-green-900/50 text-green-400 border-green-800',
    config: 'bg-violet-900/50 text-violet-400 border-violet-800',
    session: 'bg-yellow-900/50 text-yellow-400 border-yellow-800',
  }

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-zinc-100">Knowledge Base</h2>
          <p className="text-zinc-400">Visualize and manage what we store in memory</p>
        </div>
        <Button variant="outline">
          <RefreshCw className="w-4 h-4 mr-2" />
          Refresh
        </Button>
      </div>

      {/* Search and Filter */}
      <div className="flex gap-4">
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-zinc-500" />
          <Input
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search knowledge base..."
            className="pl-10 bg-zinc-900 border-zinc-800"
          />
        </div>
        <select
          value={selectedType}
          onChange={(e) => setSelectedType(e.target.value)}
          className="bg-zinc-900 border border-zinc-800 rounded-lg px-3 py-2 text-zinc-100"
        >
          <option value="all">All Types</option>
          <option value="memory">Memory</option>
          <option value="config">Config</option>
          <option value="document">Document</option>
          <option value="session">Session</option>
        </select>
      </div>

      {/* Stats */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <Database className="w-5 h-5 text-violet-500" />
              <div>
                <p className="text-2xl font-bold text-zinc-100">{items.length}</p>
                <p className="text-sm text-zinc-500">Total Items</p>
              </div>
            </div>
          </CardContent>
        </Card>
        
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <div className="w-5 h-5 rounded bg-blue-900/50 flex items-center justify-center">
                <span className="text-xs text-blue-400">M</span>
              </div>
              <div>
                <p className="text-2xl font-bold text-zinc-100">
                  {items.filter(i => i.type === 'memory').length}
                </p>
                <p className="text-sm text-zinc-500">Memory Files</p>
              </div>
            </div>
          </CardContent>
        </Card>
        
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <div className="w-5 h-5 rounded bg-violet-900/50 flex items-center justify-center">
                <span className="text-xs text-violet-400">C</span>
              </div>
              <div>
                <p className="text-2xl font-bold text-zinc-100">
                  {items.filter(i => i.type === 'config').length}
                </p>
                <p className="text-sm text-zinc-500">Config Files</p>
              </div>
            </div>
          </CardContent>
        </Card>
        
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4">
            <div className="flex items-center gap-3">
              <Layers className="w-5 h-5 text-green-500" />
              <div>
                <p className="text-2xl font-bold text-zinc-100">~20KB</p>
                <p className="text-sm text-zinc-500">Total Size</p>
              </div>
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Items List */}
      <Card className="bg-zinc-900 border-zinc-800">
        <CardHeader>
          <CardTitle className="text-zinc-100">Stored Knowledge</CardTitle>
        </CardHeader>
        <CardContent>
          <ScrollArea className="h-[400px]">
            <div className="space-y-2">
              {filteredItems.map((item) => (
                <div
                  key={item.id}
                  className="flex items-center justify-between p-3 rounded-lg bg-zinc-800/50 hover:bg-zinc-800 transition-colors"
                >
                  <div className="flex items-center gap-3">
                    <FileText className="w-5 h-5 text-zinc-500" />
                    <div>
                      <p className="font-medium text-zinc-200">{item.name}</p>
                      <p className="text-xs text-zinc-500">
                        Updated {new Date(item.updatedAt).toLocaleString()}
                      </p>
                    </div>
                  </div>
                  <div className="flex items-center gap-3">
                    <Badge variant="outline" className={typeColors[item.type]}>
                      {item.type}
                    </Badge>
                    <span className="text-sm text-zinc-500 font-mono">{item.size}</span>
                  </div>
                </div>
              ))}
            </div>
          </ScrollArea>
        </CardContent>
      </Card>
    </div>
  )
}
