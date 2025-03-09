import { useSetAtom, useStore, atom } from "jotai";
import {
  hasErrorAtom,
  progressAtom,
  progressTotalAtom,
  showSpinnerAtom,
  statusAtom,
} from "./atoms/state";
import { RefObject, useEffect, useRef } from "react";

declare global {
  interface Window {
    Module: EmscriptenModule;
  }
}

type CrawlEmscriptenModule = Partial<
  EmscriptenModule & {
    canvas: HTMLCanvasElement;
    monitorRunDependencies: (left: number) => void;
    setStatus: (text: string) => void;
  }
>;

const moduleInstanceAtom = atom<CrawlEmscriptenModule>({
  print(...args: unknown[]) {
    console.log(...args);
    // These replacements are necessary if you render to raw HTMLaw
    //text = text.replace(/&/g, "&amp;");
    //text = text.replace(/</g, "&lt;");
    //text = text.replace(/>/g, "&gt;");
    //text = text.replace('\n', '<br>', 'g');
    // if (outputElement) {
    //   var text = args.join(" ");
    //   outputElement.value += text + "\n";
    //   outputElement.scrollTop = outputElement.scrollHeight; // focus on bottom
    // }
  },
});

const moduleAtom = atom(
  (get) => get(moduleInstanceAtom),
  (_, set, update: CrawlEmscriptenModule) => {
    const nextModule = { ...window.Module, ...update };
    window.Module = nextModule;
    set(moduleInstanceAtom, nextModule);
  }
);

export const useModule = (canvasRef: RefObject<HTMLCanvasElement>) => {
  const setStatus = useSetAtom(statusAtom);
  const setShowSpinner = useSetAtom(showSpinnerAtom);
  const setProgress = useSetAtom(progressAtom);
  const setProgressTotal = useSetAtom(progressTotalAtom);
  const setModule = useSetAtom(moduleAtom);
  const setHasError = useSetAtom(hasErrorAtom);
  const store = useStore();
  const initialised = useRef(false);
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || initialised.current) return;
    initialised.current = true;
    setModule({
      print: (...args: unknown[]) => {
        console.log(...args);
      },
      canvas,
      setStatus(text: string) {
        console.log(text);
        setStatus(text);
      },
      monitorRunDependencies(left: number) {
        if (left == 0) {
          setShowSpinner(false);
          setStatus("All downloads complete.");
          return;
        }
        let total = store.get(progressTotalAtom);
        if (total === 0) {
          total = left;
          setProgressTotal(total);
        }
        setProgress(total - left);
        setProgressTotal(left);
        setStatus("Preparing... (" + (total - left) + "/" + total + ")");
      },
      onRuntimeInitialized: () => {
        // store.get(moduleInstanceAtom).resize(window.innerWidth, window.innerHeight);
      }
    });
    // As a default initial behavior, pop up an alert when webgl context is lost. To make your
    // application robust, you may want to override this behavior before shipping!
    // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
    canvasRef.current.addEventListener(
      "webglcontextlost",
      (e) => {
        alert("WebGL context lost. You will need to reload the page.");
        e.preventDefault();
      },
      false
    );
    window.onerror = (event) => {
      // TODO: do not warn on ok events like simulating an infinite loop or exitStatus
      setHasError(true);
      console.error(event);
      setStatus("Exception thrown, see JavaScript console");
    };
    window.addEventListener("resize", ()=> {
      // store.get(moduleInstanceAtom).resize(window.innerWidth, window.innerHeight);
    })
    const script = document.createElement("script");

    script.async = true;
    script.src = "/crawl.js";

    document.body.appendChild(script);
  }, [
    canvasRef,
    setHasError,
    setModule,
    setProgress,
    setProgressTotal,
    setShowSpinner,
    setStatus,
    store,
  ]);
};
