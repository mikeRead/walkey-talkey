"use client";

import Image from "next/image";
import Link from "next/link";
import { useDevice } from "@/lib/device-store";
import { api } from "@/lib/api";
import { useState, useEffect, useRef, useMemo, useCallback } from "react";
import type { ModeSummary, WifiConfig } from "@/types/config";
import { useLogs } from "@/lib/log-store";
import { useTranscription } from "@/lib/transcription-store";
import { formatBytes } from "@/lib/utils";
import type { LogEntry } from "@/types/config";
import {
  Wifi,
  WifiOff,
  Zap,
  Mic,
  MicOff,
  Loader2,
  ChevronRight,
  Gamepad2,
  X,
} from "lucide-react";

interface DeviceInfo {
  modes: ModeSummary[];
  activeMode: string | null;
  wifi: WifiConfig | null;
}

const TILT_MAX = 15;

function ParallaxLogo() {
  const [style, setStyle] = useState({ transform: "perspective(600px) rotateX(0deg) rotateY(0deg) scale(1.25)" });

  useEffect(() => {
    const onMove = (e: MouseEvent) => {
      const x = (e.clientX / window.innerWidth) - 0.5;
      const y = (e.clientY / window.innerHeight) - 0.5;
      setStyle({
        transform: `perspective(600px) rotateX(${(-y * TILT_MAX).toFixed(1)}deg) rotateY(${(x * TILT_MAX).toFixed(1)}deg) scale(1.25)`,
      });
    };
    window.addEventListener("mousemove", onMove);
    return () => window.removeEventListener("mousemove", onMove);
  }, []);

  return (
    <div className="flex items-center justify-center">
      <Image
        src="/logo-transparent.png"
        alt="WalKEY-TalKEY"
        width={400}
        height={400}
        className="pointer-events-none w-full max-w-lg transition-transform duration-150 ease-out will-change-transform"
        style={style}
      />
    </div>
  );
}

export default function HomePage() {
  const { deviceUrl, connected, connecting } = useDevice();
  const [info, setInfo] = useState<DeviceInfo>({
    modes: [],
    activeMode: null,
    wifi: null,
  });

  useEffect(() => {
    if (!connected) return;
    Promise.allSettled([
      api.getModes(deviceUrl),
      api.getActiveMode(deviceUrl),
      api.getWifi(deviceUrl),
    ]).then(([modesR, activeR, wifiR]) => {
      setInfo({
        modes: modesR.status === "fulfilled" ? modesR.value : [],
        activeMode: activeR.status === "fulfilled" ? activeR.value : null,
        wifi: wifiR.status === "fulfilled" ? wifiR.value : null,
      });
    });
  }, [deviceUrl, connected]);

  const activeModeLabel =
    info.modes.find((m) => m.id === info.activeMode)?.label ?? info.activeMode;

  return (
    <div className="space-y-6">
      {/* Hero Banner – logo + console */}
      <div className="grid gap-4 lg:grid-cols-2">
        {/* Logo – defines the row height */}
        <ParallaxLogo />

        {/* Console */}
        <DeviceConsole />
      </div>

      {/* Quick-action Cards */}
      <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
        <QuickCard
          href="/config"
          image="/config.png"
          imageScale={1.25}
          imagePosition="55% center"
          color="primary"
          title="Configuration"
          description="Edit modes, bindings, and device settings"
        />
        <QuickCard
          href="/recordings"
          image="/rec.png"
          color="accent"
          title="Recordings"
          description="Manage audio files & transcribe with Whisper"
        />
        <QuickCard
          href="/docs"
          image="/docs.png"
          imageScale={1.05}
          imagePosition="60% center"
          color="secondary"
          title="Documentation"
          description="User guide, technical docs & more"
        />
      </div>

      {/* Device Overview (only when connected) */}
      {connected && (
        <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {/* Modes */}
          <div className="card space-y-3 hover:border-primary">
            <div className="flex items-center gap-2">
              <Gamepad2 size={18} className="text-primary" />
              <h2 className="section-title text-primary">Modes</h2>
              <span className="badge badge-primary ml-auto">{info.modes.length}</span>
            </div>
            <div className="space-y-1.5">
              {info.modes.slice(0, 5).map((m) => (
                <div
                  key={m.id}
                  className={`flex items-center justify-between rounded-lg px-3 py-2 text-sm ${
                    m.id === info.activeMode
                      ? "bg-primary/10 border border-primary/30 font-bold text-primary"
                      : "bg-surface-raised text-text-muted"
                  }`}
                >
                  <span className="flex items-center gap-2">
                    {m.id === info.activeMode && (
                      <Zap size={12} className="text-highlight" />
                    )}
                    {m.label}
                  </span>
                  <span className="text-xs opacity-60">
                    {m.bindingCount} binding{m.bindingCount !== 1 ? "s" : ""}
                  </span>
                </div>
              ))}
              {info.modes.length > 5 && (
                <p className="text-center text-xs text-text-muted">
                  +{info.modes.length - 5} more
                </p>
              )}
            </div>
            <Link
              href="/config"
              className="btn btn-sm btn-ghost w-full justify-between"
            >
              Manage Modes <ChevronRight size={14} />
            </Link>
          </div>

          {/* Recording */}
          <RecordingCard />

          {/* Wi-Fi */}
          <div className="card space-y-3 hover:border-highlight">
            <div className="flex items-center gap-2">
              <Wifi size={18} className="text-highlight" />
              <h2 className="section-title text-highlight">Network</h2>
            </div>
            {info.wifi ? (
              <div className="space-y-2 text-sm">
                <InfoRow label="Hostname" value={info.wifi.hostname} />
                <InfoRow label="URL" value={info.wifi.localUrl} />
                <InfoRow label="STA SSID" value={info.wifi.sta.ssid || "—"} />
                <InfoRow label="AP SSID" value={info.wifi.ap.ssid || "—"} />
              </div>
            ) : (
              <p className="text-sm text-text-muted">Loading...</p>
            )}
          </div>
        </div>
      )}

      {/* Not connected prompt */}
      {!connected && !connecting && (
        <div className="memphis-bg flex flex-col items-center rounded-xl border-2 border-dashed border-border backdrop-blur-[6px] px-6 py-12 text-center">
          <WifiOff size={48} className="mb-4 text-text-muted" />
          <h2 className="mb-2 text-xl font-extrabold">No Device Connected</h2>
          <p className="max-w-md text-sm text-text-muted">
            Enter your WalKEY-TalKEY device URL in the header to get started.
            The default is{" "}
            <code className="rounded bg-surface-raised px-1.5 py-0.5 text-primary">
              http://walkey-talkey.local
            </code>
          </p>
        </div>
      )}
    </div>
  );
}

function RecordingCard() {
  const { recordings, recordingSettings, getTranscription, pendingCount } = useTranscription();
  const latest = recordings.length > 0 ? recordings[recordings.length - 1] : null;
  const latestState = latest ? getTranscription(latest.path) : undefined;

  return (
    <div className="card space-y-3 hover:border-accent">
      <div className="flex items-center gap-2">
        {recordingSettings?.enabled ? (
          <Mic size={18} className="text-accent" />
        ) : (
          <MicOff size={18} className="text-text-muted" />
        )}
        <h2 className="section-title text-accent">Recording</h2>
        {pendingCount > 0 && (
          <span className="ml-auto flex h-[18px] min-w-[18px] items-center justify-center rounded-full bg-accent px-1 text-[10px] font-bold text-white animate-pulse">
            {pendingCount}
          </span>
        )}
      </div>
      <div className="flex items-center gap-3">
        <div
          className={`h-3 w-3 rounded-full ${
            recordingSettings?.enabled
              ? "bg-accent animate-pulse"
              : "bg-text-muted/30"
          }`}
        />
        <span className="text-sm font-bold">
          {recordingSettings?.enabled ? "Active" : "Disabled"}
        </span>
      </div>
      {recordingSettings?.format && (
        <p className="text-xs text-text-muted">
          Format: {recordingSettings.format}
        </p>
      )}

      {latest && (
        <div className="space-y-1.5 rounded-lg bg-surface-raised p-2.5">
          <div className="flex items-center justify-between gap-2">
            <span className="truncate text-xs font-bold text-text">{latest.path}</span>
            <span className="shrink-0 text-[10px] text-text-muted">{formatBytes(latest.size)}</span>
          </div>
          {latestState?.status === "complete" && latestState.transcript && (
            <p className="line-clamp-2 text-xs text-text-muted">
              {latestState.transcript}
            </p>
          )}
          {latestState && latestState.status !== "complete" && latestState.status !== "error" && (
            <div className="flex items-center gap-1.5 text-xs text-accent">
              <Loader2 size={10} className="animate-spin" />
              <span>{latestState.progressMsg || latestState.status}</span>
            </div>
          )}
          {latestState?.status === "error" && (
            <p className="text-xs text-danger">{latestState.error}</p>
          )}
        </div>
      )}

      <Link
        href="/recordings"
        className="btn btn-sm btn-ghost w-full justify-between"
      >
        View Recordings <ChevronRight size={14} />
      </Link>
    </div>
  );
}

function QuickCard({
  href,
  icon,
  image,
  imageScale = 1.2,
  imagePosition = "center",
  color,
  title,
  description,
}: {
  href: string;
  icon?: React.ReactNode;
  image?: string;
  imageScale?: number;
  imagePosition?: string;
  color: "primary" | "accent" | "secondary";
  title: string;
  description: string;
}) {
  const colorMap = {
    primary:
      "text-primary bg-primary/20 group-hover:bg-primary/30 group-hover:shadow-primary/10",
    accent:
      "text-accent bg-accent/20 group-hover:bg-accent/30 group-hover:shadow-accent/10",
    secondary:
      "text-secondary bg-secondary/20 group-hover:bg-secondary/30 group-hover:shadow-secondary/10",
  };
  const borderMap = {
    primary: "hover:border-primary hover:shadow-lg hover:shadow-primary/10",
    accent: "hover:border-accent hover:shadow-lg hover:shadow-accent/10",
    secondary:
      "hover:border-secondary hover:shadow-lg hover:shadow-secondary/10",
  };

  if (image) {
    return (
      <Link
        href={href}
        className={`card memphis-bg backdrop-blur-[6px] group flex items-center gap-4 transition-all ${borderMap[color]}`}
      >
        <div className="h-36 w-36 shrink-0 overflow-hidden rounded-lg">
          <Image
            src={image}
            alt={title}
            width={200}
            height={200}
            className="h-full w-full object-cover"
            style={{ transform: `scale(${imageScale})`, objectPosition: imagePosition }}
          />
        </div>
        <div className="min-w-0 flex-1">
          <h2 className="text-lg font-extrabold">{title}</h2>
          <p className="mt-0.5 text-sm text-text-muted">{description}</p>
        </div>
        <ChevronRight
          size={18}
          className="ml-auto shrink-0 text-text-muted opacity-0 transition-opacity group-hover:opacity-100"
        />
      </Link>
    );
  }

  return (
    <Link
      href={href}
      className={`card group flex items-start gap-4 transition-all ${borderMap[color]}`}
    >
      <div
        className={`flex h-14 w-14 shrink-0 items-center justify-center rounded-xl transition-colors ${colorMap[color]}`}
      >
        {icon}
      </div>
      <div>
        <h2 className="text-lg font-extrabold">{title}</h2>
        <p className="mt-0.5 text-sm text-text-muted">{description}</p>
      </div>
      <ChevronRight
        size={18}
        className="ml-auto mt-1 shrink-0 text-text-muted opacity-0 transition-opacity group-hover:opacity-100"
      />
    </Link>
  );
}

const FALLBACK_CONSOLE_LINES = [
  { color: "text-white/30", text: "[0.000] ---- WalKEY-TalKEY ----" },
  { color: "text-white/30", text: "[0.012] Initializing NVS flash..." },
  { color: "text-primary", text: "[0.045] Wi-Fi STA connecting..." },
  { color: "text-primary", text: "[1.203] Wi-Fi connected" },
  { color: "text-white/30", text: "[1.215] HTTP server started on port 80" },
  { color: "text-highlight", text: "[1.220] Loading config..." },
  { color: "text-white/30", text: "[1.234] Loaded modes" },
  { color: "text-secondary", text: "[1.240] Active mode: Cursor" },
  { color: "text-accent", text: "[1.260] Recording: enabled" },
  { color: "text-white/30", text: "[1.265] SD card mounted" },
  { color: "text-primary", text: "[2.001] USB HID device connected" },
  { color: "text-white/30", text: "[2.500] Boot complete" },
];

const LOG_TYPE_COLORS: Record<string, string> = {
  BOOT: "text-primary",
  INFO: "text-primary",
  WARN: "text-highlight",
  ERROR: "text-red-400",
  CONFIG: "text-accent",
  ACTION: "text-secondary",
  INPUT: "text-highlight",
  API: "text-cyan-400/60",
  API_ERR: "text-red-400",
  WHISPER: "text-purple-400",
};

function logEntryToConsoleLine(entry: LogEntry) {
  const secs = (entry.runtime / 1000).toFixed(3);
  const color = LOG_TYPE_COLORS[entry.type] ?? "text-white/30";
  return { color, text: `[${secs}] ${entry.type}: ${entry.message}` };
}

function useAutoScroll(dep: unknown) {
  const ref = useRef<HTMLDivElement>(null);
  const stickRef = useRef(true);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    const onScroll = () => {
      stickRef.current = el.scrollHeight - el.scrollTop - el.clientHeight < 40;
    };
    el.addEventListener("scroll", onScroll, { passive: true });
    return () => el.removeEventListener("scroll", onScroll);
  }, []);

  useEffect(() => {
    const el = ref.current;
    if (el && stickRef.current) {
      el.scrollTop = el.scrollHeight;
    }
  }, [dep]);

  return ref;
}

function ConsoleContent({ lines }: { lines: { color: string; text: string }[] }) {
  return (
    <>
      {lines.map((line, i) => (
        <p key={i} className={line.color}>{line.text}</p>
      ))}
      <p className="text-white/30 animate-pulse">{"\u2588"}</p>
    </>
  );
}

function DeviceConsole() {
  const [expanded, setExpanded] = useState(false);
  const { connected } = useDevice();
  const { logs, fetchDeviceLogs } = useLogs();

  useEffect(() => {
    if (!connected) return;
    fetchDeviceLogs();
    const id = setInterval(fetchDeviceLogs, 5000);
    return () => clearInterval(id);
  }, [connected, fetchDeviceLogs]);

  const hasLogs = connected && logs.length > 0;
  const lines = useMemo(
    () =>
      hasLogs
        ? logs.map((e) => logEntryToConsoleLine(e))
        : FALLBACK_CONSOLE_LINES,
    [hasLogs, logs],
  );

  const scrollRef = useAutoScroll(lines);
  const modalScrollRef = useAutoScroll(lines);

  return (
    <>
      {/* Inline console */}
      <div className="memphis-bg flex max-h-[22rem] flex-col overflow-hidden rounded-2xl border-2 border-border bg-black/80 backdrop-blur-[6px]">
        <button
          onClick={() => setExpanded(true)}
          className="flex cursor-pointer items-center gap-2 border-b border-border bg-surface px-4 py-2 text-left hover:bg-surface-raised transition-colors"
        >
          <div className="h-3 w-3 rounded-full bg-accent" />
          <div className="h-3 w-3 rounded-full bg-highlight" />
          <div className="h-3 w-3 rounded-full bg-primary" />
          <span className="ml-2 font-mono text-xs text-white/50">
            walkey-talkey.local — console
            {connected && hasLogs && " (live)"}
          </span>
        </button>
        <div ref={scrollRef} className="console-scroll flex-1 overflow-y-auto p-4 font-mono text-xs leading-relaxed">
          <ConsoleContent lines={lines} />
        </div>
      </div>

      {/* Expanded modal */}
      {expanded && (
        <div
          className="fixed inset-0 z-50 flex items-center justify-center p-6"
          onClick={() => setExpanded(false)}
        >
          {/* Backdrop */}
          <div className="absolute inset-0 bg-black/50 backdrop-blur-sm" />

          {/* Modal console */}
          <div
            className="relative flex h-[85vh] w-full max-w-5xl flex-col overflow-hidden rounded-2xl border-2 border-border bg-black/95"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="flex items-center gap-2 border-b border-white/10 px-4 py-3">
              <div className="h-3 w-3 rounded-full bg-accent" />
              <div className="h-3 w-3 rounded-full bg-highlight" />
              <div className="h-3 w-3 rounded-full bg-primary" />
              <span className="ml-2 font-mono text-sm text-white/50">
                walkey-talkey.local — console
                {connected && hasLogs && " (live)"}
              </span>
              <button
                onClick={() => setExpanded(false)}
                className="ml-auto rounded-lg p-1 text-white/40 hover:bg-white/10 hover:text-white transition-colors cursor-pointer"
              >
                <X size={18} />
              </button>
            </div>
            <div ref={modalScrollRef} className="console-scroll flex-1 overflow-y-auto p-6 font-mono text-sm leading-relaxed">
              <ConsoleContent lines={lines} />
            </div>
          </div>
        </div>
      )}
    </>
  );
}

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex items-center justify-between rounded-lg bg-surface-raised px-3 py-2">
      <span className="text-text-muted">{label}</span>
      <span className="font-bold text-text">{value}</span>
    </div>
  );
}
