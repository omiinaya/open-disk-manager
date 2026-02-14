"use client"

import * as React from "react"
import { cn } from "@/lib/utils"
import { LayoutDashboard, Bot, Clock, Target, ListTodo, Activity, MessageSquare, BookOpen, Settings, Calendar, BarChart3, Key, Layers } from "lucide-react"
import { usePathname } from "next/navigation"

const navItems = [
  { href: "/", label: "Overview", icon: LayoutDashboard },
  { href: "/agents", label: "Agents", icon: Bot },
  { href: "/crons", label: "Cron Jobs", icon: Clock },
  { href: "/goals", label: "Goals", icon: Target },
  { href: "/tasks", label: "Tasks", icon: ListTodo },
  { href: "/activity", label: "Activity", icon: Activity },
  { href: "/chat", label: "Chat", icon: MessageSquare },
  { href: "/skills", label: "Skills", icon: BookOpen },
  { href: "/knowledge", label: "Knowledge", icon: Layers },
  { href: "/config", label: "Config", icon: Settings },
  { href: "/calendar", label: "Calendar", icon: Calendar },
  { href: "/analytics", label: "Analytics", icon: BarChart3 },
  { href: "/providers", label: "Providers", icon: Key },
  { href: "/models", label: "Models", icon: Layers },
]

interface SidebarProps extends React.HTMLAttributes<HTMLDivElement> {}

export function DashboardSidebar({ className, children }: SidebarProps) {
  const pathname = usePathname()

  return (
    <div className={cn("flex h-screen bg-zinc-950 text-zinc-100", className)}>
      {/* Sidebar */}
      <aside className="w-64 border-r border-zinc-800 flex flex-col">
        <div className="p-4 border-b border-zinc-800">
          <h1 className="text-xl font-bold bg-gradient-to-r from-violet-500 to-fuchsia-500 bg-clip-text text-transparent">
            OpenClaw
          </h1>
          <p className="text-xs text-zinc-500 mt-1">Control Center</p>
        </div>
        
        <nav className="flex-1 overflow-y-auto p-2">
          {navItems.map((item) => {
            const isActive = pathname === item.href || (item.href !== '/' && pathname.startsWith(item.href))
            return (
              <a
                key={item.href}
                href={item.href}
                className={cn(
                  "flex items-center gap-3 px-3 py-2 rounded-lg text-sm font-medium transition-colors",
                  isActive 
                    ? "bg-zinc-800 text-zinc-100" 
                    : "text-zinc-400 hover:text-zinc-100 hover:bg-zinc-900"
                )}
              >
                <item.icon className="w-4 h-4" />
                {item.label}
              </a>
            )
          })}
        </nav>

        <div className="p-4 border-t border-zinc-800">
          <div className="flex items-center gap-2 text-xs text-zinc-500">
            <div className="w-2 h-2 rounded-full bg-green-500 animate-pulse" />
            Gateway Online
          </div>
        </div>
      </aside>

      {/* Main Content */}
      <main className="flex-1 overflow-y-auto">
        {children}
      </main>
    </div>
  )
}
