export function cn(...classes: (string | false | null | undefined)[]) {
  return classes.filter(Boolean).join(" ");
}

export function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

export function triggerLabel(trigger: string): string {
  return trigger.replace(/_/g, " ").replace(/\b\w/g, (c) => c.toUpperCase());
}

export function formatShortTime(seconds: number): string {
  if (seconds >= 60) return `${Math.ceil(seconds / 60)}M`;
  return `${Math.max(1, Math.ceil(seconds))}s`;
}
