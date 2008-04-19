import struct

class reader(object):
    """Wrapper around a read-only binary file"""
    def __init__(self, fileorname):
        """fileorname may be a file-like object or a filename"""
        self.byteorder = '='    # can also be '<' or '>'
        if hasattr(fileorname, 'read'):
            self.file = fileorname
        else:
            self.file = file(fileorname, 'rb')

    def read(self,len): return self.file.read(len)

    def stream(self, fmt, len=0):
        """Unpack fmt from f and return results."""
        fmt = self.byteorder + fmt                 # don't enforce alignment!
        if len == 0: len = struct.calcsize(fmt)
        data = self.file.read(len)
        return struct.unpack(fmt, data)

    def stream1(self, fmt, len=0):
        """Unpack fmt from f and return just one result."""
        (data,) = self.stream(fmt,len)
        return data

    def _streamStringLow(self, len):
        if len <= 0: return ''
        str = self.file.read(len-1)
        char = self.file.read(1)
        if ord(char)==0: return str
        else: return str+char

    def streamCooky(self):
        l = list(self.stream("4c"))
        l.reverse()
        return ''.join(l)
        
    def streamString4(self):
        """Stream string prefixed with 4 bytes of length"""
        (strlen,) = self.stream("I",4)
        if strlen >= 2048:
            print "Bad string len: %#x" % strlen
            assert strlen < 2048
        return self._streamStringLow(strlen)

    def streamString2(self):
        """Stream string prefixed with 2 bytes of length"""
        (strlen,) = self.stream("H",2)
        assert strlen < 2048
        return self._streamStringLow(strlen)

    def streamString1(self):
        """Stream string prefixed with 1 byte of length"""
        (strlen,) = self.stream("B",1)
        assert strlen < 2048
        return self._streamStringLow(strlen)
    
class writer(object):
    """Wrapper around a write-only binary file"""
    def __init__(self, fileorname):
        """fileorname may be a file-like object or a filename"""
        if hasattr(fileorname, 'write'):
            self.file = fileorname
        else:
            self.file = file(fileorname, 'wb')

    def write(self,data): return self.file.write(data)
    def stream(self, fmt, *args):
        fmt = '='+fmt                   # add the "no alignment" flag to the fmt
        self.file.write(struct.pack(fmt, *args))
    # for symmetry with reader
    def stream1(self, fmt, *args):
        fmt = '='+fmt                   # add the "no alignment" flag to the fmt
        self.file.write(struct.pack(fmt, *args))

    def streamCooky(self, cooky):
        assert len(cooky)==4
        l = list(cooky)
        l.reverse()
        self.stream("4c", *l)
        
    def streamString1(self, str):
        """Stream string prefixed with 1 byte of length"""
        #print "writing %s" % str
        # Game doesn't handle 0-length null strings
        self.file.write(struct.pack("B",len(str)+1))
        self.file.write(str)
        self.file.write("\0")

    def streamString2(self, str):
        """Stream string prefixed with 2 bytes of length"""
        #print "writing %s" % str
        # Game doesn't handle 0-length null strings
        self.file.write(struct.pack("H",len(str)+1))
        self.file.write(str)
        self.file.write("\0")

    def streamString4(self, str):
        """Stream string prefixed with 4 bytes of length"""
        if str=='':
            self.file.write(struct.pack("I",0))
        else:
            #print "writing %s" % str
            self.file.write(struct.pack("I",len(str)+1))
            self.file.write(str)
            self.file.write("\0")
