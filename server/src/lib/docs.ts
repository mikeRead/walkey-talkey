export interface DocMeta {
  slug: string;
  file: string;
  title: string;
  description: string;
  icon: string;
  /** Resolve from project root instead of docs/ */
  rootLevel?: boolean;
}

export const DOCS: DocMeta[] = [
  {
    slug: "readme",
    file: "README.md",
    title: "Project README",
    description: "Overview, features, and quick-start for WalKEY-TalKEY",
    icon: "Home",
    rootLevel: true,
  },
  {
    slug: "user-guide",
    file: "USER_GUIDE.md",
    title: "User Guide",
    description: "Setup, modes, JSON authoring, recording, and daily use",
    icon: "BookOpen",
  },
  {
    slug: "rest-api",
    file: "REST_API.md",
    title: "REST API Reference",
    description: "All HTTP endpoints with request/response examples",
    icon: "Globe",
  },
  {
    slug: "technical",
    file: "TECHNICAL.md",
    title: "Technical Details",
    description: "Hardware specs, build process, USB behavior, partitions",
    icon: "Cpu",
  },
  {
    slug: "mode-system",
    file: "mode-system-reference.md",
    title: "Mode System Reference",
    description: "Binding model, action types, trigger reference",
    icon: "Layers",
  },
  {
    slug: "whisper",
    file: "WHISPER.md",
    title: "Whisper Transcription",
    description: "How browser-based speech-to-text works, known issues, and tuning",
    icon: "Sparkles",
  },
];

/** Map a .md filename (case-insensitive) to its slug for inter-doc linking. */
const FILE_TO_SLUG = new Map<string, string>(
  DOCS.map((d) => [d.file.toLowerCase(), d.slug])
);

/**
 * Resolve a markdown href (e.g. "USER_GUIDE.md", "docs/REST_API.md",
 * "../README.md") to a /docs/<slug> path, or return null if unrecognised.
 */
export function resolveDocLink(href: string): string | null {
  const basename = href.split("/").pop()?.split("#")[0]?.split("?")[0] ?? "";
  const slug = FILE_TO_SLUG.get(basename.toLowerCase());
  if (!slug) return null;
  const hash = href.includes("#") ? href.slice(href.indexOf("#")) : "";
  return `/docs/${slug}${hash}`;
}
