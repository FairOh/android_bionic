# common python utility routines for the Bionic tool scripts

import sys, os, commands, string

# support Bionic architectures, add new ones as appropriate
#
bionic_archs = [ "arm", "x86", "mips" ]

# basic debugging trace support
# call D_setlevel to set the verbosity level
# and D(), D2(), D3(), D4() to add traces
#
verbose = 0

def D(msg):
    global verbose
    if verbose > 0:
        print msg

def D2(msg):
    global verbose
    if verbose >= 2:
        print msg

def D3(msg):
    global verbose
    if verbose >= 3:
        print msg

def D4(msg):
    global verbose
    if verbose >= 4:
        print msg

def D_setlevel(level):
    global verbose
    verbose = level


def find_dir_of(path):
    '''return the directory name of 'path', or "." if there is none'''
    # remove trailing slash
    if len(path) > 1 and path[-1] == '/':
        path = path[:-1]

    # find parent directory name
    d = os.path.dirname(path)
    if d == "":
        return "."
    else:
        return d

#  other stuff
#
#
def find_file_from_upwards(from_path,target_file):
    """find a file in the current directory or its parents. if 'from_path' is None,
       seach from the current program's directory"""
    path = from_path
    if path == None:
        path = find_dir_of(sys.argv[0])
        D("this script seems to be located in: %s" % path)

    while 1:
        if path == "":
            path = "."

        file = path + "/" + target_file
        D("probing "+file)

        if os.path.isfile(file):
            D("found %s in %s" % (target_file, path))
            return file

        if path == ".":
            break

        path = os.path.dirname(path)

    path = ""
    while 1:
        path = "../" + path
        file = path + target_file
        D("probing "+file)

        if os.path.isfile(file):
            D("found %s in %s" % (target_file, path))
            return file


    return None

def find_bionic_root():
    '''find the root of the Bionic source tree. we check for the SYSCALLS.TXT file
       from the location of the current program's directory.'''

    # note that we can't use find_file_from_upwards() since we can't use os.path.abspath
    # that's because in some cases the p4 client is in a symlinked directory, and this
    # function will return the real path instead, which later creates problems when
    # p4 commands are issued
    #
    file = find_file_from_upwards(None, "SYSCALLS.TXT")
    if file:
        return os.path.dirname(file)
    else:
        return None

def find_original_kernel_headers():
    """try to find the directory containing the original kernel headers"""
    bionic_root = find_bionic_root()
    if not bionic_root:
        D("Could not find Bionic root !!")
        return None

    path = os.path.normpath(bionic_root + "/../../external/kernel-headers/original")
    if not os.path.isdir(path):
        D("Could not find %s" % (path))
        return None

    return path

def find_kernel_headers():
    """try to find the directory containing the kernel headers for this machine"""

    # First try to find the original kernel headers.
    ret = find_original_kernel_headers()
    if ret:
        D("found original kernel headers in: %s" % (ret))
        return ret

    status, version = commands.getstatusoutput( "uname -r" )  # get Linux kernel version
    if status != 0:
        D("could not execute 'uname -r' command properly")
        return None

    # get rid of the "-xenU" suffix that is found in Xen virtual machines
    if len(version) > 5 and version[-5:] == "-xenU":
        version = version[:-5]

    path = "/usr/src/linux-headers-" + version + "/include"
    D("probing %s for kernel headers" % (path))
    ret = os.path.isdir( path )
    if ret:
        D("found kernel headers in: %s" % (path))
        return path
    return None

def find_arch_header(kernel_headers,arch,header):
    # First, try in <root>/arch/<arm>/include/<header>
    # corresponding to the location in the kernel source tree for
    # certain architectures (e.g. arm).
    path = "%s/arch/%s/include/asm/%s" % (kernel_headers, arch, header)
    D("Probing for %s" % path)
    if os.path.exists(path):
        return path

    # Try <root>/asm-<arch>/include/<header> corresponding to the location
    # in the kernel source tree for other architectures (e.g. x86).
    path = "%s/include/asm-%s/%s" % (kernel_headers, arch, header)
    D("Probing for %s" % path)
    if os.path.exists(path):
        return path

    # Otherwise, look under <root>/asm-<arch>/<header> corresponding
    # the original kernel headers directory
    path = "%s/asm-%s/%s" % (kernel_headers, arch, header)
    D("Probing for %s" % path)
    if os.path.exists(path):
        return path


    return None

# parser for the SYSCALLS.TXT file
#
class SysCallsTxtParser:
    def __init__(self):
        self.syscalls = []
        self.lineno   = 0

    def E(self, msg):
        print "%d: %s" % (self.lineno, msg)

    def parse_line(self, line):
        """ parse a syscall spec line.

        line processing, format is
           return type    func_name[:syscall_name[:call_id]] ( [paramlist] )   architecture_list
        """
        pos_lparen = line.find('(')
        E          = self.E
        if pos_lparen < 0:
            E("missing left parenthesis in '%s'" % line)
            return

        pos_rparen = line.rfind(')')
        if pos_rparen < 0 or pos_rparen <= pos_lparen:
            E("missing or misplaced right parenthesis in '%s'" % line)
            return

        return_type = line[:pos_lparen].strip().split()
        if len(return_type) < 2:
            E("missing return type in '%s'" % line)
            return

        syscall_func = return_type[-1]
        return_type  = string.join(return_type[:-1],' ')
        call_id = -1

        pos_colon = syscall_func.find(':')
        if pos_colon < 0:
            syscall_name = syscall_func
        else:
            if pos_colon == 0 or pos_colon+1 >= len(syscall_func):
                E("misplaced colon in '%s'" % line)
                return

            # now find if there is a call_id for a dispatch-type syscall
            # after the optional 2nd colon
            pos_colon2 = syscall_func.find(':', pos_colon + 1)
            if pos_colon2 < 0:
                syscall_name = syscall_func[pos_colon+1:]
                syscall_func = syscall_func[:pos_colon]
            else:
                if pos_colon2+1 >= len(syscall_func):
                    E("misplaced colon2 in '%s'" % line)
                    return
                syscall_name = syscall_func[(pos_colon+1):pos_colon2]
                call_id = int(syscall_func[pos_colon2+1:])
                syscall_func = syscall_func[:pos_colon]

        if pos_rparen > pos_lparen+1:
            syscall_params = line[pos_lparen+1:pos_rparen].split(',')
            params         = string.join(syscall_params,',')
        else:
            syscall_params = []
            params         = "void"

        # Parse the architecture list.
        syscall_common = -1
        syscall_arm  = -1
        syscall_x86 = -1
        syscall_mips = -1
        arch_list = line[pos_rparen+1:].strip()
        if arch_list == "custom":
            pass
        elif arch_list == "all":
            syscall_common = 1
        else:
            for arch in string.split(arch_list, ','):
                if arch == "arm":
                    syscall_arm = 1
                elif arch == "x86":
                    syscall_x86 = 1
                elif arch == "mips":
                    syscall_mips = 1
                else:
                    E("invalid syscall architecture list in '%s'" % line)
                    return

        global verbose
        if verbose >= 2:
            if call_id == -1:
                if syscall_common == -1:
                    print "%s: %d,%d,%d" % (syscall_name, syscall_arm, syscall_x86, syscall_mips)
                else:
                    print "%s: %d" % (syscall_name, syscall_common)
            else:
                if syscall_common == -1:
                    print "%s(%d): %d,%d,%d" % (syscall_name, call_id, syscall_arm, syscall_x86, syscall_mips)
                else:
                    print "%s(%d): %d" % (syscall_name, call_id, syscall_common)

        t = { "armid"  : syscall_arm,
              "x86id"  : syscall_x86,
              "mipsid" : syscall_mips,
              "common" : syscall_common,
              "cid"    : call_id,
              "name"   : syscall_name,
              "func"   : syscall_func,
              "params" : syscall_params,
              "decl"   : "%-15s  %s (%s);" % (return_type, syscall_func, params) }
        self.syscalls.append(t)

    def parse_file(self, file_path):
        D2("parse_file: %s" % file_path)
        fp = open(file_path)
        for line in fp.xreadlines():
            self.lineno += 1
            line = line.strip()
            if not line: continue
            if line[0] == '#': continue
            self.parse_line(line)

        fp.close()


class StringOutput:
    def __init__(self):
        self.line = ""

    def write(self,msg):
        self.line += msg
        D2("write '%s'" % msg)

    def get(self):
        return self.line


def create_file_path(path):
    dirs = []
    while 1:
        parent = os.path.dirname(path)
        if parent == "/":
            break
        dirs.append(parent)
        path = parent

    dirs.reverse()
    for dir in dirs:
        #print "dir %s" % dir
        if os.path.isdir(dir):
            continue
        os.mkdir(dir)

def walk_source_files(paths,callback,args,excludes=[]):
    """recursively walk a list of paths and files, only keeping the source files in directories"""
    for path in paths:
        if not os.path.isdir(path):
            callback(path,args)
        else:
            for root, dirs, files in os.walk(path):
                #print "w-- %s (ex: %s)" % (repr((root,dirs)), repr(excludes))
                if len(excludes):
                    for d in dirs[:]:
                        if d in excludes:
                            dirs.remove(d)
                for f in files:
                    r, ext = os.path.splitext(f)
                    if ext in [ ".h", ".c", ".cpp", ".S" ]:
                        callback( "%s/%s" % (root,f), args )

def cleanup_dir(path):
    """create a directory if needed, and ensure that it is totally empty
       by removing any existing content in it"""
    if not os.path.exists(path):
        os.mkdir(path)
    else:
        for root, dirs, files in os.walk(path, topdown=False):
            if root.endswith("kernel_headers/"):
                # skip 'kernel_headers'
                continue
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
