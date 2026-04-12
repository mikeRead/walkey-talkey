import { pipeline, type AutomaticSpeechRecognitionPipeline } from "@huggingface/transformers";

let transcriber: AutomaticSpeechRecognitionPipeline | null = null;
let currentModel = "";

const HALLUCINATION_PHRASES = new Set([
  "you", "thank you", "thanks", "thank you.", "thanks.", "thank you!",
  "thank you for watching", "thank you for watching.", "thank you for watching!",
  "thanks for watching", "thanks for watching.", "thanks for watching!",
  "bye", "bye.", "bye bye", "bye bye.",
  "subscribe", "please subscribe",
  "the end", "the end.",
  "so", "and", "but", "um", "uh",
  "...", ".", "",
]);

const TRAILING_MUTE_SAMPLES = 1600;

function prepareAudio(raw: Float32Array): Float32Array {
  const audio = new Float32Array(raw);
  const muteStart = Math.max(0, audio.length - TRAILING_MUTE_SAMPLES);
  for (let i = muteStart; i < audio.length; i++) audio[i] = 0;
  return audio;
}

function isHallucination(text: string): boolean {
  const cleaned = text.trim().toLowerCase().replace(/[^\w\s.!]/g, "");
  if (cleaned.length === 0) return true;
  if (HALLUCINATION_PHRASES.has(cleaned)) return true;
  return false;
}

const ctx = self as unknown as Worker;

ctx.addEventListener("message", async (event: MessageEvent) => {
  const { audio: rawAudio, model, language } = event.data;

  try {
    if (!transcriber || currentModel !== model) {
      ctx.postMessage({ status: "loading", message: "Loading model..." });

      transcriber = await pipeline("automatic-speech-recognition", model, {
        dtype: "fp32",
        progress_callback: (data: Record<string, unknown>) => {
          ctx.postMessage({ status: "progress", ...data });
        },
      }) as AutomaticSpeechRecognitionPipeline;
      currentModel = model;
    }

    ctx.postMessage({ status: "transcribing" });

    const audio = prepareAudio(rawAudio);
    const isDistil = model.startsWith("distil-whisper/");

    const result = await transcriber(audio, {
      chunk_length_s: isDistil ? 20 : 30,
      stride_length_s: isDistil ? 3 : 5,
      language,
      task: "transcribe",
      return_timestamps: true,
      force_full_sequences: false,
    });

    const text = typeof result === "string" ? result : (result as { text?: string })?.text ?? "";
    if (isHallucination(text)) {
      ctx.postMessage({
        status: "complete",
        data: { text: "(No speech detected)" },
      });
    } else {
      ctx.postMessage({ status: "complete", data: result });
    }
  } catch (error) {
    ctx.postMessage({
      status: "error",
      data: error instanceof Error ? error.message : String(error),
    });
  }
});
