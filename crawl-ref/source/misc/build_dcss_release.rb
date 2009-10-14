# build_dcss_release.rb
#   Builds DOS and Windows binaries for a Stone Soup release.
# Needs rubyzip to be installed.

require 'fileutils'
require 'zip/zipfilesystem'

SVN_BASE_URL    = 'https://crawl-ref.svn.sourceforge.net/svnroot/crawl-ref/'
SVN_BRANCH      = 'branches/stone_soup-0.3'
SVN_URL         = SVN_BASE_URL + SVN_BRANCH + '/crawl-ref'

# If empty, nothing is done. Useful to sync svk mirrors.
SVN_PRESYNC     = ''

SVN             = 'svn'

BUILDS          = [ Proc.new { build_win32 }, 
                    Proc.new { build_dos } ].reverse

W32MAKE         = 'mingw32-make'
DOSMAKE         = 'make'

UPX             = 'upx'

SVN_CO_DIR      = 'stone_soup'

DEV_NULL        = 'nul'

CLEANPATH       = ENV['PATH']
# Leave empty if svn is already in the path
SVN_PATH_PREFIX = 'C:\etc\misc\svk\bin;C:\etc\misc\svn\bin;'

W32MAKE_PATH_PREFIX = 'C:\etc\MinGW\bin;C:\etc\misc\yacc;'
DOSMAKE_PATH_PREFIX = 'C:\etc\djgpp\bin;'

BUILD_HOME      = File.join( FileUtils.pwd(), 'release' )

# Relative to source directory
CRAWL_NDB_PATH  = 'rel/crawl.exe'
CRAWL_DBG_PATH  = 'dbg/crawl.exe'
CRAWL_DOS_PATH  = 'crawl.exe'

PACKAGE_PATH    = BUILD_HOME

def build_soup
  checkout
  make
  package
end

def setup_svn_env
  ENV['PATH'] = SVN_PATH_PREFIX + CLEANPATH
end

def setup_build_home
  FileUtils.mkdir_p(BUILD_HOME)
  Dir.chdir(BUILD_HOME)
end

def full_checkout
  puts "#{SVN} co #{SVN_URL} #{SVN_CO_DIR}"
  system "#{SVN} co #{SVN_URL} #{SVN_CO_DIR}" or 
          raise "#{SVN} co failed: #$?"
  Dir.chdir SVN_CO_DIR
end

def checkout
  setup_svn_env

  if SVN_PRESYNC and not SVN_PRESYNC.empty?
    puts "Running presync: '#{SVN_PRESYNC}'"
    system(SVN_PRESYNC) or raise "#{SVN_PRESYNC} failed: #$?!"
  end

  setup_build_home

  clean_directory SVN_CO_DIR if File.directory? SVN_CO_DIR
  full_checkout
end

def clean_directory(*dirs)
  dirs.each do |dir|
    currdir = FileUtils.pwd
    if currdir != BUILD_HOME
      raise "In #{currdir}, need to be in #{BUILD_HOME}!"
    end
    raise "Evil directory name: #{dir}" if dir =~ /\.\./

    puts "Trying to remove #{dir}"
    FileUtils.rm_r dir, :verbose => true
  end
end

def path_prefix(prefix)
  ENV['PATH'] = prefix + CLEANPATH unless ENV['PATH'].index(prefix)
end

def clean_objects
  [ '.', 'rel', 'dbg', 'util', 'contrib/lua/src', 'contrib/sqlite' ].each do |dir|
    if File.directory? dir
      FileUtils.rm( Dir[dir + '/*.o'], :force => true )
      FileUtils.rm( Dir[dir + '/*.a'], :force => true )
    end
  end
end

def clean_w32build_area
  clean_objects
  system "#{W32MAKE} -f makefile.mgw clean"
end

def clean_dosbuild_area
  clean_objects
  system "#{DOSMAKE} -f makefile.dos clean"
end

def make
  BUILDS.each do |builder|
    Dir.chdir( File.join(BUILD_HOME, SVN_CO_DIR) )
    builder.call
  end
end

def setup_w32make_env
  Dir.chdir 'source'
  path_prefix W32MAKE_PATH_PREFIX
  clean_w32build_area
end

def setup_dosmake_env
  Dir.chdir 'source'
  path_prefix DOSMAKE_PATH_PREFIX
  clean_dosbuild_area
end

$release_version = nil
def release_version
  if not $release_version
    raise "Can't find version.h" unless File.file? 'version.h'
    IO.readlines('version.h').each do |line|
      if line =~ /VER_NUM\s+"(\d\.\d(?:\.\d)?)/
        $release_version = $1
      end
      if line =~ /VER_QUAL\s+"(.*)"/
        if $1 =~ /-svn/
          raise "Version number has -svn suffix! Remove it to proceed"
        end
      end
    end
  end
  $release_version or raise "Unable to read version at #{FileUtils.pwd}"
end

def build_dos
  setup_dosmake_env

  puts "\nBuilding stone_soup (ndebug) for #{release_version} DOS"
  ENV['LIB'] = "-static -Lcontrib\\lua\\src -llua -lpcre -Lcontrib\\sqlite -lsql3"
  ENV['EXTRA_FLAGS'] = "-O2 -DCLUA_BINDINGS -DREGEX_PCRE -DDEBUG -DWIZARD"

  puts %{ #{DOSMAKE} -e -f makefile.dos DOYACC=y }
  system( %{ #{DOSMAKE} -e -f makefile.dos DOYACC=y } ) or
            raise "#{DOSMAKE} failed: #$?"

  upx CRAWL_DOS_PATH
end

def build_win32
  setup_w32make_env
  
  puts "\nBuilding stone_soup (non-debug) for #{release_version} release!"
  system( %{#{W32MAKE} -f makefile.mgw DOYACC=y "EXTRA_FLAGS=-O2 } +
         %{-DCLUA_BINDINGS -DREGEX_PCRE -DDEBUG -DWIZARD" } +
         %{"LIB=-lwinmm -static -Lcontrib/lua/src -llua -lpcre -Lcontrib/sqlite -lsqlite3"} ) or
            raise "#{W32MAKE} failed: #$?"

  clean_w32build_area

  #puts "\nBuilding stone_soup (debug) for #{release_version}!"
  #system( %{#{W32MAKE} -f makefile.mgw debug DEBUG_CRAWL=y "EXTRA_FLAGS=-O2 } +
  #       %{-DCLUA_BINDINGS -DREGEX_PCRE -DFULLDEBUG -DWIZARD" } +
  #       %{"LIB=-lwinmm -static -llua -lpcre" } +
  #       %{DOYACC=y} ) or
  #          raise "#{W32MAKE} failed: #$?"

  #upx CRAWL_NDB_PATH, CRAWL_DBG_PATH
  upx CRAWL_NDB_PATH
end

def upx(*files)
  return unless UPX and not UPX.empty?
  files.each do |file|
    system "#{UPX} #{file}" or raise "#{UPX} failed: #$?"
  end
end

def makezip(path, name, exe)
  zipname = File.join(path, name) + '.zip'

  puts "Creating zip archive at #{zipname}"
  if File.exists? zipname
    FileUtils.rm(zipname)
  end
  Zip::ZipFile.open( zipname, Zip::ZipFile::CREATE ) do |zip|
    zip.dir.mkdir(name)
    
    zip.dir.chdir name

    # The exe itself
    zip.add 'source/' + exe

    # Add base documentation
    zip.add [ 'CREDITS', 'licence.txt', 'readme.txt' ] + Dir['README*']
    
    # Add base config
    zip.add [ 'init.txt', 'macro.txt' ]

    zip.add( Dir['docs/*'].find_all { |f| not File.directory?(f) },
            :keep_paths => true )

    [ 'lua', 'dat', 'dat/clua', 'dat/descript' ].each do |dir|
      zip.add( Dir['source/' + dir + '/*'], :prefix => dir )
    end
  end
end

def package
  Dir.chdir( File.join(BUILD_HOME, SVN_CO_DIR) )

  # Wipe all existing zips!
  FileUtils.rm( Dir[ File.join(PACKAGE_PATH, '*.zip') ] )

  [ [ "stone_soup-#{release_version}-win32", CRAWL_NDB_PATH ],
    # [ "stone_soup-#{release_version}-win32-debug", CRAWL_DBG_PATH ],
    [ "ss#{release_version.tr '.', ''}dos", CRAWL_DOS_PATH ]
  ].
    each do |pkg, exe|

    makezip(PACKAGE_PATH, pkg, exe)
  end
end

##########################################################################
# Zip extensions

class Zip::ZipFile
  def mkdir_p(dirpath)
    dirpath.tr! '\\', '/'
    segments = File.split(dirpath)
    fullpath = nil
    segments.each do |dir|
      next if dir == '.'
      fullpath = fullpath ? File.join(fullpath, dir) : dir
      self.dir.mkdir fullpath unless self.file.directory? fullpath
    end
  end

  def add(files, options = {})
    files = [ files ] unless files.respond_to? :to_ary
    files.each do |f|
      entryname = options[:keep_paths]? f : File.basename(f)
      entryname = File.join(options[:prefix], entryname) if options[:prefix]

      if File.directory? f
        # We DON'T add the contents of the directory automagically
        self.mkdir_p entryname
      else
        dirname = File.dirname(entryname)
        self.mkdir_p(dirname) unless dirname == '.'

        self.file.open(entryname, 'w') do |outf|
          File.open(f, 'rb') do |inf|
            outf.write( inf.read )
          end
        end
      end
    end
  end
end

build_soup
