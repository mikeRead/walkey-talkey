"use client";

import {
  createContext,
  useContext,
  useState,
  useCallback,
  type ReactNode,
} from "react";
import { api } from "./api";
import { useDevice } from "./device-store";
import type { LogEntry } from "@/types/config";

const MAX_WEB_LOGS = 50;

interface LogState {
  deviceLogs: LogEntry[];
  webLogs: LogEntry[];
  allLogs: LogEntry[];
  fetchDeviceLogs: () => Promise<void>;
  addWebLog: (type: string, message: string) => void;
}

const LogContext = createContext<LogState | null>(null);

export function LogProvider({ children }: { children: ReactNode }) {
  const { deviceUrl, connected } = useDevice();
  const [deviceLogs, setDeviceLogs] = useState<LogEntry[]>([]);
  const [webLogs, setWebLogs] = useState<LogEntry[]>([]);

  const fetchDeviceLogs = useCallback(async () => {
    if (!connected) return;
    try {
      const raw = await api.getLogs(deviceUrl);
      setDeviceLogs(
        raw.map((e) => ({ ...e, source: "device" as const })),
      );
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
    };
    setWebLogs((prev) => [...prev, entry].slice(-MAX_WEB_LOGS));
  }, []);

  const allLogs = [...deviceLogs, ...webLogs];

  return (
    <LogContext.Provider
      value={{ deviceLogs, webLogs, allLogs, fetchDeviceLogs, addWebLog }}
    >
      {children}
    </LogContext.Provider>
  );
}

export function useLogs() {
  const ctx = useContext(LogContext);
  if (!ctx) throw new Error("useLogs must be used within LogProvider");
  return ctx;
}
