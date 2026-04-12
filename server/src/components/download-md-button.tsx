"use client";

import { Download } from "lucide-react";

interface Props {
  content: string;
  filename: string;
}

export function DownloadMdButton({ content, filename }: Props) {
  const handleDownload = () => {
    const blob = new Blob([content], { type: "text/markdown;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <button
      onClick={handleDownload}
      className="ml-auto flex cursor-pointer items-center gap-1.5 rounded-lg border border-border bg-surface-raised px-3 py-1.5 text-xs font-bold text-text-muted transition-colors hover:border-secondary hover:text-secondary"
    >
      <Download size={14} />
      Download MD
    </button>
  );
}
