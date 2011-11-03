define(["jquery", "comm"], function ($, comm) {
    function get_text_area_line(name, line)
    {
        var area = $("#" + name);
        var current_line = line;
        var append = "";
        do
        {
            var line_span = $("#" + name + "-" + current_line);
            if (line_span[0])
                break;

            append = "<span style='white-space: pre;' id='" + name +
                "-" + current_line + "'></span>" + append;

            if (current_line == 0)
            {
                // first line didn't exist, so this wasn't handled by this function before
                // -> clean up first
                $("#" + name).html("");
                break;
            }
            else
            {
                append = "<br>" + append;
            }

            current_line--;
        }
        while (true);

        if (append !== "")
            $("#" + name).append(append);

        return $("#" + name + "-" + line);
    }

    function set_text_area_line(name, line, content)
    {
        get_text_area_line(name, line).html(content);
    }

    function handle_text_update(data)
    {
        for (var line in data.lines)
            set_text_area_line(data.id, line, data.lines[line]);
    }

    comm.register_handlers({
        "txt": handle_text_update
    });
});
