
function prepareCell(element, x, y) {
    $element = $(element);
    lineLengths = $element.data("lineLengths");
    if (lineLengths == undefined) {
        lineLengths = [];
        $element.data("lineLengths", lineLengths);
    }
    id = element.id;
    html = "";
    while (lineLengths.length <= y) {
        html += "<br id=\"" + id + "-end-" + lineLengths.length + "\">";
        lineLengths.push(0);
    }
    if (html != "")
        $element.append(html);
    lineAdd = "";
    while (lineLengths[y] <= x) {
        lineAdd += "<span id=\"" + id + "-" + lineLengths[y] + "-" + y + "\">&nbsp;</span>";
        lineLengths[y]++;
    }
    if (lineAdd != "")
        $("#" + id + "-end-" + y).before(lineAdd);
}

function setCellStyle(element, x, y, attr, value) {
    prepareCell(element, x, y);
     $("#" + id + "-" + x + "-" + y).css(attr, value);
}

function putChar(element, x, y, content) {
    prepareCell(element, x, y);
    if (content == " ") content = "&nbsp;";
    $("#" + id + "-" + x + "-" + y).html(content);
}

function putString(element, x, y, string) {

}

function clearTextArea(element) {
    $element = $(element);
    $element.data("lineLengths", []);
    $element.html("");
}
