import { atom } from "jotai";

export const statusAtom = atom("Downloading...");

export const showSpinnerAtom = atom(true);

export const progressAtom = atom(0)

export const progressTotalAtom = atom(0)

export const hasErrorAtom = atom(false)
