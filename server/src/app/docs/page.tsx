import Link from "next/link";
import { DOCS } from "@/lib/docs";
import { BookOpen, Globe, Cpu, Layers, Sparkles } from "lucide-react";

const ICON_MAP: Record<string, React.ComponentType<{ size?: number; className?: string }>> = {
  BookOpen,
  Globe,
  Cpu,
  Layers,
  Sparkles,
};

export default function DocsHub() {
  return (
    <div>
      <div className="mb-6 rounded-xl border border-dashed border-border p-6 backdrop-blur-[6px]">
        <h1 className="text-2xl font-extrabold sm:text-3xl">
          <span className="text-secondary">Documentation</span>
        </h1>
        <p className="mt-1 text-sm text-text-muted">
          Everything you need to know about WalKEY-TalKEY
        </p>
      </div>

      <div className="grid gap-4 sm:grid-cols-2">
        {DOCS.map((doc) => {
          const Icon = ICON_MAP[doc.icon] ?? BookOpen;
          return (
            <Link
              key={doc.slug}
              href={`/docs/${doc.slug}`}
              className="card group flex items-start gap-4 transition-all hover:border-secondary hover:shadow-lg hover:shadow-secondary/10"
            >
              <div className="flex h-12 w-12 shrink-0 items-center justify-center rounded-lg bg-secondary/20 text-secondary transition-colors group-hover:bg-secondary/30">
                <Icon size={24} />
              </div>
              <div>
                <h2 className="text-lg font-extrabold group-hover:text-secondary">
                  {doc.title}
                </h2>
                <p className="mt-1 text-sm text-text-muted">
                  {doc.description}
                </p>
              </div>
            </Link>
          );
        })}
      </div>
    </div>
  );
}
