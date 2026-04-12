"use client";

import { useEffect, useState, useRef } from "react";
import { useLogs } from "@/lib/log-store";
import { useDevice } from "@/lib/device-store";
import {
  ScrollText,
  RefreshCw,
  ToggleLeft,
  ToggleRight,
  Cpu,
  Globe,
  WifiOff,
} from "lucide-react";
import { cn } from "@/lib/utils";

const TYPE_COLORS: Record<string, string> = {
  BOOT: "bg-primary/20 text-primary",
  INFO: "bg-blue-500/20 text-blue-400",
  WARN: "bg-highlight/20 text-highlight",
  ERROR: "bg-red-500/20 text-red-400",
  CONFIG: "bg-accent/20 text-accent",
  ACTION: "bg-secondary/20 text-secondary",
  WHISPER: "bg-purple-500/20 text-purple-400",
};

function typeBadge(type: string) {
  const color = TYPE_COLORS[type] ?? "bg-surface-raised text-text-muted";
  return (
    <span
      className={cn(
        "inline-block w-20 rounded-md px-2 py-0.5 text-center text-xs font-bold uppercase tracking-wider",
        color,
      )}
    >
      {type}
    </span>
  );
}

function formatRuntime(ms: number) {
  if (ms < 1000) return `${ms}ms`;
  return `${(ms / 1000).toFixed(2)}s`;
}

export default function LogsPage() {
  const { connected } = useDevice();
  const { logs, fetchDeviceLogs } = useLogs();
  const [autoRefresh, setAutoRefresh] = useState(false);
  const [refreshing, setRefreshing] = useState(false);
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (connected) {
      fetchDeviceLogs();
    }
  }, [connected, fetchDeviceLogs]);

  useEffect(() => {
    if (!autoRefresh || !connected) return;
    const id = setInterval(() => {
      fetchDeviceLogs();
    }, 5000);
    return () => clearInterval(id);
  }, [autoRefresh, connected, fetchDeviceLogs]);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [logs.length]);

  const handleRefresh = async () => {
    setRefreshing(true);
    await fetchDeviceLogs();
    setRefreshing(false);
  };

  return (
    <div>
      <div className="mb-6 rounded-xl border border-dashed border-border p-6 backdrop-blur-[6px]">
        <h1 className="text-2xl font-extrabold sm:text-3xl">
          <span className="text-primary">Device</span>{" "}
          <span className="text-accent">Logs</span>
        </h1>
        <p className="mt-1 text-sm text-text-muted">
          Real-time logs from the device and local web events
        </p>
      </div>

      {/* Controls */}
      <div className="mb-4 flex flex-wrap items-center gap-3">
        <button
          className="btn btn-sm btn-primary"
          onClick={handleRefresh}
          disabled={!connected || refreshing}
        >
          <RefreshCw
            size={14}
            className={cn("mr-1", refreshing && "animate-spin")}
          />
          Refresh
        </button>

        <button
          className="btn btn-sm btn-ghost"
          onClick={() => setAutoRefresh(!autoRefresh)}
        >
          {autoRefresh ? (
            <ToggleRight size={16} className="mr-1 text-accent" />
          ) : (
            <ToggleLeft size={16} className="mr-1" />
          )}
          Auto-refresh {autoRefresh ? "ON" : "OFF"}
        </button>

        <span className="ml-auto text-xs text-text-muted">
          {logs.length} log{logs.length !== 1 ? "s" : ""}
        </span>
      </div>

      {!connected ? (
        <div className="flex flex-col items-center rounded-xl border-2 border-dashed border-border bg-surface/80 px-6 py-16 text-center">
          <WifiOff size={48} className="mb-4 text-text-muted" />
          <h2 className="mb-2 text-xl font-extrabold">No Device Connected</h2>
          <p className="text-sm text-text-muted">
            Connect to your device to view logs.
          </p>
        </div>
      ) : logs.length === 0 ? (
        <div className="flex flex-col items-center rounded-xl border-2 border-dashed border-border bg-surface/80 px-6 py-16 text-center">
          <ScrollText size={48} className="mb-4 text-text-muted" />
          <h2 className="mb-2 text-xl font-extrabold">No Logs Yet</h2>
          <p className="text-sm text-text-muted">
            Logs will appear here as the device operates.
          </p>
        </div>
      ) : (
        <div className="overflow-hidden rounded-xl border-2 border-border bg-black/90 backdrop-blur-md">
          {/* Header */}
          <div className="flex items-center gap-2 border-b border-white/10 px-4 py-2">
            <div className="h-3 w-3 rounded-full bg-accent" />
            <div className="h-3 w-3 rounded-full bg-highlight" />
            <div className="h-3 w-3 rounded-full bg-primary" />
            <span className="ml-2 font-mono text-xs text-white/50">
              walkey-talkey.local &mdash; logs
            </span>
            {autoRefresh && (
              <span className="ml-auto flex items-center gap-1 text-xs text-accent">
                <span className="h-2 w-2 animate-pulse rounded-full bg-accent" />
                Live
              </span>
            )}
          </div>

          {/* Log entries */}
          <div className="max-h-[70vh] overflow-y-auto p-4 font-mono text-xs leading-relaxed">
            {logs.map((log, i) => (
              <div key={i} className="flex items-start gap-3 py-1">
                <span className="w-20 shrink-0 text-right text-white/30">
                  [{formatRuntime(log.runtime)}]
                </span>
                {typeBadge(log.type)}
                <span className="flex-1 text-white/80">{log.message}</span>
                <span className="shrink-0">
                  {log.source === "device" ? (
                    <Cpu size={12} className="text-primary/50" title="Device" />
                  ) : (
                    <Globe
                      size={12}
                      className="text-accent/50"
                      title="Web"
                    />
                  )}
                </span>
              </div>
            ))}
            <div ref={bottomRef} />
          </div>
        </div>
      )}
    </div>
  );
}
