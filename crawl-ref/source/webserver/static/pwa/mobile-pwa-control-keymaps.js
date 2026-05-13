"use strict";

function movementKey(label, input, direction)
{
    return {
        label: label + direction,
        input: input,
        className: "is-move",
        ariaLabel: label + " " + direction
    };
}

function inputKey(label, input, className)
{
    return {
        label: label,
        input: input || label,
        className: className || ""
    };
}

function keycodeKey(label, keycode, className, span)
{
    return {
        label: label,
        keycode: keycode,
        className: className || "is-action",
        span: span || 1
    };
}

function actionKey(label, action, className, span)
{
    return {
        label: label,
        action: action,
        className: className || "is-system",
        span: span || 1
    };
}

export function createControlKeymaps() {
    return {
        actionKey: actionKey,
        keyboardRows: [
            [
                inputKey("q"), inputKey("w"), inputKey("e"),
                inputKey("r"), inputKey("t"),
                movementKey("y", "y", "↖"),
                movementKey("u", "u", "↗"), inputKey("i"),
                inputKey("o"), inputKey("p")
            ],
            [
                inputKey("a"), inputKey("s"), inputKey("d"),
                inputKey("f"), inputKey("g"),
                movementKey("h", "h", "←"),
                movementKey("j", "j", "↓"),
                movementKey("k", "k", "↑"),
                movementKey("l", "l", "→"), keycodeKey("⌫", 8)
            ],
            [
                keycodeKey("⇥", 9), inputKey("z"), inputKey("x"),
                inputKey("c"), inputKey("v"),
                movementKey("b", "b", "↙"),
                movementKey("n", "n", "↘"), inputKey("m"),
                inputKey(";"), inputKey("."), inputKey("=")
            ],
            [
                actionKey("⇧", "shift", "is-modifier", 2),
                actionKey("Ctrl", "ctrl", "is-modifier", 2),
                keycodeKey("Esc", 27, "is-action"),
                inputKey("5"),
                actionKey("Pad", "compact", "is-system", 2),
                keycodeKey("↵", 13, "is-enter", 2),
                actionKey("123", "symbols", "is-modifier", 2)
            ]
        ],
        symbolRows: [
            [
                inputKey("1"), inputKey("2"), inputKey("3"),
                inputKey("4"), inputKey("5"), inputKey("6"),
                inputKey("7"), inputKey("8"), inputKey("9"),
                inputKey("0")
            ],
            [
                inputKey("<"), inputKey(">"), inputKey("["),
                inputKey("]"), inputKey("{"), inputKey("}"),
                inputKey("/"), inputKey("*"), inputKey("="),
                keycodeKey("⌫", 8)
            ],
            [
                keycodeKey("⇥", 9), inputKey("!"), inputKey("?"),
                inputKey(":"), inputKey("\""), inputKey(","),
                inputKey("."), inputKey("_"), inputKey("@"),
                inputKey("#")
            ],
            [
                actionKey("⇧", "shift", "is-modifier", 2),
                actionKey("Ctrl", "ctrl", "is-modifier", 2),
                keycodeKey("Esc", 27, "is-action"),
                inputKey("-"), inputKey("+"),
                actionKey("Pad", "compact", "is-system", 2),
                keycodeKey("↵", 13, "is-enter", 2),
                actionKey("ABC", "symbols", "is-modifier", 2)
            ]
        ],
        compactRows: [
            [
                movementKey("y", "y", "↖"),
                movementKey("k", "k", "↑"),
                movementKey("u", "u", "↗"), inputKey("o"),
                keycodeKey("Esc", 27, "is-action"), inputKey(".")
            ],
            [
                movementKey("h", "h", "←"),
                inputKey("5", "5", "is-move"),
                movementKey("l", "l", "→"), inputKey("g"),
                inputKey("<"), inputKey(">")
            ],
            [
                movementKey("b", "b", "↙"),
                movementKey("j", "j", "↓"),
                movementKey("n", "n", "↘"), inputKey("i"),
                keycodeKey("Tab", 9, "is-action"),
                actionKey("ABC", "keyboard", "is-modifier")
            ]
        ]
    };
}
