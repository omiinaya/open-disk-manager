"use client"

import { useEffect, useState } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Badge } from "@/components/ui/badge"
import { Calendar as CalendarIcon, ChevronLeft, ChevronRight, Clock, Bell } from "lucide-react"
import { getCronJobs } from "@/lib/api"
import { format, startOfMonth, endOfMonth, eachDayOfInterval, isSameDay, addMonths, subMonths } from "date-fns"

export default function CalendarPage() {
  const [cronJobs, setCronJobs] = useState<any[]>([])
  const [currentDate, setCurrentDate] = useState(new Date())
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    async function loadData() {
      try {
        const jobs = await getCronJobs()
        setCronJobs(jobs)
      } catch (error) {
        console.error('Failed to load cron jobs:', error)
      } finally {
        setLoading(false)
      }
    }
    loadData()
  }, [])

  const monthStart = startOfMonth(currentDate)
  const monthEnd = endOfMonth(currentDate)
  const days = eachDayOfInterval({ start: monthStart, end: monthEnd })

  const getJobsForDay = (day: Date) => {
    return cronJobs.filter(job => {
      if (!job.state?.nextRunAtMs) return false
      return isSameDay(new Date(job.state.nextRunAtMs), day)
    })
  }

  const nextMonth = () => setCurrentDate(addMonths(currentDate, 1))
  const prevMonth = () => setCurrentDate(subMonths(currentDate, 1))

  // Calculate padding days for the calendar grid
  const startPadding = monthStart.getDay()
  const endPadding = 6 - monthEnd.getDay()

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-zinc-100">Calendar</h2>
          <p className="text-zinc-400">View scheduled tasks, crons, and reminders</p>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Calendar */}
        <Card className="bg-zinc-900 border-zinc-800 lg:col-span-2">
          <CardHeader className="flex flex-row items-center justify-between pb-2">
            <CardTitle className="text-zinc-100">
              {format(currentDate, 'MMMM yyyy')}
            </CardTitle>
            <div className="flex gap-1">
              <Button variant="outline" size="icon" onClick={prevMonth}>
                <ChevronLeft className="w-4 h-4" />
              </Button>
              <Button variant="outline" size="icon" onClick={nextMonth}>
                <ChevronRight className="w-4 h-4" />
              </Button>
            </div>
          </CardHeader>
          <CardContent>
            {/* Day headers */}
            <div className="grid grid-cols-7 gap-1 mb-2">
              {['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'].map(day => (
                <div key={day} className="text-center text-xs text-zinc-500 py-2">
                  {day}
                </div>
              ))}
            </div>
            
            {/* Calendar grid */}
            <div className="grid grid-cols-7 gap-1">
              {/* Start padding */}
              {Array.from({ length: startPadding }).map((_, i) => (
                <div key={`pad-start-${i}`} className="h-20 bg-zinc-950/30 rounded" />
              ))}
              
              {/* Days */}
              {days.map(day => {
                const dayJobs = getJobsForDay(day)
                const isToday = isSameDay(day, new Date())
                
                return (
                  <div 
                    key={day.toISOString()} 
                    className={`h-20 p-1 rounded border ${
                      isToday 
                        ? 'bg-violet-900/30 border-violet-700' 
                        : 'bg-zinc-950/30 border-zinc-800'
                    }`}
                  >
                    <div className={`text-sm ${isToday ? 'text-violet-400 font-bold' : 'text-zinc-400'}`}>
                      {format(day, 'd')}
                    </div>
                    <div className="space-y-1 mt-1">
                      {dayJobs.slice(0, 2).map(job => (
                        <div 
                          key={job.id}
                          className="text-[10px] truncate px-1 py-0.5 rounded bg-green-900/50 text-green-300"
                        >
                          {job.name}
                        </div>
                      ))}
                      {dayJobs.length > 2 && (
                        <div className="text-[10px] text-zinc-500">
                          +{dayJobs.length - 2} more
                        </div>
                      )}
                    </div>
                  </div>
                )
              })}
              
              {/* End padding */}
              {Array.from({ length: endPadding }).map((_, i) => (
                <div key={`pad-end-${i}`} className="h-20 bg-zinc-950/30 rounded" />
              ))}
            </div>
          </CardContent>
        </Card>

        {/* Upcoming Events */}
        <Card className="bg-zinc-900 border-zinc-800">
          <CardHeader>
            <CardTitle className="text-zinc-100 flex items-center gap-2">
              <Clock className="w-5 h-5" />
              Upcoming
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-3">
            {cronJobs
              .filter(j => j.state?.nextRunAtMs)
              .sort((a, b) => a.state.nextRunAtMs - b.state.nextRunAtMs)
              .slice(0, 10)
              .map(job => (
                <div
                  key={job.id}
                  className="flex items-center justify-between p-3 rounded-lg bg-zinc-800/50"
                >
                  <div>
                    <p className="text-sm font-medium text-zinc-200">{job.name}</p>
                    <p className="text-xs text-zinc-500">
                      {job.schedule.expr || 'Custom schedule'}
                    </p>
                  </div>
                  <Badge variant="outline" className="bg-green-900/30 text-green-400">
                    {format(new Date(job.state.nextRunAtMs), 'MMM d, HH:mm')}
                  </Badge>
                </div>
              ))}
            
            {cronJobs.filter(j => j.state?.nextRunAtMs).length === 0 && (
              <p className="text-center text-zinc-500 py-4">No upcoming events</p>
            )}
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
