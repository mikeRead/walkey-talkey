import type { Metadata } from "next";
import "./globals.css";
import { NavShell } from "@/components/nav-shell";
import { MemphisBackground } from "@/components/memphis-background";

export const metadata: Metadata = {
  title: "WalKEY-TalKEY Dashboard",
  description: "Configure your WalKEY-TalKEY ESP32-S3 macro controller",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en" className="dark" suppressHydrationWarning>
      <head>
        <script
          dangerouslySetInnerHTML={{
            __html: `(function(){var o=console.error,w=console.warn;function f(a){return typeof a==="string"&&(a.includes("searchParams")||a.includes("params are being enumerated"))&&a.includes("Promise")}console.error=function(){if(!f(arguments[0]))o.apply(console,arguments)};console.warn=function(){if(!f(arguments[0]))w.apply(console,arguments)}})();`,
          }}
        />
      </head>
      <body className="min-h-screen bg-bg antialiased">
        <MemphisBackground />
        <div className="relative z-10">
          <NavShell>{children}</NavShell>
        </div>
      </body>
    </html>
  );
}
