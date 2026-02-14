"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Input } from "@/components/ui/input"
import { Textarea } from "@/components/ui/textarea"
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from "@/components/ui/dialog"
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select"
import { BookOpen, Search, Plus, Edit, Trash2, FileText, Folder } from "lucide-react"

interface Skill {
  name: string
  description: string
  location: string
}

export default function SkillsPage() {
  const [skills, setSkills] = useState<Skill[]>([
    { name: 'agent-lock', description: 'File-based locking for coordinated operations', location: '/root/.openclaw/agents/main/workspace/skills/agent-lock' },
    { name: 'health-check', description: 'Periodic health check for heartbeat', location: '/root/.openclaw/agents/main/workspace/skills/health-check' },
    { name: 'morning-briefing', description: 'Generate morning status report', location: '/root/.openclaw/agents/main/workspace/skills/morning-briefing' },
    { name: 'qmd', description: 'Local hybrid search for markdown notes', location: '/opt/qmd' },
    { name: 'save-config', description: 'Save configuration by committing changes', location: '/root/.openclaw/agents/main/workspace/skills/save-config' },
  ])
  const [searchQuery, setSearchQuery] = useState("")
  const [selectedSkill, setSelectedSkill] = useState<Skill | null>(null)
  const [editContent, setEditContent] = useState("")

  const filteredSkills = skills.filter(skill => 
    skill.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    skill.description.toLowerCase().includes(searchQuery.toLowerCase())
  )

  const openSkillEditor = (skill: Skill) => {
    setSelectedSkill(skill)
    // In production, would read the actual file
    setEditContent(`# ${skill.name}\n\n${skill.description}\n\nSkill location: ${skill.location}`)
  }

  const deleteSkill = (name: string) => {
    if (confirm(`Are you sure you want to delete "${name}"?`)) {
      setSkills(skills.filter(s => s.name !== name))
    }
  }

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-zinc-100">Skills</h2>
          <p className="text-zinc-400">Browse, search, edit, and manage your OpenClaw skills</p>
        </div>
        <Button className="bg-violet-600 hover:bg-violet-700">
          <Plus className="w-4 h-4 mr-2" />
          Add Skill
        </Button>
      </div>

      {/* Search */}
      <div className="relative">
        <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-zinc-500" />
        <Input
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          placeholder="Search skills..."
          className="pl-10 bg-zinc-900 border-zinc-800"
        />
      </div>

      {/* Skills Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {filteredSkills.map((skill) => (
          <Card key={skill.name} className="bg-zinc-900 border-zinc-800 hover:border-zinc-700 transition-colors">
            <CardHeader className="pb-3">
              <CardTitle className="text-zinc-100 flex items-center gap-2">
                <BookOpen className="w-5 h-5 text-violet-500" />
                {skill.name}
              </CardTitle>
            </CardHeader>
            <CardContent className="space-y-4">
              <p className="text-sm text-zinc-400">{skill.description}</p>
              
              <div className="flex items-center gap-2 text-xs text-zinc-500">
                <Folder className="w-3 h-3" />
                <span className="truncate font-mono">{skill.location}</span>
              </div>

              <div className="flex gap-2">
                <Button 
                  variant="outline" 
                  size="sm" 
                  className="flex-1"
                  onClick={() => openSkillEditor(skill)}
                >
                  <Edit className="w-3 h-3 mr-1" />
                  Edit
                </Button>
                <Button 
                  variant="outline" 
                  size="sm" 
                  className="flex-1"
                >
                  <FileText className="w-3 h-3 mr-1" />
                  View
                </Button>
                <Button 
                  variant="ghost" 
                  size="icon"
                  onClick={() => deleteSkill(skill.name)}
                >
                  <Trash2 className="w-4 h-4 text-red-400" />
                </Button>
              </div>
            </CardContent>
          </Card>
        ))}
      </div>

      {filteredSkills.length === 0 && (
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-8 text-center text-zinc-500">
            No skills found matching your search
          </CardContent>
        </Card>
      )}

      {/* Edit Dialog */}
      <Dialog open={!!selectedSkill} onOpenChange={() => setSelectedSkill(null)}>
        <DialogContent className="bg-zinc-900 border-zinc-800 max-w-2xl">
          <DialogHeader>
            <DialogTitle className="text-zinc-100">Edit Skill: {selectedSkill?.name}</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <Textarea
              value={editContent}
              onChange={(e) => setEditContent(e.target.value)}
              className="h-[400px] font-mono bg-zinc-950 border-zinc-800 text-zinc-300"
            />
            <div className="flex justify-between">
              <Button 
                variant="destructive" 
                onClick={() => selectedSkill && deleteSkill(selectedSkill.name)}
              >
                <Trash2 className="w-4 h-4 mr-2" />
                Delete Skill
              </Button>
              <Button className="bg-violet-600">Save Changes</Button>
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  )
}
