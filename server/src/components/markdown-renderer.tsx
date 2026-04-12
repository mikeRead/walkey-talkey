"use client";

import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import { Prism as SyntaxHighlighter } from "react-syntax-highlighter";
import type { Components } from "react-markdown";

interface MarkdownRendererProps {
  content: string;
}

/* ── WalKEY-TalKEY branded syntax theme ── */
const C = {
  bg: "#0a0a0a",
  surface: "#141414",
  border: "#2a2a2a",
  text: "#f5f5f5",
  muted: "#888888",
  cyan: "#00BFFF",
  magenta: "#E91E8C",
  gold: "#FFD700",
  purple: "#7B2FBE",
  cyanDim: "#66d9ff",
  magentaDim: "#f472b6",
  greenish: "#4ade80",
  orange: "#fb923c",
};

const walkeyTheme: Record<string, React.CSSProperties> = {
  'pre[class*="language-"]': {
    background: C.bg,
    borderRadius: "0.75rem",
    border: `2px solid ${C.border}`,
    margin: "1rem 0",
    fontSize: "0.875rem",
    padding: "1rem",
    overflow: "auto",
    textShadow: "none",
  },
  'code[class*="language-"]': {
    background: "none",
    color: C.text,
    fontSize: "0.875rem",
    fontFamily: '"Fira Code", "Fira Mono", Menlo, Consolas, monospace',
    textShadow: "none",
  },

  comment: { color: C.muted, fontStyle: "italic" },
  prolog: { color: C.muted, fontStyle: "italic" },
  doctype: { color: C.muted, fontStyle: "italic" },
  cdata: { color: C.muted },

  punctuation: { color: "#a0a0a0" },
  namespace: { opacity: 0.8 },

  property: { color: C.cyan },
  tag: { color: C.magenta },
  boolean: { color: C.gold },
  number: { color: C.gold },
  constant: { color: C.gold },
  symbol: { color: C.gold },
  deleted: { color: C.magenta },

  selector: { color: C.greenish },
  "attr-name": { color: C.cyan },
  string: { color: C.greenish },
  char: { color: C.greenish },
  builtin: { color: C.cyanDim },
  inserted: { color: C.greenish },

  operator: { color: C.magentaDim },
  entity: { color: C.orange, cursor: "help" },
  url: { color: C.cyan },
  ".language-css .token.string": { color: C.orange },
  ".style .token.string": { color: C.orange },

  "attr-value": { color: C.greenish },
  keyword: { color: C.magenta, fontWeight: "bold" },
  function: { color: C.cyan },
  "class-name": { color: C.gold },
  regex: { color: C.orange },
  important: { color: C.gold, fontWeight: "bold" },
  variable: { color: C.cyanDim },

  bold: { fontWeight: "bold" },
  italic: { fontStyle: "italic" },

  "function-variable": { color: C.cyan },
  parameter: { color: C.orange },
  interpolation: { color: C.magentaDim },
  "template-string": { color: C.greenish },
  "template-punctuation": { color: C.magenta },
  "script-punctuation": { color: C.magenta },

  "key": { color: C.cyan },
  "atrule": { color: C.magenta },
  "rule": { color: C.cyan },
  "plain-text": { color: C.text },
};

const components: Partial<Components> = {
  code({ className, children, ...rest }) {
    const match = /language-(\w+)/.exec(className || "");
    const codeString = String(children).replace(/\n$/, "");

    if (match) {
      return (
        <SyntaxHighlighter
          style={walkeyTheme}
          language={match[1]}
          PreTag="div"
          showLineNumbers
          lineNumberStyle={{ color: "#444", fontSize: "0.75rem", paddingRight: "1em", minWidth: "2em", userSelect: "none" }}
        >
          {codeString}
        </SyntaxHighlighter>
      );
    }

    return (
      <code className={`${className ?? ""} font-bold`} {...rest}>
        {children}
      </code>
    );
  },
};

export function MarkdownRenderer({ content }: MarkdownRendererProps) {
  return (
    <div className="prose-branded">
      <ReactMarkdown remarkPlugins={[remarkGfm]} components={components}>
        {content}
      </ReactMarkdown>
    </div>
  );
}
