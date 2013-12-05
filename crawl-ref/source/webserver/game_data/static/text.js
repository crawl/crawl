define(["jquery", "comm"], function ($, comm) {
    var line_span = $("<span>");
    line_span.css("white-space", "pre");

    function get_text_area_line(name, line)
    {
        var area = $("#" + name);
        var lines = $("#" + name + " > span");
        for (var i = lines.length; i <= line; ++i)
        {
            if (i != 0)
                area.append("<br>");
            area.append(line_span.clone());
        }
        return $("#" + name + " > span").eq(line);
    }

    function set_text_area_line(name, line, content)
    {
        var span = get_text_area_line(name, line);
        span.html(content);
    }

    function handle_text_update(data)
    {
        if (data.clear)
        {
            var lines = $("#" + data.id + " > span");
            for (var i = 0; i < lines.length; ++i)
            {
                if (!(i in data.lines))
                    lines.eq(i).empty();
            }
        }
        for (var line in data.lines)
            set_text_area_line(data.id, line, data.lines[line]);
        $("#" + data.id).trigger("text_update");
    }

    comm.register_handlers({
        "txt": handle_text_update
    });
});
