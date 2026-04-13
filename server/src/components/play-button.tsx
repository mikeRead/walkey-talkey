"use client";

import { useState, useRef, useCallback, useEffect } from "react";
import { Play, Loader2 } from "lucide-react";
import { formatShortTime } from "@/lib/utils";
import { downloadFile } from "@/lib/api";

interface PlayButtonProps {
  src: string;
  expectedSize?: number;
}


const SIZE = 36;
const STROKE = 2.5;
const RADIUS = (SIZE - STROKE) / 2;
const CIRCUMFERENCE = 2 * Math.PI * RADIUS;

export function PlayButton({ src, expectedSize }: PlayButtonProps) {
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const rafRef = useRef<number>(0);
  const blobUrlRef = useRef<string | null>(null);
  const srcRef = useRef(src);
  const [playing, setPlaying] = useState(false);
  const [loading, setLoading] = useState(false);
  const [progress, setProgress] = useState(0);
  const [remaining, setRemaining] = useState<number | null>(null);

  const tick = useCallback(() => {
    const a = audioRef.current;
    if (!a || a.paused) return;
    const dur = a.duration;
    if (isFinite(dur) && dur > 0) {
      const cur = a.currentTime;
      setProgress(cur / dur);
      setRemaining(Math.max(0, dur - cur));
    }
    rafRef.current = requestAnimationFrame(tick);
  }, []);

  const stop = useCallback(() => {
    const a = audioRef.current;
    if (a) {
      a.pause();
      a.currentTime = 0;
    }
    setPlaying(false);
    setProgress(0);
    setRemaining(null);
    cancelAnimationFrame(rafRef.current);
  }, []);

  // Invalidate cached blob when src changes
  useEffect(() => {
    if (srcRef.current !== src) {
      stop();
      if (blobUrlRef.current) {
        URL.revokeObjectURL(blobUrlRef.current);
        blobUrlRef.current = null;
      }
      srcRef.current = src;
    }
  }, [src, stop]);

  useEffect(() => {
    return () => {
      cancelAnimationFrame(rafRef.current);
      if (blobUrlRef.current) URL.revokeObjectURL(blobUrlRef.current);
    };
  }, []);

  const handleEnded = useCallback(() => {
    setPlaying(false);
    setProgress(0);
    setRemaining(null);
    cancelAnimationFrame(rafRef.current);
  }, []);

  const handlePlay = useCallback(async () => {
    if (loading) return;
    const a = audioRef.current;
    if (!a) return;

    if (playing) {
      stop();
      return;
    }

    if (!blobUrlRef.current) {
      setLoading(true);
      try {
        const buf = await downloadFile(src, expectedSize ?? 0);
        blobUrlRef.current = URL.createObjectURL(
          new Blob([buf], { type: "audio/wav" }),
        );
      } catch {
        setLoading(false);
        return;
      }
      setLoading(false);
    }

    a.src = blobUrlRef.current;
    try {
      await a.play();
      setPlaying(true);
      if (isFinite(a.duration) && a.duration > 0) {
        setRemaining(a.duration);
      }
      rafRef.current = requestAnimationFrame(tick);
    } catch {
      // autoplay blocked
    }
  }, [playing, loading, src, tick, stop]);

  const dashOffset = CIRCUMFERENCE * (1 - progress);

  return (
    <button
      onClick={handlePlay}
      disabled={loading}
      className="relative flex items-center justify-center shrink-0 cursor-pointer bg-transparent border-0 p-0"
      style={{ width: SIZE, height: SIZE }}
      title={loading ? "Loading..." : playing ? "Stop" : "Play"}
    >
      <svg
        viewBox={`0 0 ${SIZE} ${SIZE}`}
        width={SIZE}
        height={SIZE}
        className="absolute inset-0"
      >
        <circle
          cx={SIZE / 2}
          cy={SIZE / 2}
          r={RADIUS}
          fill="none"
          stroke="var(--color-text-muted)"
          strokeWidth={STROKE}
          opacity={0.3}
        />
        {playing && (
          <circle
            cx={SIZE / 2}
            cy={SIZE / 2}
            r={RADIUS}
            fill="none"
            stroke="var(--color-success)"
            strokeWidth={STROKE}
            strokeDasharray={CIRCUMFERENCE}
            strokeDashoffset={dashOffset}
            strokeLinecap="round"
            transform={`rotate(-90 ${SIZE / 2} ${SIZE / 2})`}
          />
        )}
      </svg>
      <span className="relative z-10 flex items-center justify-center">
        {loading ? (
          <Loader2 size={14} className="text-text-muted animate-spin" />
        ) : playing ? (
          remaining !== null ? (
            <span className="text-[10px] font-bold leading-none text-success tabular-nums">
              {formatShortTime(remaining)}
            </span>
          ) : (
            <span className="text-[10px] font-bold leading-none text-success animate-pulse">
              &hellip;
            </span>
          )
        ) : (
          <Play size={14} className="text-text ml-0.5" />
        )}
      </span>
      <audio ref={audioRef} onEnded={handleEnded} className="hidden" />
    </button>
  );
}
