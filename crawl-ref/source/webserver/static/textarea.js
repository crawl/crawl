var textAreaFont = "16px monospace";
var textAreaCellWidth, textAreaCellHeight = 17;

function putChar(element, cx, cy, content, fg, bg) {
    cx--; cy--; // Text area coordinates are 1-based
    x = cx * textAreaCellWidth;
    y = cy * textAreaCellHeight;
    ctx = element.getContext("2d");
    ctx.fillStyle = termColours[bg];
    ctx.fillRect(x, y, textAreaCellWidth, textAreaCellHeight);
    ctx.fillStyle = termColours[fg];
    ctx.font = textAreaFont;
    ctx.textAlign = "start";
    ctx.textBaseline = "top";
    ctx.fillText(content, x, y);
}

function clearTextArea(element) {
    ctx = element.getContext("2d");
    ctx.fillStyle = "black";
    ctx.fillRect(0, 0, element.width, element.height);
}

function textAreaSize(element, cols, rows) {
    console.log("Resizing " + element.id + " to " + cols + "x" + rows);
    if (textAreaCellWidth == undefined) {
        ctx = element.getContext("2d");
        ctx.font = textAreaFont;
        metrics = ctx.measureText("@");
        textAreaCellWidth = metrics.width;
    }
    element.width = cols * textAreaCellWidth;
    element.height = rows * textAreaCellHeight;
    clearTextArea(element);
}
