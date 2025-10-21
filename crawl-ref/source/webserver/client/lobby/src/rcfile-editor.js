import "./rcfile-editor.css";
import completions from "./rcfile-completions.json";
import "ace-builds/src-noconflict/ace.js";
import "ace-builds/src-noconflict/theme-tomorrow_night_bright.js";
import "ace-builds/src-noconflict/mode-lua.js";
import "ace-builds/src-noconflict/ext-language_tools.js";

document.querySelector("#rc_edit").style.zIndex = 9999;
const editorParent = document.querySelector("#rc_edit_form");
const textarea = editorParent.querySelector("textarea");
ace.config.set(
  "basePath",
  "https://cdn.jsdelivr.net/npm/ace-builds@1.43.4/src-noconflict",
);
const customCompleter = {
  getCompletions: (editor, session, pos, prefix, callback) => {
    if (prefix.length === 0) {
      return callback(null, []);
    }
    callback(
      null,
      completions.map((c) => ({ ...c, score: 9999 })),
    );
  },
};
ace.require("ace/ext/language_tools").addCompleter(customCompleter);

document.querySelector("#rc_edit").style.zIndex = 9999;

const editorDiv = document.createElement("div");
editorDiv.id = "editor";
editorParent.prepend(editorDiv);
const br = editorParent.querySelector("br");
const helpSpan = document.createElement("span");
const downloadButton = document.createElement("input");
downloadButton.classList.add("button");
downloadButton.value = "Download";
downloadButton.type = "button";
downloadButton.style.float = "right";
downloadButton.style.marginRight = "5px";
downloadButton.onclick = () => {
  const rcfile = this.editor.getValue();
  const name = `${SiteInformation.current_user}.rc`;
  const blob = new Blob([rcfile], { type: "text/plain" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = name;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
};
helpSpan.textContent = "F11: Toggle FullScreen Mode";
br.insertAdjacentElement("afterend", helpSpan);
editorParent.append(downloadButton);

let editor;

async function initialise(data) {
  textarea.style.display = "";
  const rcfile = data.contents;
  editor?.destroy?.();
  editor = ace.edit("editor");

  editor.setTheme("ace/theme/tomorrow_night_bright");
  editor.session.setMode("ace/mode/lua");
  editor.session.setOption("useWorker", false);

  editor.setOptions({
    enableBasicAutocompletion: true,
    enableLiveAutocompletion: true,
    enableSnippets: true,
    wrap: true,
  });

  editor.setValue(rcfile);

  const rect = textarea.getBoundingClientRect();
  editorDiv.style.width = `${rect.width}px`;
  editorDiv.style.height = `${rect.height}px`;
  editor.resize();

  editor.renderer.once("afterRender", () => {
    const lastRow = editor.session.getLength() - 1;
    const lastColumn = editor.session.getLine(lastRow).length;
    editor.gotoLine(lastRow + 1, lastColumn);
    editor.renderer.scrollCursorIntoView();
    editor.focus();
  });

  editor.commands.addCommand({
    name: "Toggle Fullscreen",
    bindKey: { win: "F11", mac: "F11" },
    exec: (editor) => {
      const isFullScreen = document.body.classList.toggle("fullScreen");
      editor.container.classList.toggle("fullScreen", isFullScreen);
      document
        .getElementById("editor")
        .classList.toggle("fullScreen", isFullScreen);
      editor.resize();
    },
  });

  textarea.style.display = "none";

  if (this.urlRCFile) {
    try {
      const fetched = await fetch("https://rc-proxy.nemelex.cards", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ url: this.urlRCFile }),
      }).then((r) => r.text());
      editor.setValue(fetched);
      editor.renderer.updateFull(true);
      const lastRow = editor.session.getLength() - 1;
      const lastColumn = editor.session.getLine(lastRow).length;
      editor.gotoLine(lastRow + 1, lastColumn);
      editor.renderer.scrollCursorIntoView();
      editor.focus();
    } catch (e) {
      console.error(e);
    }
  }

  if (this.urlAppend) {
    const Range = ace.require("ace/range").Range;
    const lastRow = editor.session.getLength() - 1;
    const lastColumn = editor.session.getLine(lastRow).length;
    editor.moveCursorTo(lastRow, lastColumn);
    this.Range = ace.require("ace/range").Range;
    editor.renderer.updateFull(true);
    editor.insert("\n" + this.urlAppend);
    const endRow = editor.session.getLength() - 1;
    const endColumn = editor.session.getLine(endRow).length;
    editor.selection.setSelectionRange(
      new Range(lastRow + 1, 0, endRow, endColumn),
    );
    editor.renderer.scrollCursorIntoView();
    editor.focus();
  }
  this.urlRCFile = this.urlAppend = undefined;
  Array.from(document.querySelectorAll(".edit_rc_link")).forEach(
    (e) => (e.style.color = ""),
  );
}

export { initialise };
