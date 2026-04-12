import fs from "fs";
import path from "path";
import Link from "next/link";
import { DOCS } from "@/lib/docs";
import { MarkdownRenderer } from "@/components/markdown-renderer";
import { DownloadMdButton } from "@/components/download-md-button";
import { ArrowLeft } from "lucide-react";
import { notFound } from "next/navigation";

export function generateStaticParams() {
  return DOCS.map((d) => ({ slug: d.slug }));
}

interface Props {
  params: Promise<{ slug: string }>;
}

export default async function DocPage({ params }: Props) {
  const { slug } = await params;
  const doc = DOCS.find((d) => d.slug === slug);
  if (!doc) notFound();

  const docsDir = path.join(process.cwd(), "..", "docs");
  const filePath = path.join(docsDir, doc.file);

  let content: string;
  try {
    content = fs.readFileSync(filePath, "utf-8");
  } catch {
    content = `# ${doc.title}\n\nDocumentation file not found at \`${doc.file}\`. Make sure the \`docs/\` directory is present alongside the \`server/\` directory.`;
  }

  return (
    <div>
      <div className="mb-6 flex items-center gap-3">
        <Link
          href="/docs"
          className="btn btn-sm btn-ghost"
        >
          <ArrowLeft size={16} className="mr-1" />
          Docs
        </Link>
        <h1 className="text-xl font-extrabold text-secondary">{doc.title}</h1>
        <DownloadMdButton content={content} filename={doc.file} />
      </div>

      <div className="card">
        <MarkdownRenderer content={content} />
      </div>
    </div>
  );
}
