package org.develz.crawl;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

final class DCSSXlogReader {
    private DCSSXlogReader() {}

    static Map<String, Map<String, String>> readByMorgueTimestamp(File logfile) {
        Map<String, Map<String, String>> records = new LinkedHashMap<>();
        Set<String> ambiguous = new HashSet<>();
        if (!logfile.isFile())
            return records;
        try (BufferedReader reader = new BufferedReader(new FileReader(logfile))) {
            String line;
            while ((line = reader.readLine()) != null) {
                Map<String, String> record = parse(line);
                String timestamp = morgueTimestamp(record.get("end"));
                if (timestamp != null && !ambiguous.contains(timestamp)) {
                    if (records.put(timestamp, record) != null) {
                        records.remove(timestamp);
                        ambiguous.add(timestamp);
                    }
                }
            }
        } catch (IOException e) {
            return records;
        }
        return records;
    }

    private static String morgueTimestamp(String xlogEnd) {
        if (xlogEnd == null || xlogEnd.length() < 14)
            return null;
        try {
            int month = Integer.parseInt(xlogEnd.substring(4, 6)) + 1;
            String paddedMonth = month < 10 ? "0" + month : String.valueOf(month);
            return xlogEnd.substring(0, 4) + paddedMonth + xlogEnd.substring(6, 14);
        } catch (NumberFormatException e) {
            return null;
        }
    }

    private static Map<String, String> parse(String line) {
        Map<String, String> fields = new LinkedHashMap<>();
        StringBuilder field = new StringBuilder();
        for (int index = 0; index < line.length(); index++) {
            char character = line.charAt(index);
            if (character == ':') {
                if (index + 1 < line.length() && line.charAt(index + 1) == ':') {
                    field.append(':');
                    index++;
                } else {
                    addField(fields, field.toString());
                    field.setLength(0);
                }
            } else {
                field.append(character);
            }
        }
        addField(fields, field.toString());
        return fields;
    }

    private static void addField(Map<String, String> fields, String field) {
        int equals = field.indexOf('=');
        if (equals > 0)
            fields.put(field.substring(0, equals), field.substring(equals + 1));
    }
}
