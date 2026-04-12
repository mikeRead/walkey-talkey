"use client";

import {
  createContext,
  useContext,
  useState,
  useEffect,
  useCallback,
  type ReactNode,
} from "react";
import { api } from "./api";

interface DeviceState {
  deviceUrl: string;
  setDeviceUrl: (url: string) => void;
  connected: boolean;
  connecting: boolean;
  testConnection: () => Promise<boolean>;
}

const DeviceContext = createContext<DeviceState | null>(null);

const DEFAULT_URL = "http://walkey-talkey.local";
const STORAGE_KEY = "walkey-device-url";

export function DeviceProvider({ children }: { children: ReactNode }) {
  const [deviceUrl, setDeviceUrlState] = useState(DEFAULT_URL);
  const [connected, setConnected] = useState(false);
  const [connecting, setConnecting] = useState(false);

  useEffect(() => {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored) setDeviceUrlState(stored);
  }, []);

  const setDeviceUrl = useCallback((url: string) => {
    const clean = url.replace(/\/+$/, "");
    setDeviceUrlState(clean);
    localStorage.setItem(STORAGE_KEY, clean);
    setConnected(false);
  }, []);

  const testConnection = useCallback(async () => {
    setConnecting(true);
    try {
      await api.ping(deviceUrl);
      setConnected(true);
      return true;
    } catch {
      setConnected(false);
      return false;
    } finally {
      setConnecting(false);
    }
  }, [deviceUrl]);

  useEffect(() => {
    testConnection();
  }, [testConnection]);

  return (
    <DeviceContext.Provider
      value={{ deviceUrl, setDeviceUrl, connected, connecting, testConnection }}
    >
      {children}
    </DeviceContext.Provider>
  );
}

export function useDevice() {
  const ctx = useContext(DeviceContext);
  if (!ctx) throw new Error("useDevice must be used within DeviceProvider");
  return ctx;
}
