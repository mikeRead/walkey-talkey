import Image from "next/image";
import { ConfigEditor } from "@/components/config-editor";

export default function ConfigPage() {
  return (
    <div>
      <div className="memphis-bg mb-6 flex items-center gap-5 rounded-xl border-2 border-dashed border-border p-6 backdrop-blur-[6px]">
        <div className="h-40 w-40 shrink-0 overflow-hidden rounded-lg">
          <Image
            src="/config.png"
            alt="Configuration"
            width={200}
            height={200}
            className="h-full w-full object-cover"
            style={{ transform: "scale(1.25)", objectPosition: "55% center" }}
          />
        </div>
        <div>
          <h1 className="text-2xl font-extrabold sm:text-3xl">
            <span className="text-primary">Configuration</span>
          </h1>
          <p className="mt-1 text-sm text-text-muted">
            Manage modes, bindings, defaults, and device settings
          </p>
        </div>
      </div>
      <ConfigEditor />
    </div>
  );
}
