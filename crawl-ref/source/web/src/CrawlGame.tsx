import styled from "@emotion/styled";
import { useAtom } from "jotai";
import { useRef } from "react";
import { showSpinnerAtom } from "./atoms/state";
import { Spinner } from "./Spinner";
import { useModule } from "./Module";

const GameCanvas = styled.canvas`
  border: 0px none;
  background-color: black;
`;

export const CrawlGame = () => {
  const canvasRef = useRef<HTMLCanvasElement>(null!);
  const [showSpinner] = useAtom(showSpinnerAtom);

  useModule(canvasRef);
  return (
    <>
      <GameCanvas
        onContextMenu={(e) => {
          e.preventDefault();
        }}
        tabIndex={-1}
        ref={canvasRef}
        className="emscripten"
        id="canvas"
      ></GameCanvas>
      {showSpinner && <Spinner />}
    </>
  );
};
