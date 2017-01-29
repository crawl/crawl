#!/usr/bin/env fish

set backup_orig_name (find (pwd) -name '*.backup' | awk -F'.' '{ $NF=""; OFS="."; print $0 }' | sed 's/\.$//')

#echo $backup_orig_name
for orig_name in $backup_orig_name
        mv "$orig_name.backup" "$orig_name"
end
