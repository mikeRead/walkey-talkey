"use client";

import React, { useState, useEffect, useMemo } from "react";

const C = {
  primary: "#00BFFF",
  accent: "#E91E8C",
  highlight: "#FFD700",
  secondary: "#7B2FBE",
};
const COLORS = [C.primary, C.accent, C.highlight, C.secondary];

interface Shape {
  type:
    | "tri"
    | "ring"
    | "zig"
    | "cross"
    | "squig"
    | "bolt"
    | "dot"
    | "half"
    | "arc"
    | "star"
    | "hex"
    | "diamond"
    | "bigCircle"
    | "bigSquare";
  x: number;
  y: number;
  size: number;
  color: string;
  rotate?: number;
  stroke?: boolean;
  fillOpacity?: number;
  opacity?: number;
}

function pick<T>(arr: T[]): T {
  return arr[Math.floor(Math.random() * arr.length)];
}
function rand(min: number, max: number) {
  return min + Math.random() * (max - min);
}

function generateShapes(): Shape[] {
  const shapes: Shape[] = [];

  const n = (lo: number, hi: number) => lo + Math.floor(Math.random() * (hi - lo + 1));
  const randOpacity = () => rand(0.35, 1.0);

  // Large backdrop shapes first (render behind everything) - overlapping allowed
  for (let i = 0; i < n(12, 20); i++) {
    shapes.push({
      type: Math.random() > 0.5 ? "bigCircle" : "bigSquare",
      x: rand(-10, 95),
      y: rand(-10, 95),
      size: rand(80, 320),
      color: pick(COLORS),
      rotate: rand(0, 60),
      fillOpacity: rand(0.03, 0.10),
      stroke: Math.random() > 0.5,
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(4, 7); i++) {
    shapes.push({
      type: "squig",
      x: rand(2, 92),
      y: rand(5, 92),
      size: rand(20, 90),
      color: pick(COLORS),
      rotate: rand(-35, 35),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(3, 6); i++) {
    shapes.push({
      type: "zig",
      x: rand(5, 92),
      y: rand(5, 92),
      size: rand(18, 80),
      color: pick(COLORS),
      rotate: rand(-30, 35),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(3, 6); i++) {
    shapes.push({
      type: "bolt",
      x: rand(5, 92),
      y: rand(5, 88),
      size: rand(8, 48),
      color: pick(COLORS),
      rotate: rand(-20, 25),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(3, 6); i++) {
    shapes.push({
      type: "star",
      x: rand(5, 95),
      y: rand(5, 92),
      size: rand(8, 52),
      color: pick(COLORS),
      rotate: rand(-20, 45),
      stroke: Math.random() > 0.5,
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(2, 5); i++) {
    shapes.push({
      type: "ring",
      x: rand(5, 92),
      y: rand(5, 92),
      size: rand(12, 70),
      color: pick(COLORS),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(1, 3); i++) {
    shapes.push({
      type: "arc",
      x: rand(5, 85),
      y: rand(5, 88),
      size: rand(16, 72),
      color: pick(COLORS),
      rotate: rand(-40, 40),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(1, 3); i++) {
    shapes.push({
      type: "half",
      x: rand(5, 85),
      y: rand(10, 90),
      size: rand(14, 60),
      color: pick(COLORS),
      rotate: rand(-60, 60),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(2, 4); i++) {
    shapes.push({
      type: "hex",
      x: rand(2, 90),
      y: rand(5, 92),
      size: rand(8, 48),
      color: pick(COLORS),
      stroke: Math.random() > 0.4,
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(2, 4); i++) {
    shapes.push({
      type: "diamond",
      x: rand(5, 88),
      y: rand(10, 88),
      size: rand(6, 36),
      color: pick(COLORS),
      rotate: rand(15, 65),
      stroke: Math.random() > 0.5,
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(1, 3); i++) {
    shapes.push({
      type: "cross",
      x: rand(10, 88),
      y: rand(10, 92),
      size: rand(6, 34),
      color: pick(COLORS),
      rotate: rand(0, 45),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(1, 3); i++) {
    shapes.push({
      type: "tri",
      x: rand(10, 88),
      y: rand(5, 88),
      size: rand(8, 44),
      color: pick(COLORS),
      rotate: rand(-30, 30),
      opacity: randOpacity(),
    });
  }

  for (let i = 0; i < n(4, 8); i++) {
    shapes.push({
      type: "dot",
      x: rand(3, 97),
      y: rand(3, 97),
      size: rand(3, 20),
      color: pick(COLORS),
      opacity: randOpacity(),
    });
  }

  return shapes;
}

/* ---- Deterministic PRNG for per-shape animation timing ---- */
function seededRandom(seed: number) {
  let s = seed;
  return () => {
    s = (s * 16807 + 0) % 2147483647;
    return (s - 1) / 2147483646;
  };
}

function generateDriftKf(i: number): string {
  const rng = seededRandom(i * 1301 + 7);
  const name = `drift-${i}`;
  const lines = [`@keyframes ${name} {`, `  0%, 100% { transform: translate(0, 0); }`];
  for (let p = 1; p <= 3; p++) {
    const pct = Math.round((p / 4) * 100);
    const tx = Math.round((rng() - 0.5) * 50);
    const ty = Math.round((rng() - 0.5) * 50);
    lines.push(`  ${pct}% { transform: translate(${tx}px, ${ty}px); }`);
  }
  lines.push(`}`);
  return lines.join("\n");
}

/* ------------------------------------------------------------------ */
/*  Per-type signature keyframes                                       */
/* ------------------------------------------------------------------ */
const TYPE_KEYFRAMES = `
/* ---- Squiggle: slithering snake scroll ---- */
@keyframes squig-scroll {
  from { transform: translateX(0); }
  to   { transform: translateX(-14px); }
}

/* ---- Zigzag: electric current flowing + glow pulse ---- */
@keyframes zig-current {
  to { stroke-dashoffset: -100; }
}
@keyframes zig-glow {
  0%, 100% { opacity: 0.65; filter: brightness(1); }
  50%      { opacity: 1; filter: brightness(1.6); }
}

/* ---- Bolt: trace-in, flash, trace-out ---- */
@keyframes bolt-trace {
  0%        { stroke-dashoffset: 100; }
  22%       { stroke-dashoffset: 0; }
  40%       { stroke-dashoffset: 0; }
  62%       { stroke-dashoffset: -100; }
  100%      { stroke-dashoffset: 100; }
}
@keyframes bolt-flash {
  0%, 62%, 100% { opacity: 0.15; filter: drop-shadow(0 0 0px currentColor); }
  22%           { opacity: 1;    filter: drop-shadow(0 0 6px currentColor); }
  40%           { opacity: 1;    filter: drop-shadow(0 0 10px currentColor); }
}

/* ---- Star: twinkle sparkle with rotation ---- */
@keyframes star-twinkle {
  0%, 100% { transform: scale(0.25) rotate(0deg); opacity: 0.08; }
  12%      { transform: scale(1.35) rotate(36deg); opacity: 1;   filter: drop-shadow(0 0 5px currentColor); }
  22%      { transform: scale(0.8)  rotate(72deg); opacity: 0.55; }
  32%      { transform: scale(1.2)  rotate(108deg); opacity: 1;  filter: drop-shadow(0 0 3px currentColor); }
  48%      { transform: scale(1)    rotate(180deg); opacity: 0.8; }
  65%      { transform: scale(0.4)  rotate(252deg); opacity: 0.15; }
}

/* ---- Ring: loading-spinner dash morph + rotation ---- */
@keyframes ring-dash {
  0%   { stroke-dasharray: 1 99;  stroke-dashoffset: 0; }
  50%  { stroke-dasharray: 55 45; stroke-dashoffset: -25; }
  100% { stroke-dasharray: 1 99;  stroke-dashoffset: -100; }
}
@keyframes ring-rotate {
  to { transform: rotate(360deg); }
}

/* ---- Arc: pendulum swing ---- */
@keyframes arc-pendulum {
  0%, 100% { transform: rotate(-28deg) scaleX(1); }
  25%      { transform: rotate(0deg)   scaleX(1.06); }
  50%      { transform: rotate(28deg)  scaleX(1); }
  75%      { transform: rotate(0deg)   scaleX(1.06); }
}

/* ---- Half-circle: rocking cradle ---- */
@keyframes half-rock {
  0%, 100% { transform: rotate(-22deg) translateX(0); }
  25%      { transform: rotate(4deg)   translateX(5px); }
  50%      { transform: rotate(22deg)  translateX(0); }
  75%      { transform: rotate(-4deg)  translateX(-5px); }
}

/* ---- Hexagon: 3D perspective flip ---- */
@keyframes hex-flip {
  0%, 100% { transform: perspective(150px) rotateY(0deg)   scale(1); }
  25%      { transform: perspective(150px) rotateY(90deg)  scale(0.82); }
  50%      { transform: perspective(150px) rotateY(180deg) scale(1); }
  75%      { transform: perspective(150px) rotateY(270deg) scale(0.82); }
}

/* ---- Diamond: float up + card flip ---- */
@keyframes diamond-flip {
  0%, 100% { transform: translateY(0)     perspective(120px) rotateX(0deg); }
  28%      { transform: translateY(-20px)  perspective(120px) rotateX(0deg); }
  50%      { transform: translateY(-20px)  perspective(120px) rotateX(180deg); }
  72%      { transform: translateY(0)      perspective(120px) rotateX(180deg); }
}

/* ---- Cross: windmill spin with breath ---- */
@keyframes cross-windmill {
  0%   { transform: rotate(0deg)   scale(1); }
  25%  { transform: rotate(90deg)  scale(1.12); }
  50%  { transform: rotate(180deg) scale(1); }
  75%  { transform: rotate(270deg) scale(0.88); }
  100% { transform: rotate(360deg) scale(1); }
}

/* ---- Triangle: bouncing with squash & stretch ---- */
@keyframes tri-bounce {
  0%, 100% { transform: translateY(0)     scaleX(1)    scaleY(1); }
  12%      { transform: translateY(-24px)  scaleX(0.9)  scaleY(1.14); }
  30%      { transform: translateY(5px)    scaleX(1.2)  scaleY(0.78); }
  42%      { transform: translateY(-11px)  scaleX(0.95) scaleY(1.07); }
  58%      { transform: translateY(2px)    scaleX(1.06) scaleY(0.94); }
  72%      { transform: translateY(0)      scaleX(1)    scaleY(1); }
}

/* ---- Dot: heartbeat double-pulse ---- */
@keyframes dot-heartbeat {
  0%, 100% { transform: scale(1);   opacity: 0.55; }
  12%      { transform: scale(1.7); opacity: 1; }
  24%      { transform: scale(1);   opacity: 0.55; }
  36%      { transform: scale(1.4); opacity: 0.85; }
  48%      { transform: scale(1);   opacity: 0.55; }
}

/* ---- Big backdrop shapes: slow gentle float ---- */
@keyframes backdrop-float {
  0%, 100% { transform: scale(1)    rotate(0deg); }
  33%      { transform: scale(1.06) rotate(4deg); }
  66%      { transform: scale(0.94) rotate(-4deg); }
}
`;

/* ------------------------------------------------------------------ */
/*  Render each shape with its signature animation                     */
/* ------------------------------------------------------------------ */
function renderShape(s: Shape, i: number) {
  const rng = seededRandom(i * 997 + 31);
  const tMult = 0.5 + rng() * 1.8;
  const delay = -(rng() * 40);
  const driftDur = 20 + rng() * 50;
  const driftDelay = -(rng() * 40);

  const outerStyle: React.CSSProperties = {
    position: "absolute",
    left: `${s.x}%`,
    top: `${s.y}%`,
    transform: s.rotate ? `rotate(${s.rotate}deg)` : undefined,
    opacity: s.opacity ?? 1,
  };

  const driftStyle: React.CSSProperties = {
    animation: `drift-${i} ${driftDur.toFixed(1)}s ease-in-out ${driftDelay.toFixed(1)}s infinite`,
  };

  const dur = (base: number) => `${(base * tMult).toFixed(1)}s`;
  const dl = `${delay.toFixed(1)}s`;

  let content: React.ReactNode;

  switch (s.type) {
    case "bigCircle": {
      const r = s.size / 2;
      const sw = Math.max(3, s.size * 0.025);
      content = (
        <div style={{ animation: `backdrop-float ${dur(22)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`}>
            {s.stroke ? (
              <circle cx={r} cy={r} r={r - sw / 2} fill="none" stroke={s.color} strokeWidth={sw} opacity={s.fillOpacity ?? 0.06} />
            ) : (
              <circle cx={r} cy={r} r={r} fill={s.color} fillOpacity={s.fillOpacity ?? 0.06} />
            )}
          </svg>
        </div>
      );
      break;
    }

    case "bigSquare": {
      const rx = s.size * 0.08;
      const sw = Math.max(3, s.size * 0.025);
      content = (
        <div style={{ animation: `backdrop-float ${dur(25)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`}>
            {s.stroke ? (
              <rect x={sw / 2} y={sw / 2} width={s.size - sw} height={s.size - sw} rx={rx} fill="none" stroke={s.color} strokeWidth={sw} opacity={s.fillOpacity ?? 0.06} />
            ) : (
              <rect width={s.size} height={s.size} rx={rx} fill={s.color} fillOpacity={s.fillOpacity ?? 0.06} />
            )}
          </svg>
        </div>
      );
      break;
    }

    case "squig": {
      const wl = 14;
      const amp = 6;
      const mid = 7;
      const viewH = 14;
      const viewW = s.size;
      const extra = wl * 3;
      const startX = -extra;
      const endX = viewW + extra;
      const numHalves = Math.ceil((endX - startX) / (wl / 2));
      let pathD = `M ${startX} ${mid}`;
      for (let j = 0; j < numHalves; j++) {
        const x0 = startX + j * (wl / 2);
        const x1 = x0 + wl / 2;
        const cx = (x0 + x1) / 2;
        const cy = j % 2 === 0 ? mid - amp : mid + amp;
        pathD += ` Q ${cx} ${cy} ${x1} ${mid}`;
      }
      const dir = rng() > 0.5 ? "normal" : "reverse";
      content = (
        <svg width={viewW} height={viewH} viewBox={`0 0 ${viewW} ${viewH}`} fill="none" overflow="hidden">
          <path
            d={pathD}
            stroke={s.color}
            strokeWidth={2.5}
            strokeLinecap="round"
            style={{
              animation: `squig-scroll ${dur(2)} linear ${dl} infinite`,
              animationDirection: dir,
            }}
          />
        </svg>
      );
      break;
    }

    case "zig": {
      content = (
        <div style={{ animation: `zig-glow ${dur(2.2)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={4} viewBox={`0 0 ${s.size} 4`} fill="none">
            <line
              x1={0} y1={2} x2={s.size} y2={2}
              stroke={s.color}
              strokeWidth={2}
              strokeLinecap="round"
              pathLength={100}
              strokeDasharray="18 12"
              style={{ animation: `zig-current ${dur(1.6)} linear ${dl} infinite` }}
            />
          </svg>
        </div>
      );
      break;
    }

    case "bolt": {
      content = (
        <div style={{ color: s.color, animation: `bolt-flash ${dur(4.5)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={s.size * 1.55} viewBox="0 0 18 28" fill="none">
            <path
              d="M10 0L6 12H14L4 28"
              stroke={s.color}
              strokeWidth={2.5}
              strokeLinecap="round"
              pathLength={100}
              strokeDasharray="100"
              style={{ animation: `bolt-trace ${dur(4.5)} ease-in-out ${dl} infinite` }}
            />
          </svg>
        </div>
      );
      break;
    }

    case "star": {
      const r = s.size / 2;
      const cx = r, cy = r;
      const pts = 8;
      const points = Array.from({ length: pts * 2 }, (_, j) => {
        const angle = (Math.PI * j) / pts - Math.PI / 2;
        const rad = j % 2 === 0 ? r : r * 0.5;
        return `${cx + rad * Math.cos(angle)},${cy + rad * Math.sin(angle)}`;
      }).join(" ");
      content = (
        <div style={{ color: s.color, animation: `star-twinkle ${dur(5)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`} fill="none">
            <polygon
              points={points}
              fill={s.stroke ? "none" : s.color}
              stroke={s.stroke ? s.color : "none"}
              strokeWidth={2}
            />
          </svg>
        </div>
      );
      break;
    }

    case "ring": {
      const r = s.size / 2;
      content = (
        <div style={{ animation: `ring-rotate ${dur(3)} linear ${dl} infinite` }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`} fill="none">
            <circle
              cx={r}
              cy={r}
              r={r - 2}
              stroke={s.color}
              strokeWidth={3}
              strokeLinecap="round"
              pathLength={100}
              style={{ animation: `ring-dash ${dur(2.5)} ease-in-out ${dl} infinite` }}
            />
          </svg>
        </div>
      );
      break;
    }

    case "arc": {
      content = (
        <div style={{ animation: `arc-pendulum ${dur(4)} ease-in-out ${dl} infinite`, transformOrigin: "50% 100%" }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`} fill="none">
            <path
              d={`M${s.size * 0.15} ${s.size * 0.85}A${s.size / 2} ${s.size / 2} 0 0 1 ${s.size * 0.85} ${s.size * 0.85}`}
              stroke={s.color}
              strokeWidth={2.5}
              strokeLinecap="round"
            />
          </svg>
        </div>
      );
      break;
    }

    case "half": {
      content = (
        <div style={{ animation: `half-rock ${dur(3.8)} ease-in-out ${dl} infinite`, transformOrigin: "50% 100%" }}>
          <svg width={s.size} height={s.size / 2} viewBox={`0 0 ${s.size} ${s.size / 2}`} fill="none">
            <path
              d={`M0 ${s.size / 2}A${s.size / 2} ${s.size / 2} 0 0 1 ${s.size} ${s.size / 2}`}
              stroke={s.color}
              strokeWidth={3}
              strokeLinecap="round"
            />
          </svg>
        </div>
      );
      break;
    }

    case "hex": {
      const r = s.size / 2;
      const cx = r, cy = r;
      const points = Array.from({ length: 6 }, (_, j) => {
        const angle = (Math.PI * 2 * j) / 6 - Math.PI / 2;
        return `${cx + r * Math.cos(angle)},${cy + r * Math.sin(angle)}`;
      }).join(" ");
      content = (
        <div style={{ animation: `hex-flip ${dur(7)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`} fill="none">
            <polygon
              points={points}
              fill={s.stroke ? "none" : s.color}
              stroke={s.stroke ? s.color : "none"}
              strokeWidth={2}
            />
          </svg>
        </div>
      );
      break;
    }

    case "diamond": {
      content = (
        <div style={{ animation: `diamond-flip ${dur(5.5)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size * 0.7} height={s.size} viewBox="0 0 10 18" fill="none">
            <rect
              width={10}
              height={18}
              rx={2}
              fill={s.stroke ? "none" : s.color}
              stroke={s.stroke ? s.color : "none"}
              strokeWidth={2}
            />
          </svg>
        </div>
      );
      break;
    }

    case "cross": {
      content = (
        <div style={{ animation: `cross-windmill ${dur(4)} linear ${dl} infinite` }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`} fill="none">
            <line x1={s.size / 2} y1={0} x2={s.size / 2} y2={s.size} stroke={s.color} strokeWidth={2} strokeLinecap="round" />
            <line x1={0} y1={s.size / 2} x2={s.size} y2={s.size / 2} stroke={s.color} strokeWidth={2} strokeLinecap="round" />
          </svg>
        </div>
      );
      break;
    }

    case "tri": {
      content = (
        <div style={{ animation: `tri-bounce ${dur(2.8)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={s.size * 0.86} viewBox="0 0 28 24" fill="none">
            <path d="M14 0L28 24H0Z" fill={s.color} />
          </svg>
        </div>
      );
      break;
    }

    case "dot": {
      content = (
        <div style={{ animation: `dot-heartbeat ${dur(2)} ease-in-out ${dl} infinite` }}>
          <svg width={s.size} height={s.size} viewBox={`0 0 ${s.size} ${s.size}`} fill="none">
            <circle cx={s.size / 2} cy={s.size / 2} r={s.size / 2} fill={s.color} />
          </svg>
        </div>
      );
      break;
    }

    default:
      content = null;
  }

  return (
    <div key={i} style={outerStyle}>
      <div style={driftStyle}>{content}</div>
    </div>
  );
}

export function MemphisBackground() {
  const [shapes, setShapes] = useState<Shape[]>([]);

  useEffect(() => {
    setShapes(generateShapes());
  }, []);

  const css = useMemo(() => {
    if (!shapes.length) return "";
    return TYPE_KEYFRAMES + "\n" + shapes.map((_, i) => generateDriftKf(i)).join("\n");
  }, [shapes]);

  if (!shapes.length) return null;

  return (
    <>
      <style dangerouslySetInnerHTML={{ __html: css }} />
      <div
        className="pointer-events-none fixed inset-0 z-0 overflow-hidden opacity-[0.35]"
        aria-hidden="true"
      >
        {shapes.map((s, i) => renderShape(s, i))}
      </div>
    </>
  );
}
