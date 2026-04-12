import { ConfigEditor } from "@/components/config-editor";

export default function ConfigPage() {
  return (
    <div>
      <div className="mb-6 rounded-xl border border-dashed border-border p-6 backdrop-blur-[6px]">
        <h1 className="text-2xl font-extrabold sm:text-3xl">
          <span className="text-primary">Configuration</span>
        </h1>
        <p className="mt-1 text-sm text-text-muted">
          Manage modes, bindings, defaults, and device settings
        </p>
      </div>
      <ConfigEditor />
    </div>
  );
}
