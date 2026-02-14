"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Switch } from "@/components/ui/switch"
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from "@/components/ui/dialog"
import { Textarea } from "@/components/ui/textarea"
import { Label } from "@/components/ui/label"
import { Input } from "@/components/ui/input"
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select"
import { Clock, Play, Pause, Edit, Trash2, AlertCircle, CheckCircle2, Plus } from "lucide-react"
import { getCronJobs, runCronJob, getAgents } from "@/lib/api"
import { format } from "date-fns"

export default function CronsPage() {
  const [cronJobs, setCronJobs] = useState<any[]>([])
  const [agents, setAgents] = useState<any[]>([])
  const [loading, setLoading] = useState(true)
  const [selectedJob, setSelectedJob] = useState<any>(null)
  const [editMode, setEditMode] = useState(false)
  const [editContent, setEditContent] = useState("")

  useEffect(() => {
    async function loadData() {
      try {
        const [jobsData, agentsData] = await Promise.all([
          getCronJobs(),
          getAgents(),
        ])
        setCronJobs(jobsData)
        setAgents(agentsData)
      } catch (error) {
        console.error('Failed to load data:', error)
      } finally {
        setLoading(false)
      }
    }
    loadData()
  }, [])

  const runNow = async (jobId: string) => {
    try {
      await runCronJob(jobId)
      alert('Job triggered!')
    } catch (error) {
      console.error('Failed to run job:', error)
    }
  }

  const toggleEnabled = async (job: any) => {
    // Would need an API to toggle - for now just UI
    setCronJobs(cronJobs.map(j => 
      j.id === job.id ? { ...j, enabled: !j.enabled } : j
    ))
  }

  const openEditor = (job: any, edit: boolean = false) => {
    setSelectedJob(job)
    setEditMode(edit)
    if (edit) {
      setEditContent(JSON.stringify({
        name: job.name,
        enabled: job.enabled,
        schedule: job.schedule,
        payload: job.payload,
        agentId: job.agentId,
      }, null, 2))
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
          <h2 className="text-2xl font-bold text-zinc-100">Cron Jobs</h2>
          <p className="text-zinc-400">Manage scheduled tasks and automation</p>
        </div>
        <Button className="bg-violet-600 hover:bg-violet-700">
          <Plus className="w-4 h-4 mr-2" />
          New Job
        </Button>
      </div>

      <div className="space-y-4">
        {cronJobs.map((job) => (
          <Card key={job.id} className="bg-zinc-900 border-zinc-800">
            <CardContent className="p-4">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-4">
                  <div className="flex items-center gap-2">
                    {job.state?.lastStatus === 'error' ? (
                      <AlertCircle className="w-5 h-5 text-red-500" />
                    ) : job.state?.lastStatus === 'ok' ? (
                      <CheckCircle2 className="w-5 h-5 text-green-500" />
                    ) : (
                      <Clock className="w-5 h-5 text-zinc-500" />
                    )}
                  </div>
                  <div>
                    <h3 className="font-medium text-zinc-100">{job.name}</h3>
                    <div className="flex items-center gap-2 mt-1">
                      <Badge variant="secondary" className="bg-zinc-800">
                        {job.agentId}
                      </Badge>
                      <span className="text-xs text-zinc-500">
                        {job.schedule.expr || `Every ${job.schedule.everyMs}ms`}
                        {job.schedule.tz && ` (${job.schedule.tz})`}
                      </span>
                    </div>
                  </div>
                </div>

                <div className="flex items-center gap-4">
                  {job.state && (
                    <div className="text-right text-sm">
                      <p className="text-zinc-400">
                        Next: {job.state.nextRunAtMs ? format(new Date(job.state.nextRunAtMs), 'MMM d, HH:mm') : 'N/A'}
                      </p>
                      {job.state.lastDurationMs && (
                        <p className="text-xs text-zinc-500">
                          Last: {job.state.lastDurationMs}ms
                        </p>
                      )}
                    </div>
                  )}

                  <div className="flex items-center gap-2">
                    <Switch 
                      checked={job.enabled} 
                      onCheckedChange={() => toggleEnabled(job)}
                    />
                    <Button 
                      variant="ghost" 
                      size="icon"
                      onClick={() => runNow(job.id)}
                    >
                      <Play className="w-4 h-4 text-green-500" />
                    </Button>
                    <Button 
                      variant="ghost" 
                      size="icon"
                      onClick={() => openEditor(job, true)}
                    >
                      <Edit className="w-4 h-4" />
                    </Button>
                  </div>
                </div>
              </div>

              {job.state?.lastError && (
                <div className="mt-3 p-2 bg-red-900/20 border border-red-800 rounded text-sm text-red-400">
                  Error: {job.state.lastError}
                </div>
              )}
            </CardContent>
          </Card>
        ))}
      </div>

      {/* Edit Dialog */}
      <Dialog open={editMode} onOpenChange={setEditMode}>
        <DialogContent className="bg-zinc-900 border-zinc-800 max-w-2xl">
          <DialogHeader>
            <DialogTitle className="text-zinc-100">Edit Cron Job</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <Textarea
              value={editContent}
              onChange={(e) => setEditContent(e.target.value)}
              className="h-[400px] font-mono bg-zinc-950 border-zinc-800 text-zinc-300"
            />
            <div className="flex justify-end">
              <Button className="bg-violet-600">Save Changes</Button>
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  )
}
