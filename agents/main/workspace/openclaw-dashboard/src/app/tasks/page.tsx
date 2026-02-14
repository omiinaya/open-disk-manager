"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Textarea } from "@/components/ui/textarea"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select"
import { Dialog, DialogContent, DialogHeader, DialogTitle } from "@/components/ui/dialog"
import { ListTodo, Plus, ArrowRight, User, Bot, Trash2, Edit } from "lucide-react"
import { getAgents } from "@/lib/api"

type TaskStage = 'todo' | 'in_progress' | 'review' | 'done'

interface Task {
  id: string
  title: string
  description: string
  stage: TaskStage
  assignee?: string
  createdAt: number
}

const stages: TaskStage[] = ['todo', 'in_progress', 'review', 'done']
const stageLabels: Record<TaskStage, string> = {
  todo: 'To Do',
  in_progress: 'In Progress', 
  review: 'Review',
  done: 'Done',
}
const stageColors: Record<TaskStage, string> = {
  todo: 'bg-zinc-700 text-zinc-300',
  in_progress: 'bg-blue-900/50 text-blue-400 border-blue-800',
  review: 'bg-yellow-900/50 text-yellow-400 border-yellow-800',
  done: 'bg-green-900/50 text-green-400 border-green-800',
}

export default function TasksPage() {
  const [tasks, setTasks] = useState<Task[]>([
    { id: '1', title: 'Set up dashboard routing', description: 'Create navigation and routing structure', stage: 'done', assignee: 'ciel', createdAt: Date.now() - 86400000 },
    { id: '2', title: 'Add shadcn components', description: 'Install and configure all needed UI components', stage: 'in_progress', assignee: 'ciel', createdAt: Date.now() - 43200000 },
    { id: '3', title: 'Create API client', description: 'Build API integration with OpenClaw gateway', stage: 'review', createdAt: Date.now() - 21600000 },
    { id: '4', title: 'Build agents section', description: 'Visualize and edit agent configurations', stage: 'todo', createdAt: Date.now() },
  ])
  const [agents, setAgents] = useState<any[]>([])
  const [showAddDialog, setShowAddDialog] = useState(false)
  const [newTask, setNewTask] = useState({ title: '', description: '', assignee: '' })

  useEffect(() => {
    getAgents().then(setAgents).catch(console.error)
  }, [])

  const addTask = () => {
    if (!newTask.title.trim()) return
    const task: Task = {
      id: Date.now().toString(),
      title: newTask.title,
      description: newTask.description,
      stage: 'todo',
      assignee: newTask.assignee || undefined,
      createdAt: Date.now(),
    }
    setTasks([task, ...tasks])
    setNewTask({ title: '', description: '', assignee: '' })
    setShowAddDialog(false)
  }

  const moveTask = (taskId: string, direction: 'forward' | 'backward') => {
    setTasks(tasks.map(t => {
      if (t.id !== taskId) return t
      const currentIndex = stages.indexOf(t.stage)
      let newIndex = direction === 'forward' ? currentIndex + 1 : currentIndex - 1
      newIndex = Math.max(0, Math.min(stages.length - 1, newIndex))
      return { ...t, stage: stages[newIndex] }
    }))
  }

  const deleteTask = (id: string) => {
    setTasks(tasks.filter(t => t.id !== id))
  }

  const assignTask = (taskId: string, agentId: string) => {
    setTasks(tasks.map(t => 
      t.id === taskId ? { ...t, assignee: agentId } : t
    ))
  }

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-zinc-100">Tasks</h2>
          <p className="text-zinc-400">Track tasks and assign agents to accomplish them</p>
        </div>
        <Button onClick={() => setShowAddDialog(true)} className="bg-violet-600 hover:bg-violet-700">
          <Plus className="w-4 h-4 mr-2" />
          Add Task
        </Button>
      </div>

      {/* Kanban Board */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        {stages.map((stage) => {
          const stageTasks = tasks.filter(t => t.stage === stage)
          return (
            <div key={stage} className="space-y-3">
              <div className="flex items-center justify-between">
                <h3 className="font-medium text-zinc-300 flex items-center gap-2">
                  <span className={`w-2 h-2 rounded-full ${stageColors[stage].split(' ')[0].replace('bg-', 'bg-').replace('/50', '')}`} />
                  {stageLabels[stage]}
                </h3>
                <Badge variant="secondary" className="bg-zinc-800">{stageTasks.length}</Badge>
              </div>
              
              <div className="space-y-2 min-h-[200px] p-2 rounded-lg bg-zinc-900/50 border border-zinc-800">
                {stageTasks.map((task) => (
                  <Card key={task.id} className="bg-zinc-800 border-zinc-700">
                    <CardContent className="p-3">
                      <div className="flex items-start justify-between mb-2">
                        <h4 className="font-medium text-zinc-100 text-sm">{task.title}</h4>
                        <div className="flex gap-1">
                          <Button 
                            variant="ghost" 
                            size="icon" 
                            className="h-6 w-6"
                            onClick={() => moveTask(task.id, 'backward')}
                            disabled={stage === 'todo'}
                          >
                            <ArrowRight className="w-3 h-3 rotate-180" />
                          </Button>
                          <Button 
                            variant="ghost" 
                            size="icon"
                            className="h-6 w-6" 
                            onClick={() => moveTask(task.id, 'forward')}
                            disabled={stage === 'done'}
                          >
                            <ArrowRight className="w-3 h-3" />
                          </Button>
                        </div>
                      </div>
                      
                      {task.description && (
                        <p className="text-xs text-zinc-400 mb-2">{task.description}</p>
                      )}
                      
                      <div className="flex items-center justify-between">
                        {task.assignee ? (
                          <Badge variant="outline" className="text-xs bg-violet-900/30 border-violet-800 text-violet-400">
                            <Bot className="w-3 h-3 mr-1" />
                            {task.assignee}
                          </Badge>
                        ) : (
                          <Select onValueChange={(v) => assignTask(task.id, v)}>
                            <SelectTrigger className="h-6 text-xs bg-zinc-900">
                              <SelectValue placeholder="Assign..." />
                            </SelectTrigger>
                            <SelectContent>
                              {agents.map((agent) => (
                                <SelectItem key={agent.id} value={agent.id}>
                                  {agent.id}
                                </SelectItem>
                              ))}
                            </SelectContent>
                          </Select>
                        )}
                        
                        <Button 
                          variant="ghost" 
                          size="icon" 
                          className="h-6 w-6"
                          onClick={() => deleteTask(task.id)}
                        >
                          <Trash2 className="w-3 h-3 text-red-400" />
                        </Button>
                      </div>
                    </CardContent>
                  </Card>
                ))}
              </div>
            </div>
          )
        })}
      </div>

      {/* Add Task Dialog */}
      <Dialog open={showAddDialog} onOpenChange={setShowAddDialog}>
        <DialogContent className="bg-zinc-900 border-zinc-800">
          <DialogHeader>
            <DialogTitle className="text-zinc-100">Add New Task</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <div>
              <Label className="text-zinc-400">Title</Label>
              <Input 
                value={newTask.title} 
                onChange={(e) => setNewTask({...newTask, title: e.target.value})}
                className="bg-zinc-950 border-zinc-800"
              />
            </div>
            <div>
              <Label className="text-zinc-400">Description</Label>
              <Textarea 
                value={newTask.description} 
                onChange={(e) => setNewTask({...newTask, description: e.target.value})}
                className="bg-zinc-950 border-zinc-800"
              />
            </div>
            <div>
              <Label className="text-zinc-400">Assign to Agent (optional)</Label>
              <Select onValueChange={(v) => setNewTask({...newTask, assignee: v})}>
                <SelectTrigger className="bg-zinc-950 border-zinc-800">
                  <SelectValue placeholder="Select agent..." />
                </SelectTrigger>
                <SelectContent>
                  {agents.map((agent) => (
                    <SelectItem key={agent.id} value={agent.id}>
                      {agent.id}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>
            <Button onClick={addTask} className="w-full bg-violet-600">Add Task</Button>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  )
}
