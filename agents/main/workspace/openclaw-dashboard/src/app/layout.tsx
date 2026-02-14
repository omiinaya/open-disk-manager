import type { Metadata } from "next"
import { Inter } from "next/font/google"
import "./globals.css"
import { DashboardSidebar } from "@/components/dashboard-sidebar"
import { TooltipProvider } from "@/components/ui/tooltip"

const inter = Inter({ subsets: ["latin"] })

export const metadata: Metadata = {
  title: "OpenClaw Dashboard",
  description: "Professional dashboard for OpenClaw AI Assistant",
}

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode
}>) {
  return (
    <html lang="en" className="dark">
      <body className={inter.className}>
        <TooltipProvider>
          <DashboardSidebar>
            {children}
          </DashboardSidebar>
        </TooltipProvider>
      </body>
    </html>
  )
}
