"use client";

import {
  createContext,
  useContext,
  useState,
  useCallback,
  useEffect,
  useRef,
  useMemo,
  type ReactNode,
} from "react";
import { api, setApiLogCallback } from "./api";
import { useDevice } from "./device-store";
import type { LogEntry } from "@/types/config";

const MAX_LOGS = 200;

interface LogState {
  logs: LogEntry[];
  fetchDeviceLogs: () => Promise<void>;
  addWebLog: (type: string, message: string) => void;
}

const LogContext = createContext<LogState | null>(null);

export function LogProvider({ children }: { children: ReactNode }) {
  const { deviceUrl, connected } = useDevice();
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const deviceLogCountRef = useRef(0);

  const fetchDeviceLogs = useCallback(async () => {
    if (!connected) return;
    try {
      const raw = await api.getLogs(deviceUrl);
      const sorted = [...raw].sort((a, b) => a.runtime - b.runtime);
      let skip = deviceLogCountRef.current;
      if (sorted.length < skip) skip = 0;
      const newOnes = sorted.slice(skip);
      deviceLogCountRef.current = sorted.length;
      if (newOnes.length === 0) return;
      const now = Math.round(performance.now());
      const entries: LogEntry[] = newOnes.map((e, i) => ({
        ...e,
        source: "device" as const,
        runtime: now + i,
        receivedAt: Date.now(),
      }));
      setLogs((prev) => [...prev, ...entries].slice(-MAX_LOGS));
    } catch {
      // device unreachable -- keep stale logs
    }
  }, [deviceUrl, connected]);

  const addWebLog = useCallback((type: string, message: string) => {
    const entry: LogEntry = {
      type,
      message,
      runtime: Math.round(performance.now()),
      source: "web",
      receivedAt: Date.now(),
    };
    setLogs((prev) => [...prev, entry].slice(-MAX_LOGS));
  }, []);

  useEffect(() => {
    setApiLogCallback((method, path, status, ok, ms) => {
      if (path === "/api/logs" && ok) return;
      const tag = ok ? "API" : "API_ERR";
      addWebLog(tag, `${method} ${path} → ${status} (${ms}ms)`);
    });
    return () => setApiLogCallback(null);
  }, [addWebLog]);

  const ctxValue = useMemo(
    () => ({ logs, fetchDeviceLogs, addWebLog }),
    [logs, fetchDeviceLogs, addWebLog],
  );

  return (
    <LogContext.Provider value={ctxValue}>
      {children}
    </LogContext.Provider>
  );
}

export function useLogs() {
  const ctx = useContext(LogContext);
  if (!ctx) throw new Error("useLogs must be used within LogProvider");
  return ctx;
}
