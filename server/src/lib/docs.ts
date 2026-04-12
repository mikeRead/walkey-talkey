export interface DocMeta {
  slug: string;
  file: string;
  title: string;
  description: string;
  icon: string;
}

export const DOCS: DocMeta[] = [
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
