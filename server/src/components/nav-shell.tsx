"use client";

import { type ReactNode } from "react";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { DeviceProvider } from "@/lib/device-store";
import { LogProvider } from "@/lib/log-store";
import { DeviceConnector } from "./device-connector";
import { DeviceStatus } from "./device-status";
import { Home, Layers, FileAudio, BookOpen, ScrollText } from "lucide-react";

function GithubIcon({ size = 20 }: { size?: number }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill="currentColor">
      <path d="M12 .3a12 12 0 0 0-3.8 23.38c.6.11.82-.26.82-.58v-2.02c-3.34.73-4.04-1.61-4.04-1.61a3.18 3.18 0 0 0-1.33-1.76c-1.09-.74.08-.73.08-.73a2.52 2.52 0 0 1 1.84 1.24 2.56 2.56 0 0 0 3.5 1 2.56 2.56 0 0 1 .76-1.6c-2.67-.3-5.47-1.33-5.47-5.93a4.64 4.64 0 0 1 1.24-3.22 4.3 4.3 0 0 1 .12-3.18s1-.33 3.3 1.23a11.38 11.38 0 0 1 6 0c2.28-1.56 3.29-1.23 3.29-1.23a4.3 4.3 0 0 1 .12 3.18 4.64 4.64 0 0 1 1.24 3.22c0 4.61-2.81 5.63-5.48 5.93a2.86 2.86 0 0 1 .82 2.22v3.29c0 .32.21.7.82.58A12 12 0 0 0 12 .3" />
    </svg>
  );
}
import { cn } from "@/lib/utils";

const NAV_ITEMS = [
  { href: "/", label: "Home", icon: Home },
  { href: "/config", label: "Config", icon: Layers },
  { href: "/recordings", label: "Recordings", icon: FileAudio },
  { href: "/logs", label: "Logs", icon: ScrollText },
  { href: "/docs", label: "Docs", icon: BookOpen },
];

export function NavShell({ children }: { children: ReactNode }) {
  const pathname = usePathname();
  return (
    <DeviceProvider>
    <LogProvider>
      {/* Top Header */}
      <header className="sticky top-0 z-40 border-b-2 border-border bg-surface/95 backdrop-blur">
        <div className="mx-auto flex max-w-7xl items-center gap-4 px-4 py-3">
          {/* Desktop Nav */}
          <nav className="hidden items-center gap-1 md:flex">
            {NAV_ITEMS.map((item) => {
              const active =
                item.href === "/"
                  ? pathname === "/"
                  : pathname.startsWith(item.href);
              return (
                <Link
                  key={item.href}
                  href={item.href}
                  className={cn(
                    "flex items-center gap-1.5 rounded-lg px-3 py-2 text-sm font-bold uppercase tracking-wider transition-colors",
                    active
                      ? "bg-primary/10 text-primary"
                      : "text-text-muted hover:text-text",
                  )}
                >
                  <item.icon size={16} />
                  {item.label}
                </Link>
              );
            })}
          </nav>

          {/* Right side */}
          <div className="ml-auto flex items-center gap-3">
            <DeviceStatus />
            <DeviceConnector />
            <a
              href="https://github.com/mikeRead/walkey-talkey"
              target="_blank"
              rel="noopener noreferrer"
              className="btn btn-sm btn-ghost"
              aria-label="GitHub"
            >
              <GithubIcon size={20} />
            </a>
          </div>
        </div>

      </header>

      {/* Page Content */}
      <main className="mx-auto max-w-7xl px-4 py-6">{children}</main>

    </LogProvider>
    </DeviceProvider>
  );
}
