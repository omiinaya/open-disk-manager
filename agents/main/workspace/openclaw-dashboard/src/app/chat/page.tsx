"use client"

import { useEffect, useState, useRef } from "react"
import { Card, CardContent } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { Textarea } from "@/components/ui/textarea"
import { ScrollArea } from "@/components/ui/scroll-area"
import { Avatar, AvatarFallback } from "@/components/ui/avatar"
import { Send, Bot, User, MoreVertical, ThumbsUp, ThumbsDown, Copy, RefreshCw } from "lucide-react"

interface Message {
  id: string
  role: 'user' | 'assistant'
  content: string
  timestamp: number
  thinking?: string
}

export default function ChatPage() {
  const [messages, setMessages] = useState<Message[]>([
    {
      id: '1',
      role: 'assistant',
      content: 'Hey! I\'m Ciel, your AI assistant. What can I help you with today?',
      timestamp: Date.now() - 60000,
    },
  ])
  const [input, setInput] = useState("")
  const [isLoading, setIsLoading] = useState(false)
  const [thinking, setThinking] = useState("")
  const scrollRef = useRef<HTMLDivElement>(null)

  const sendMessage = async () => {
    if (!input.trim() || isLoading) return

    const userMessage: Message = {
      id: Date.now().toString(),
      role: 'user',
      content: input.trim(),
      timestamp: Date.now(),
    }

    setMessages(prev => [...prev, userMessage])
    setInput("")
    setIsLoading(true)
    setThinking("Thinking...")

    // Simulate response - in production this would call the API
    setTimeout(() => {
      const assistantMessage: Message = {
        id: (Date.now() + 1).toString(),
        role: 'assistant',
        content: "This is a placeholder response. In production, this would connect to the OpenClaw gateway API to send messages and receive responses from the AI agent.",
        timestamp: Date.now(),
      }
      setMessages(prev => [...prev, assistantMessage])
      setIsLoading(false)
      setThinking("")
    }, 1500)
  }

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      sendMessage()
    }
  }

  const copyMessage = (content: string) => {
    navigator.clipboard.writeText(content)
  }

  return (
    <div className="h-[calc(100vh-48px)] flex flex-col">
      {/* Header */}
      <div className="p-4 border-b border-zinc-800 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <Avatar className="w-10 h-10 bg-gradient-to-br from-violet-500 to-fuchsia-500">
            <AvatarFallback className="text-white font-bold">C</AvatarFallback>
          </Avatar>
          <div>
            <h2 className="font-semibold text-zinc-100">Ciel</h2>
            <p className="text-xs text-zinc-500">GLM-5</p>
          </div>
        </div>
        <Button variant="ghost" size="icon">
          <MoreVertical className="w-5 h-5" />
        </Button>
      </div>

      {/* Messages */}
      <ScrollArea className="flex-1 p-4">
        <div className="space-y-6 max-w-3xl mx-auto">
          {messages.map((message) => (
            <div
              key={message.id}
              className={`flex gap-4 ${message.role === 'user' ? 'flex-row-reverse' : ''}`}
            >
              <Avatar className={`w-8 h-8 ${message.role === 'assistant' ? 'bg-gradient-to-br from-violet-500 to-fuchsia-500' : 'bg-zinc-700'}`}>
                <AvatarFallback className="text-white text-sm">
                  {message.role === 'assistant' ? 'C' : 'U'}
                </AvatarFallback>
              </Avatar>
              
              <div className={`flex-1 space-y-2 ${message.role === 'user' ? 'text-right' : ''}`}>
                <div className={`inline-block max-w-[80%] ${
                  message.role === 'user' 
                    ? 'bg-violet-600 text-white rounded-2xl rounded-br-md' 
                    : 'bg-zinc-800 text-zinc-100 rounded-2xl rounded-bl-md'
                } px-4 py-3`}>
                  <p className="whitespace-pre-wrap">{message.content}</p>
                </div>
                
                {message.role === 'assistant' && (
                  <div className="flex items-center gap-2">
                    <Button variant="ghost" size="icon" className="h-8 w-8" onClick={() => copyMessage(message.content)}>
                      <Copy className="w-4 h-4 text-zinc-500" />
                    </Button>
                    <Button variant="ghost" size="icon" className="h-8 w-8">
                      <ThumbsUp className="w-4 h-4 text-zinc-500" />
                    </Button>
                    <Button variant="ghost" size="icon" className="h-8 w-8">
                      <ThumbsDown className="w-4 h-4 text-zinc-500" />
                    </Button>
                  </div>
                )}
                
                <p className="text-xs text-zinc-600">
                  {new Date(message.timestamp).toLocaleTimeString()}
                </p>
              </div>
            </div>
          ))}

          {/* Thinking Indicator */}
          {thinking && (
            <div className="flex gap-4">
              <Avatar className="w-8 h-8 bg-gradient-to-br from-violet-500 to-fuchsia-500">
                <AvatarFallback className="text-white text-sm">C</AvatarFallback>
              </Avatar>
              <div className="bg-zinc-800 rounded-2xl rounded-bl-md px-4 py-3">
                <div className="flex items-center gap-2 text-zinc-400">
                  <RefreshCw className="w-4 h-4 animate-spin" />
                  <span className="text-sm">{thinking}</span>
                </div>
              </div>
            </div>
          )}
        </div>
      </ScrollArea>

      {/* Input */}
      <div className="p-4 border-t border-zinc-800">
        <div className="max-w-3xl mx-auto">
          <Card className="bg-zinc-900 border-zinc-800">
            <CardContent className="p-2">
              <Textarea
                value={input}
                onChange={(e) => setInput(e.target.value)}
                onKeyDown={handleKeyDown}
                placeholder="Message Ciel..."
                className="bg-transparent border-none resize-none text-zinc-100 placeholder:text-zinc-500 min-h-[44px] max-h-[200px]"
                disabled={isLoading}
              />
              <div className="flex items-center justify-between mt-2">
                <p className="text-xs text-zinc-600">
                  Press Enter to send, Shift+Enter for new line
                </p>
                <Button 
                  onClick={sendMessage}
                  disabled={!input.trim() || isLoading}
                  size="sm"
                  className="bg-violet-600 hover:bg-violet-700"
                >
                  <Send className="w-4 h-4 mr-1" />
                  Send
                </Button>
              </div>
            </CardContent>
          </Card>
        </div>
      </div>
    </div>
  )
}
