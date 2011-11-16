import re
import logging
import tornado.template
import os.path
import time

class TornadoFilter(logging.Filter):
    def filter(self, record):
        if record.module == "web" and record.levelno <= logging.INFO:
            return False
        return True

class DynamicTemplateLoader(tornado.template.Loader):
    def __init__(self, root_dir):
        tornado.template.Loader.__init__(self, root_dir)

    def load(self, name, parent_path=None):
        name = self.resolve_path(name, parent_path=parent_path)
        if name in self.templates:
            template = self.templates[name]
            path = os.path.join(self.root, name)
            if os.path.getmtime(path) > template.load_time:
                del self.templates[name]
            else:
                return template

        template = super(DynamicTemplateLoader, self).load(name, parent_path)
        template.load_time = time.time()
        return template

    _instances = {}

    @classmethod
    def get(cls, path):
        if path in cls._instances:
            return cls._instances[path]
        else:
            l = DynamicTemplateLoader(path)
            cls._instances[path] = l
            return l

def dgl_format_str(s, username, game_params):
    s = s.replace("%n", username)

    return s

where_entry_regex = re.compile("(?<=[^:]):(?=[^:])")

def parse_where_data(data):
    where = {}

    for entry in where_entry_regex.split(data):
        if entry.strip() == "": continue
        field, _, value = entry.partition("=")
        where[field.strip()] = value.strip().replace("::", ":")
    return where
