"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Textarea } from "@/components/ui/textarea"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Target, Plus, Check, Trash2, Lightbulb } from "lucide-react"

interface Goal {
  id: string
  title: string
  description: string
  status: 'active' | 'completed' | 'archived'
  progress: number
  createdAt: number
}

export default function GoalsPage() {
  const [goals, setGoals] = useState<Goal[]>([
    { id: '1', title: 'Build amazing dashboard', description: 'Create a professional dashboard with shadcn components', status: 'active', progress: 35, createdAt: Date.now() - 86400000 },
    { id: '2', title: 'Improve agent performance', description: 'Optimize agent response times and reduce costs', status: 'active', progress: 60, createdAt: Date.now() - 172800000 },
  ])
  const [newGoalTitle, setNewGoalTitle] = useState("")
  const [newGoalDesc, setNewGoalDesc] = useState("")
  const [showAdd, setShowAdd] = useState(false)

  const addGoal = () => {
    if (!newGoalTitle.trim()) return
    const goal: Goal = {
      id: Date.now().toString(),
      title: newGoalTitle,
      description: newGoalDesc,
      status: 'active',
      progress: 0,
      createdAt: Date.now(),
    }
    setGoals([goal, ...goals])
    setNewGoalTitle("")
    setNewGoalDesc("")
    setShowAdd(false)
  }

  const toggleComplete = (id: string) => {
    setGoals(goals.map(g => 
      g.id === id ? { ...g, status: g.status === 'completed' ? 'active' : 'completed' } : g
    ))
  }

  const deleteGoal = (id: string) => {
    setGoals(goals.filter(g => g.id !== id))
  }

  const activeGoals = goals.filter(g => g.status === 'active')
  const completedGoals = goals.filter(g => g.status === 'completed')

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-zinc-100">Goals</h2>
          <p className="text-zinc-400">Set goals and I'll help you achieve them</p>
        </div>
        <Button onClick={() => setShowAdd(!showAdd)} className="bg-violet-600 hover:bg-violet-700">
          <Plus className="w-4 h-4 mr-2" />
          Add Goal
        </Button>
      </div>

      {showAdd && (
        <Card className="bg-zinc-900 border-zinc-800">
          <CardContent className="p-4 space-y-4">
            <div>
              <Label className="text-zinc-400">Goal Title</Label>
              <Input 
                value={newGoalTitle} 
                onChange={(e) => setNewGoalTitle(e.target.value)}
                placeholder="What do you want to achieve?"
                className="bg-zinc-950 border-zinc-800 text-zinc-100"
              />
            </div>
            <div>
              <Label className="text-zinc-400">Description</Label>
              <Textarea 
                value={newGoalDesc} 
                onChange={(e) => setNewGoalDesc(e.target.value)}
                placeholder="More details about this goal..."
                className="bg-zinc-950 border-zinc-800 text-zinc-100"
              />
            </div>
            <div className="flex gap-2">
              <Button onClick={addGoal} className="bg-violet-600">Save Goal</Button>
              <Button variant="outline" onClick={() => setShowAdd(false)}>Cancel</Button>
            </div>
          </CardContent>
        </Card>
      )}

      {/* Active Goals */}
      <div className="space-y-4">
        <h3 className="text-lg font-semibold text-zinc-200 flex items-center gap-2">
          <Target className="w-5 h-5 text-violet-500" />
          Active Goals ({activeGoals.length})
        </h3>
        
        {activeGoals.length === 0 ? (
          <Card className="bg-zinc-900/50 border-zinc-800">
            <CardContent className="p-8 text-center text-zinc-500">
              No active goals. Add one to get started!
            </CardContent>
          </Card>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {activeGoals.map((goal) => (
              <Card key={goal.id} className="bg-zinc-900 border-zinc-800">
                <CardContent className="p-4">
                  <div className="flex items-start justify-between mb-3">
                    <div className="flex-1">
                      <h4 className="font-medium text-zinc-100">{goal.title}</h4>
                      {goal.description && (
                        <p className="text-sm text-zinc-400 mt-1">{goal.description}</p>
                      )}
                    </div>
                    <Button 
                      variant="ghost" 
                      size="icon"
                      onClick={() => toggleComplete(goal.id)}
                    >
                      <Check className="w-4 h-4 text-green-500" />
                    </Button>
                  </div>
                  <div className="space-y-2">
                    <div className="flex justify-between text-xs text-zinc-500">
                      <span>Progress</span>
                      <span>{goal.progress}%</span>
                    </div>
                    <div className="h-2 bg-zinc-800 rounded-full overflow-hidden">
                      <div 
                        className="h-full bg-gradient-to-r from-violet-500 to-fuchsia-500 transition-all"
                        style={{ width: `${goal.progress}%` }}
                      />
                    </div>
                  </div>
                  <div className="flex justify-end mt-3">
                    <Button variant="ghost" size="sm" onClick={() => deleteGoal(goal.id)}>
                      <Trash2 className="w-3 h-3" />
                    </Button>
                  </div>
                </CardContent>
              </Card>
            ))}
          </div>
        )}
      </div>

      {/* AI Suggestions */}
      <Card className="bg-zinc-900 border-zinc-800">
        <CardHeader>
          <CardTitle className="text-zinc-100 flex items-center gap-2">
            <Lightbulb className="w-5 h-5 text-yellow-500" />
            AI Suggestions
          </CardTitle>
        </CardHeader>
        <CardContent>
          <p className="text-zinc-400 text-sm">
            I can help you break down complex goals into actionable tasks. 
            I can also track progress and remind you of pending goals.
          </p>
          <Button variant="outline" className="mt-4">
            Ask me for help with a goal
          </Button>
        </CardContent>
      </Card>

      {/* Completed Goals */}
      {completedGoals.length > 0 && (
        <div className="space-y-4">
          <h3 className="text-lg font-semibold text-zinc-200">
            Completed ({completedGoals.length})
          </h3>
          <div className="space-y-2">
            {completedGoals.map((goal) => (
              <div
                key={goal.id}
                className="flex items-center justify-between p-3 rounded-lg bg-zinc-900/50 border border-zinc-800"
              >
                <div className="flex items-center gap-3">
                  <Check className="w-4 h-4 text-green-500" />
                  <span className="text-zinc-400 line-through">{goal.title}</span>
                </div>
                <Button variant="ghost" size="sm" onClick={() => deleteGoal(goal.id)}>
                  <Trash2 className="w-3 h-3" />
                </Button>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  )
}
