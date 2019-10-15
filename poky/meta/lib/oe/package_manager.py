from abc import ABCMeta, abstractmethod
import os
import glob
import subprocess
import shutil
import multiprocessing
import re
import collections
import bb
import tempfile
import oe.utils
import oe.path
import string
from oe.gpg_sign import get_signer

# this can be used by all PM backends to create the index files in parallel
def create_index(arg):
    index_cmd = arg

    bb.note("Executing '%s' ..." % index_cmd)
    result = subprocess.check_output(index_cmd, stderr=subprocess.STDOUT, shell=True).decode("utf-8")
    if result:
        bb.note(result)

"""
This method parse the output from the package managerand return
a dictionary with the information of the packages. This is used
when the packages are in deb or ipk format.
"""
def opkg_query(cmd_output):
    verregex = re.compile(' \([=<>]* [^ )]*\)')
    output = dict()
    pkg = ""
    arch = ""
    ver = ""
    filename = ""
    dep = []
    pkgarch = ""
    for line in cmd_output.splitlines():
        line = line.rstrip()
        if ':' in line:
            if line.startswith("Package: "):
                pkg = line.split(": ")[1]
            elif line.startswith("Architecture: "):
                arch = line.split(": ")[1]
            elif line.startswith("Version: "):
                ver = line.split(": ")[1]
            elif line.startswith("File: ") or line.startswith("Filename:"):
                filename = line.split(": ")[1]
                if "/" in filename:
                    filename = os.path.basename(filename)
            elif line.startswith("Depends: "):
                depends = verregex.sub('', line.split(": ")[1])
                for depend in depends.split(", "):
                    dep.append(depend)
            elif line.startswith("Recommends: "):
                recommends = verregex.sub('', line.split(": ")[1])
                for recommend in recommends.split(", "):
                    dep.append("%s [REC]" % recommend)
            elif line.startswith("PackageArch: "):
                pkgarch = line.split(": ")[1]

        # When there is a blank line save the package information
        elif not line:
            # IPK doesn't include the filename
            if not filename:
                filename = "%s_%s_%s.ipk" % (pkg, ver, arch)
            if pkg:
                output[pkg] = {"arch":arch, "ver":ver,
                        "filename":filename, "deps": dep, "pkgarch":pkgarch }
            pkg = ""
            arch = ""
            ver = ""
            filename = ""
            dep = []
            pkgarch = ""

    if pkg:
        if not filename:
            filename = "%s_%s_%s.ipk" % (pkg, ver, arch)
        output[pkg] = {"arch":arch, "ver":ver,
                "filename":filename, "deps": dep }

    return output


class Indexer(object, metaclass=ABCMeta):
    def __init__(self, d, deploy_dir):
        self.d = d
        self.deploy_dir = deploy_dir

    @abstractmethod
    def write_index(self):
        pass


class RpmIndexer(Indexer):
    def write_index(self):
        if self.d.getVar('PACKAGE_FEED_SIGN') == '1':
            signer = get_signer(self.d, self.d.getVar('PACKAGE_FEED_GPG_BACKEND'))
        else:
            signer = None

        createrepo_c = bb.utils.which(os.environ['PATH'], "createrepo_c")
        result = create_index("%s --update -q %s" % (createrepo_c, self.deploy_dir))
        if result:
            bb.fatal(result)

        # Sign repomd
        if signer:
            sig_type = self.d.getVar('PACKAGE_FEED_GPG_SIGNATURE_TYPE')
            is_ascii_sig = (sig_type.upper() != "BIN")
            signer.detach_sign(os.path.join(self.deploy_dir, 'repodata', 'repomd.xml'),
                               self.d.getVar('PACKAGE_FEED_GPG_NAME'),
                               self.d.getVar('PACKAGE_FEED_GPG_PASSPHRASE_FILE'),
                               armor=is_ascii_sig)


class OpkgIndexer(Indexer):
    def write_index(self):
        arch_vars = ["ALL_MULTILIB_PACKAGE_ARCHS",
                     "SDK_PACKAGE_ARCHS",
                     "MULTILIB_ARCHS"]

        opkg_index_cmd = bb.utils.which(os.getenv('PATH'), "opkg-make-index")
        if self.d.getVar('PACKAGE_FEED_SIGN') == '1':
            signer = get_signer(self.d, self.d.getVar('PACKAGE_FEED_GPG_BACKEND'))
        else:
            signer = None

        if not os.path.exists(os.path.join(self.deploy_dir, "Packages")):
            open(os.path.join(self.deploy_dir, "Packages"), "w").close()

        index_cmds = set()
        index_sign_files = set()
        for arch_var in arch_vars:
            archs = self.d.getVar(arch_var)
            if archs is None:
                continue

            for arch in archs.split():
                pkgs_dir = os.path.join(self.deploy_dir, arch)
                pkgs_file = os.path.join(pkgs_dir, "Packages")

                if not os.path.isdir(pkgs_dir):
                    continue

                if not os.path.exists(pkgs_file):
                    open(pkgs_file, "w").close()

                index_cmds.add('%s -r %s -p %s -m %s' %
                                  (opkg_index_cmd, pkgs_file, pkgs_file, pkgs_dir))

                index_sign_files.add(pkgs_file)

        if len(index_cmds) == 0:
            bb.note("There are no packages in %s!" % self.deploy_dir)
            return

        oe.utils.multiprocess_exec(index_cmds, create_index)

        if signer:
            feed_sig_type = self.d.getVar('PACKAGE_FEED_GPG_SIGNATURE_TYPE')
            is_ascii_sig = (feed_sig_type.upper() != "BIN")
            for f in index_sign_files:
                signer.detach_sign(f,
                                   self.d.getVar('PACKAGE_FEED_GPG_NAME'),
                                   self.d.getVar('PACKAGE_FEED_GPG_PASSPHRASE_FILE'),
                                   armor=is_ascii_sig)


class DpkgIndexer(Indexer):
    def _create_configs(self):
        bb.utils.mkdirhier(self.apt_conf_dir)
        bb.utils.mkdirhier(os.path.join(self.apt_conf_dir, "lists", "partial"))
        bb.utils.mkdirhier(os.path.join(self.apt_conf_dir, "apt.conf.d"))
        bb.utils.mkdirhier(os.path.join(self.apt_conf_dir, "preferences.d"))

        with open(os.path.join(self.apt_conf_dir, "preferences"),
                "w") as prefs_file:
            pass
        with open(os.path.join(self.apt_conf_dir, "sources.list"),
                "w+") as sources_file:
            pass

        with open(self.apt_conf_file, "w") as apt_conf:
            with open(os.path.join(self.d.expand("${STAGING_ETCDIR_NATIVE}"),
                "apt", "apt.conf.sample")) as apt_conf_sample:
                for line in apt_conf_sample.read().split("\n"):
                    line = re.sub("#ROOTFS#", "/dev/null", line)
                    line = re.sub("#APTCONF#", self.apt_conf_dir, line)
                    apt_conf.write(line + "\n")

    def write_index(self):
        self.apt_conf_dir = os.path.join(self.d.expand("${APTCONF_TARGET}"),
                "apt-ftparchive")
        self.apt_conf_file = os.path.join(self.apt_conf_dir, "apt.conf")
        self._create_configs()

        os.environ['APT_CONFIG'] = self.apt_conf_file

        pkg_archs = self.d.getVar('PACKAGE_ARCHS')
        if pkg_archs is not None:
            arch_list = pkg_archs.split()
        sdk_pkg_archs = self.d.getVar('SDK_PACKAGE_ARCHS')
        if sdk_pkg_archs is not None:
            for a in sdk_pkg_archs.split():
                if a not in pkg_archs:
                    arch_list.append(a)

        all_mlb_pkg_arch_list = (self.d.getVar('ALL_MULTILIB_PACKAGE_ARCHS') or "").split()
        arch_list.extend(arch for arch in all_mlb_pkg_arch_list if arch not in arch_list)

        apt_ftparchive = bb.utils.which(os.getenv('PATH'), "apt-ftparchive")
        gzip = bb.utils.which(os.getenv('PATH'), "gzip")

        index_cmds = []
        deb_dirs_found = False
        for arch in arch_list:
            arch_dir = os.path.join(self.deploy_dir, arch)
            if not os.path.isdir(arch_dir):
                continue

            cmd = "cd %s; PSEUDO_UNLOAD=1 %s packages . > Packages;" % (arch_dir, apt_ftparchive)

            cmd += "%s -fcn Packages > Packages.gz;" % gzip

            with open(os.path.join(arch_dir, "Release"), "w+") as release:
                release.write("Label: %s\n" % arch)

            cmd += "PSEUDO_UNLOAD=1 %s release . >> Release" % apt_ftparchive
            
            index_cmds.append(cmd)

            deb_dirs_found = True

        if not deb_dirs_found:
            bb.note("There are no packages in %s" % self.deploy_dir)
            return

        oe.utils.multiprocess_exec(index_cmds, create_index)
        if self.d.getVar('PACKAGE_FEED_SIGN') == '1':
            raise NotImplementedError('Package feed signing not implementd for dpkg')



class PkgsList(object, metaclass=ABCMeta):
    def __init__(self, d, rootfs_dir):
        self.d = d
        self.rootfs_dir = rootfs_dir

    @abstractmethod
    def list_pkgs(self):
        pass

class RpmPkgsList(PkgsList):
    def list_pkgs(self):
        return RpmPM(self.d, self.rootfs_dir, self.d.getVar('TARGET_VENDOR')).list_installed()

class OpkgPkgsList(PkgsList):
    def __init__(self, d, rootfs_dir, config_file):
        super(OpkgPkgsList, self).__init__(d, rootfs_dir)

        self.opkg_cmd = bb.utils.which(os.getenv('PATH'), "opkg")
        self.opkg_args = "-f %s -o %s " % (config_file, rootfs_dir)
        self.opkg_args += self.d.getVar("OPKG_ARGS")

    def list_pkgs(self, format=None):
        cmd = "%s %s status" % (self.opkg_cmd, self.opkg_args)

        # opkg returns success even when it printed some
        # "Collected errors:" report to stderr. Mixing stderr into
        # stdout then leads to random failures later on when
        # parsing the output. To avoid this we need to collect both
        # output streams separately and check for empty stderr.
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
        cmd_output, cmd_stderr = p.communicate()
        cmd_output = cmd_output.decode("utf-8")
        cmd_stderr = cmd_stderr.decode("utf-8")
        if p.returncode or cmd_stderr:
            bb.fatal("Cannot get the installed packages list. Command '%s' "
                     "returned %d and stderr:\n%s" % (cmd, p.returncode, cmd_stderr))

        return opkg_query(cmd_output)


class DpkgPkgsList(PkgsList):

    def list_pkgs(self):
        cmd = [bb.utils.which(os.getenv('PATH'), "dpkg-query"),
               "--admindir=%s/var/lib/dpkg" % self.rootfs_dir,
               "-W"]

        cmd.append("-f=Package: ${Package}\nArchitecture: ${PackageArch}\nVersion: ${Version}\nFile: ${Package}_${Version}_${Architecture}.deb\nDepends: ${Depends}\nRecommends: ${Recommends}\n\n")

        try:
            cmd_output = subprocess.check_output(cmd, stderr=subprocess.STDOUT).strip().decode("utf-8")
        except subprocess.CalledProcessError as e:
            bb.fatal("Cannot get the installed packages list. Command '%s' "
                     "returned %d:\n%s" % (' '.join(cmd), e.returncode, e.output.decode("utf-8")))

        return opkg_query(cmd_output)


class PackageManager(object, metaclass=ABCMeta):
    """
    This is an abstract class. Do not instantiate this directly.
    """

    def __init__(self, d):
        self.d = d
        self.deploy_dir = None
        self.deploy_lock = None

    """
    Update the package manager package database.
    """
    @abstractmethod
    def update(self):
        pass

    """
    Install a list of packages. 'pkgs' is a list object. If 'attempt_only' is
    True, installation failures are ignored.
    """
    @abstractmethod
    def install(self, pkgs, attempt_only=False):
        pass

    """
    Remove a list of packages. 'pkgs' is a list object. If 'with_dependencies'
    is False, the any dependencies are left in place.
    """
    @abstractmethod
    def remove(self, pkgs, with_dependencies=True):
        pass

    """
    This function creates the index files
    """
    @abstractmethod
    def write_index(self):
        pass

    @abstractmethod
    def remove_packaging_data(self):
        pass

    @abstractmethod
    def list_installed(self):
        pass

    """
    Returns the path to a tmpdir where resides the contents of a package.

    Deleting the tmpdir is responsability of the caller.

    """
    @abstractmethod
    def extract(self, pkg):
        pass

    """
    Add remote package feeds into repository manager configuration. The parameters
    for the feeds are set by feed_uris, feed_base_paths and feed_archs.
    See http://www.yoctoproject.org/docs/current/ref-manual/ref-manual.html#var-PACKAGE_FEED_URIS
    for their description.
    """
    @abstractmethod
    def insert_feeds_uris(self, feed_uris, feed_base_paths, feed_archs):
        pass

    """
    Install all packages that match a glob.
    """
    def install_glob(self, globs, sdk=False):
        # TODO don't have sdk here but have a property on the superclass
        # (and respect in install_complementary)
        if sdk:
            pkgdatadir = self.d.expand("${TMPDIR}/pkgdata/${SDK_SYS}")
        else:
            pkgdatadir = self.d.getVar("PKGDATA_DIR")

        try:
            bb.note("Installing globbed packages...")
            cmd = ["oe-pkgdata-util", "-p", pkgdatadir, "list-pkgs", globs]
            pkgs = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode("utf-8")
            self.install(pkgs.split(), attempt_only=True)
        except subprocess.CalledProcessError as e:
            # Return code 1 means no packages matched
            if e.returncode != 1:
                bb.fatal("Could not compute globbed packages list. Command "
                         "'%s' returned %d:\n%s" %
                         (' '.join(cmd), e.returncode, e.output.decode("utf-8")))

    """
    Install complementary packages based upon the list of currently installed
    packages e.g. locales, *-dev, *-dbg, etc. This will only attempt to install
    these packages, if they don't exist then no error will occur.  Note: every
    backend needs to call this function explicitly after the normal package
    installation
    """
    def install_complementary(self, globs=None):
        if globs is None:
            globs = self.d.getVar('IMAGE_INSTALL_COMPLEMENTARY')
            split_linguas = set()

            for translation in self.d.getVar('IMAGE_LINGUAS').split():
                split_linguas.add(translation)
                split_linguas.add(translation.split('-')[0])

            split_linguas = sorted(split_linguas)

            for lang in split_linguas:
                globs += " *-locale-%s" % lang

        if globs is None:
            return

        # we need to write the list of installed packages to a file because the
        # oe-pkgdata-util reads it from a file
        with tempfile.NamedTemporaryFile(mode="w+", prefix="installed-pkgs") as installed_pkgs:
            pkgs = self.list_installed()
            output = oe.utils.format_pkg_list(pkgs, "arch")
            installed_pkgs.write(output)
            installed_pkgs.flush()

            cmd = ["oe-pkgdata-util",
                   "-p", self.d.getVar('PKGDATA_DIR'), "glob", installed_pkgs.name,
                   globs]
            exclude = self.d.getVar('PACKAGE_EXCLUDE_COMPLEMENTARY')
            if exclude:
                cmd.extend(['--exclude=' + '|'.join(exclude.split())])
            try:
                bb.note("Installing complementary packages ...")
                bb.note('Running %s' % cmd)
                complementary_pkgs = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode("utf-8")
                self.install(complementary_pkgs.split(), attempt_only=True)
            except subprocess.CalledProcessError as e:
                bb.fatal("Could not compute complementary packages list. Command "
                         "'%s' returned %d:\n%s" %
                         (' '.join(cmd), e.returncode, e.output.decode("utf-8")))

    def deploy_dir_lock(self):
        if self.deploy_dir is None:
            raise RuntimeError("deploy_dir is not set!")

        lock_file_name = os.path.join(self.deploy_dir, "deploy.lock")

        self.deploy_lock = bb.utils.lockfile(lock_file_name)

    def deploy_dir_unlock(self):
        if self.deploy_lock is None:
            return

        bb.utils.unlockfile(self.deploy_lock)

        self.deploy_lock = None

    """
    Construct URIs based on the following pattern: uri/base_path where 'uri'
    and 'base_path' correspond to each element of the corresponding array
    argument leading to len(uris) x len(base_paths) elements on the returned
    array
    """
    def construct_uris(self, uris, base_paths):
        def _append(arr1, arr2, sep='/'):
            res = []
            narr1 = [a.rstrip(sep) for a in arr1]
            narr2 = [a.rstrip(sep).lstrip(sep) for a in arr2]
            for a1 in narr1:
                if arr2:
                    for a2 in narr2:
                        res.append("%s%s%s" % (a1, sep, a2))
                else:
                    res.append(a1)
            return res
        return _append(uris, base_paths)

class RpmPM(PackageManager):
    def __init__(self,
                 d,
                 target_rootfs,
                 target_vendor,
                 task_name='target',
                 providename=None,
                 arch_var=None,
                 os_var=None,
                 rpm_repo_workdir="oe-rootfs-repo"):
        super(RpmPM, self).__init__(d)
        self.target_rootfs = target_rootfs
        self.target_vendor = target_vendor
        self.task_name = task_name
        if arch_var == None:
            self.archs = self.d.getVar('ALL_MULTILIB_PACKAGE_ARCHS').replace("-","_")
        else:
            self.archs = self.d.getVar(arch_var).replace("-","_")
        if task_name == "host":
            self.primary_arch = self.d.getVar('SDK_ARCH')
        else:
            self.primary_arch = self.d.getVar('MACHINE_ARCH')

        self.rpm_repo_dir = oe.path.join(self.d.getVar('WORKDIR'), rpm_repo_workdir)
        bb.utils.mkdirhier(self.rpm_repo_dir)
        oe.path.symlink(self.d.getVar('DEPLOY_DIR_RPM'), oe.path.join(self.rpm_repo_dir, "rpm"), True)

        self.saved_packaging_data = self.d.expand('${T}/saved_packaging_data/%s' % self.task_name)
        if not os.path.exists(self.d.expand('${T}/saved_packaging_data')):
            bb.utils.mkdirhier(self.d.expand('${T}/saved_packaging_data'))
        self.packaging_data_dirs = ['var/lib/rpm', 'var/lib/dnf', 'var/cache/dnf']
        self.solution_manifest = self.d.expand('${T}/saved/%s_solution' %
                                               self.task_name)
        if not os.path.exists(self.d.expand('${T}/saved')):
            bb.utils.mkdirhier(self.d.expand('${T}/saved'))

    def _configure_dnf(self):
        # libsolv handles 'noarch' internally, we don't need to specify it explicitly
        archs = [i for i in reversed(self.archs.split()) if i not in ["any", "all", "noarch"]]
        # This prevents accidental matching against libsolv's built-in policies
        if len(archs) <= 1:
            archs = archs + ["bogusarch"]
        confdir = "%s/%s" %(self.target_rootfs, "etc/dnf/vars/")
        bb.utils.mkdirhier(confdir)
        open(confdir + "arch", 'w').write(":".join(archs))
        distro_codename = self.d.getVar('DISTRO_CODENAME')
        open(confdir + "releasever", 'w').write(distro_codename if distro_codename is not None else '')

        open(oe.path.join(self.target_rootfs, "etc/dnf/dnf.conf"), 'w').write("")


    def _configure_rpm(self):
        # We need to configure rpm to use our primary package architecture as the installation architecture,
        # and to make it compatible with other package architectures that we use.
        # Otherwise it will refuse to proceed with packages installation.
        platformconfdir = "%s/%s" %(self.target_rootfs, "etc/rpm/")
        rpmrcconfdir = "%s/%s" %(self.target_rootfs, "etc/")
        bb.utils.mkdirhier(platformconfdir)
        open(platformconfdir + "platform", 'w').write("%s-pc-linux" % self.primary_arch)
        open(rpmrcconfdir + "rpmrc", 'w').write("arch_compat: %s: %s\n" % (self.primary_arch, self.archs if len(self.archs) > 0 else self.primary_arch))

        open(platformconfdir + "macros", 'w').write("%_transaction_color 7\n")
        if self.d.getVar('RPM_PREFER_ELF_ARCH'):
            open(platformconfdir + "macros", 'a').write("%%_prefer_color %s" % (self.d.getVar('RPM_PREFER_ELF_ARCH')))
        else:
            open(platformconfdir + "macros", 'a').write("%_prefer_color 7")

        if self.d.getVar('RPM_SIGN_PACKAGES') == '1':
            signer = get_signer(self.d, self.d.getVar('RPM_GPG_BACKEND'))
            pubkey_path = oe.path.join(self.d.getVar('B'), 'rpm-key')
            signer.export_pubkey(pubkey_path, self.d.getVar('RPM_GPG_NAME'))
            rpm_bin = bb.utils.which(os.getenv('PATH'), "rpmkeys")
            cmd = [rpm_bin, '--root=%s' % self.target_rootfs, '--import', pubkey_path]
            try:
                subprocess.check_output(cmd, stderr=subprocess.STDOUT)
            except subprocess.CalledProcessError as e:
                bb.fatal("Importing GPG key failed. Command '%s' "
                        "returned %d:\n%s" % (' '.join(cmd), e.returncode, e.output.decode("utf-8")))

    def create_configs(self):
        self._configure_dnf()
        self._configure_rpm()

    def write_index(self):
        lockfilename = self.d.getVar('DEPLOY_DIR_RPM') + "/rpm.lock"
        lf = bb.utils.lockfile(lockfilename, False)
        RpmIndexer(self.d, self.rpm_repo_dir).write_index()
        bb.utils.unlockfile(lf)

    def insert_feeds_uris(self, feed_uris, feed_base_paths, feed_archs):
        from urllib.parse import urlparse

        if feed_uris == "":
            return

        gpg_opts = ''
        if self.d.getVar('PACKAGE_FEED_SIGN') == '1':
            gpg_opts += 'repo_gpgcheck=1\n'
            gpg_opts += 'gpgkey=file://%s/pki/packagefeed-gpg/PACKAGEFEED-GPG-KEY-%s-%s\n' % (self.d.getVar('sysconfdir'), self.d.getVar('DISTRO'), self.d.getVar('DISTRO_CODENAME'))

        if self.d.getVar('RPM_SIGN_PACKAGES') != '1':
            gpg_opts += 'gpgcheck=0\n'

        bb.utils.mkdirhier(oe.path.join(self.target_rootfs, "etc", "yum.repos.d"))
        remote_uris = self.construct_uris(feed_uris.split(), feed_base_paths.split())
        for uri in remote_uris:
            repo_base = "oe-remote-repo" + "-".join(urlparse(uri).path.split("/"))
            if feed_archs is not None:
                for arch in feed_archs.split():
                    repo_uri = uri + "/" + arch
                    repo_id   = "oe-remote-repo"  + "-".join(urlparse(repo_uri).path.split("/"))
                    repo_name = "OE Remote Repo:" + " ".join(urlparse(repo_uri).path.split("/"))
                    open(oe.path.join(self.target_rootfs, "etc", "yum.repos.d", repo_base + ".repo"), 'a').write(
                             "[%s]\nname=%s\nbaseurl=%s\n%s\n" % (repo_id, repo_name, repo_uri, gpg_opts))
            else:
                repo_name = "OE Remote Repo:" + " ".join(urlparse(uri).path.split("/"))
                repo_uri = uri
                open(oe.path.join(self.target_rootfs, "etc", "yum.repos.d", repo_base + ".repo"), 'w').write(
                             "[%s]\nname=%s\nbaseurl=%s\n%s" % (repo_base, repo_name, repo_uri, gpg_opts))

    def _prepare_pkg_transaction(self):
        os.environ['D'] = self.target_rootfs
        os.environ['OFFLINE_ROOT'] = self.target_rootfs
        os.environ['IPKG_OFFLINE_ROOT'] = self.target_rootfs
        os.environ['OPKG_OFFLINE_ROOT'] = self.target_rootfs
        os.environ['INTERCEPT_DIR'] = oe.path.join(self.d.getVar('WORKDIR'),
                                                   "intercept_scripts")
        os.environ['NATIVE_ROOT'] = self.d.getVar('STAGING_DIR_NATIVE')


    def install(self, pkgs, attempt_only = False):
        if len(pkgs) == 0:
            return
        self._prepare_pkg_transaction()

        bad_recommendations = self.d.getVar('BAD_RECOMMENDATIONS')
        package_exclude = self.d.getVar('PACKAGE_EXCLUDE')
        exclude_pkgs = (bad_recommendations.split() if bad_recommendations else []) + (package_exclude.split() if package_exclude else [])

        output = self._invoke_dnf((["--skip-broken"] if attempt_only else []) +
                         (["-x", ",".join(exclude_pkgs)] if len(exclude_pkgs) > 0 else []) +
                         (["--setopt=install_weak_deps=False"] if self.d.getVar('NO_RECOMMENDATIONS') == "1" else []) +
                         (["--nogpgcheck"] if self.d.getVar('RPM_SIGN_PACKAGES') != '1' else ["--setopt=gpgcheck=True"]) +
                         ["install"] +
                         pkgs)

        failed_scriptlets_pkgnames = collections.OrderedDict()
        for line in output.splitlines():
            if line.startswith("Non-fatal POSTIN scriptlet failure in rpm package"):
                failed_scriptlets_pkgnames[line.split()[-1]] = True

        for pkg in failed_scriptlets_pkgnames.keys():
            self.save_rpmpostinst(pkg)

    def remove(self, pkgs, with_dependencies = True):
        if len(pkgs) == 0:
            return
        self._prepare_pkg_transaction()

        if with_dependencies:
            self._invoke_dnf(["remove"] + pkgs)
        else:
            cmd = bb.utils.which(os.getenv('PATH'), "rpm")
            args = ["-e", "-v", "--nodeps", "--root=%s" %self.target_rootfs]

            try:
                bb.note("Running %s" % ' '.join([cmd] + args + pkgs))
                output = subprocess.check_output([cmd] + args + pkgs, stderr=subprocess.STDOUT).decode("utf-8")
                bb.note(output)
            except subprocess.CalledProcessError as e:
                bb.fatal("Could not invoke rpm. Command "
                     "'%s' returned %d:\n%s" % (' '.join([cmd] + args + pkgs), e.returncode, e.output.decode("utf-8")))

    def upgrade(self):
        self._prepare_pkg_transaction()
        self._invoke_dnf(["upgrade"])

    def autoremove(self):
        self._prepare_pkg_transaction()
        self._invoke_dnf(["autoremove"])

    def remove_packaging_data(self):
        self._invoke_dnf(["clean", "all"])
        for dir in self.packaging_data_dirs:
            bb.utils.remove(oe.path.join(self.target_rootfs, dir), True)

    def backup_packaging_data(self):
        # Save the packaging dirs for increment rpm image generation
        if os.path.exists(self.saved_packaging_data):
            bb.utils.remove(self.saved_packaging_data, True)
        for i in self.packaging_data_dirs:
            source_dir = oe.path.join(self.target_rootfs, i)
            target_dir = oe.path.join(self.saved_packaging_data, i)
            shutil.copytree(source_dir, target_dir, symlinks=True)

    def recovery_packaging_data(self):
        # Move the rpmlib back
        if os.path.exists(self.saved_packaging_data):
            for i in self.packaging_data_dirs:
                target_dir = oe.path.join(self.target_rootfs, i)
                if os.path.exists(target_dir):
                    bb.utils.remove(target_dir, True)
                source_dir = oe.path.join(self.saved_packaging_data, i)
                shutil.copytree(source_dir,
                            target_dir,
                            symlinks=True)

    def list_installed(self):
        output = self._invoke_dnf(["repoquery", "--installed", "--queryformat", "Package: %{name} %{arch} %{version} %{name}-%{version}-%{release}.%{arch}.rpm\nDependencies:\n%{requires}\nRecommendations:\n%{recommends}\nDependenciesEndHere:\n"],
                                  print_output = False)
        packages = {}
        current_package = None
        current_deps = None
        current_state = "initial"
        for line in output.splitlines():
            if line.startswith("Package:"):
                package_info = line.split(" ")[1:]
                current_package = package_info[0]
                package_arch = package_info[1]
                package_version = package_info[2]
                package_rpm = package_info[3]
                packages[current_package] = {"arch":package_arch, "ver":package_version, "filename":package_rpm}
                current_deps = []
            elif line.startswith("Dependencies:"):
                current_state = "dependencies"
            elif line.startswith("Recommendations"):
                current_state = "recommendations"
            elif line.startswith("DependenciesEndHere:"):
                current_state = "initial"
                packages[current_package]["deps"] = current_deps
            elif len(line) > 0:
                if current_state == "dependencies":
                    current_deps.append(line)
                elif current_state == "recommendations":
                    current_deps.append("%s [REC]" % line)

        return packages

    def update(self):
        self._invoke_dnf(["makecache", "--refresh"])

    def _invoke_dnf(self, dnf_args, fatal = True, print_output = True ):
        os.environ['RPM_ETCCONFIGDIR'] = self.target_rootfs

        dnf_cmd = bb.utils.which(os.getenv('PATH'), "dnf")
        standard_dnf_args = (["-v", "--rpmverbosity=debug"] if self.d.getVar('ROOTFS_RPM_DEBUG') else []) + ["-y",
                             "-c", oe.path.join(self.target_rootfs, "etc/dnf/dnf.conf"),
                             "--setopt=reposdir=%s" %(oe.path.join(self.target_rootfs, "etc/yum.repos.d")),
                             "--repofrompath=oe-repo,%s" % (self.rpm_repo_dir),
                             "--installroot=%s" % (self.target_rootfs),
                             "--setopt=logdir=%s" % (self.d.getVar('T'))
                            ]
        cmd = [dnf_cmd] + standard_dnf_args + dnf_args
        try:
            output = subprocess.check_output(cmd,stderr=subprocess.STDOUT).decode("utf-8")
            if print_output:
                bb.note(output)
            return output
        except subprocess.CalledProcessError as e:
            if print_output:
                (bb.note, bb.fatal)[fatal]("Could not invoke dnf. Command "
                     "'%s' returned %d:\n%s" % (' '.join(cmd), e.returncode, e.output.decode("utf-8")))
            else:
                (bb.note, bb.fatal)[fatal]("Could not invoke dnf. Command "
                     "'%s' returned %d:" % (' '.join(cmd), e.returncode))
            return e.output.decode("utf-8")

    def dump_install_solution(self, pkgs):
        open(self.solution_manifest, 'w').write(" ".join(pkgs))
        return pkgs

    def load_old_install_solution(self):
        if not os.path.exists(self.solution_manifest):
            return []

        return open(self.solution_manifest, 'r').read().split()

    def _script_num_prefix(self, path):
        files = os.listdir(path)
        numbers = set()
        numbers.add(99)
        for f in files:
            numbers.add(int(f.split("-")[0]))
        return max(numbers) + 1

    def save_rpmpostinst(self, pkg):
        bb.note("Saving postinstall script of %s" % (pkg))
        cmd = bb.utils.which(os.getenv('PATH'), "rpm")
        args = ["-q", "--root=%s" % self.target_rootfs, "--queryformat", "%{postin}", pkg]

        try:
            output = subprocess.check_output([cmd] + args,stderr=subprocess.STDOUT).decode("utf-8")
        except subprocess.CalledProcessError as e:
            bb.fatal("Could not invoke rpm. Command "
                     "'%s' returned %d:\n%s" % (' '.join([cmd] + args), e.returncode, e.output.decode("utf-8")))

        # may need to prepend #!/bin/sh to output

        target_path = oe.path.join(self.target_rootfs, self.d.expand('${sysconfdir}/rpm-postinsts/'))
        bb.utils.mkdirhier(target_path)
        num = self._script_num_prefix(target_path)
        saved_script_name = oe.path.join(target_path, "%d-%s" % (num, pkg))
        open(saved_script_name, 'w').write(output)
        os.chmod(saved_script_name, 0o755)

    def extract(self, pkg):
        output = self._invoke_dnf(["repoquery", "--queryformat", "%{location}", pkg])
        pkg_name = output.splitlines()[-1]
        if not pkg_name.endswith(".rpm"):
            bb.fatal("dnf could not find package %s in repository: %s" %(pkg, output))
        pkg_path = oe.path.join(self.rpm_repo_dir, pkg_name)

        cpio_cmd = bb.utils.which(os.getenv("PATH"), "cpio")
        rpm2cpio_cmd = bb.utils.which(os.getenv("PATH"), "rpm2cpio")

        if not os.path.isfile(pkg_path):
            bb.fatal("Unable to extract package for '%s'."
                     "File %s doesn't exists" % (pkg, pkg_path))

        tmp_dir = tempfile.mkdtemp()
        current_dir = os.getcwd()
        os.chdir(tmp_dir)

        try:
            cmd = "%s %s | %s -idmv" % (rpm2cpio_cmd, pkg_path, cpio_cmd)
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
        except subprocess.CalledProcessError as e:
            bb.utils.remove(tmp_dir, recurse=True)
            bb.fatal("Unable to extract %s package. Command '%s' "
                     "returned %d:\n%s" % (pkg_path, cmd, e.returncode, e.output.decode("utf-8")))
        except OSError as e:
            bb.utils.remove(tmp_dir, recurse=True)
            bb.fatal("Unable to extract %s package. Command '%s' "
                     "returned %d:\n%s at %s" % (pkg_path, cmd, e.errno, e.strerror, e.filename))

        bb.note("Extracted %s to %s" % (pkg_path, tmp_dir))
        os.chdir(current_dir)

        return tmp_dir


class OpkgDpkgPM(PackageManager):
    """
    This is an abstract class. Do not instantiate this directly.
    """
    def __init__(self, d):
        super(OpkgDpkgPM, self).__init__(d)

    """
    Returns a dictionary with the package info.

    This method extracts the common parts for Opkg and Dpkg
    """
    def package_info(self, pkg, cmd):

        try:
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).decode("utf-8")
        except subprocess.CalledProcessError as e:
            bb.fatal("Unable to list available packages. Command '%s' "
                     "returned %d:\n%s" % (cmd, e.returncode, e.output.decode("utf-8")))
        return opkg_query(output)

    """
    Returns the path to a tmpdir where resides the contents of a package.

    Deleting the tmpdir is responsability of the caller.

    This method extracts the common parts for Opkg and Dpkg
    """
    def extract(self, pkg, pkg_info):

        ar_cmd = bb.utils.which(os.getenv("PATH"), "ar")
        tar_cmd = bb.utils.which(os.getenv("PATH"), "tar")
        pkg_path = pkg_info[pkg]["filepath"]

        if not os.path.isfile(pkg_path):
            bb.fatal("Unable to extract package for '%s'."
                     "File %s doesn't exists" % (pkg, pkg_path))

        tmp_dir = tempfile.mkdtemp()
        current_dir = os.getcwd()
        os.chdir(tmp_dir)
        if self.d.getVar('IMAGE_PKGTYPE') == 'deb':
            data_tar = 'data.tar.xz'
        else:
            data_tar = 'data.tar.gz'

        try:
            cmd = [ar_cmd, 'x', pkg_path]
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
            cmd = [tar_cmd, 'xf', data_tar]
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            bb.utils.remove(tmp_dir, recurse=True)
            bb.fatal("Unable to extract %s package. Command '%s' "
                     "returned %d:\n%s" % (pkg_path, ' '.join(cmd), e.returncode, e.output.decode("utf-8")))
        except OSError as e:
            bb.utils.remove(tmp_dir, recurse=True)
            bb.fatal("Unable to extract %s package. Command '%s' "
                     "returned %d:\n%s at %s" % (pkg_path, ' '.join(cmd), e.errno, e.strerror, e.filename))

        bb.note("Extracted %s to %s" % (pkg_path, tmp_dir))
        bb.utils.remove(os.path.join(tmp_dir, "debian-binary"))
        bb.utils.remove(os.path.join(tmp_dir, "control.tar.gz"))
        os.chdir(current_dir)

        return tmp_dir


class OpkgPM(OpkgDpkgPM):
    def __init__(self, d, target_rootfs, config_file, archs, task_name='target'):
        super(OpkgPM, self).__init__(d)

        self.target_rootfs = target_rootfs
        self.config_file = config_file
        self.pkg_archs = archs
        self.task_name = task_name

        self.deploy_dir = self.d.getVar("DEPLOY_DIR_IPK")
        self.deploy_lock_file = os.path.join(self.deploy_dir, "deploy.lock")
        self.opkg_cmd = bb.utils.which(os.getenv('PATH'), "opkg")
        self.opkg_args = "--volatile-cache -f %s -t %s -o %s " % (self.config_file, self.d.expand('${T}/ipktemp/') ,target_rootfs)
        self.opkg_args += self.d.getVar("OPKG_ARGS")

        opkg_lib_dir = self.d.getVar('OPKGLIBDIR')
        if opkg_lib_dir[0] == "/":
            opkg_lib_dir = opkg_lib_dir[1:]

        self.opkg_dir = os.path.join(target_rootfs, opkg_lib_dir, "opkg")

        bb.utils.mkdirhier(self.opkg_dir)

        self.saved_opkg_dir = self.d.expand('${T}/saved/%s' % self.task_name)
        if not os.path.exists(self.d.expand('${T}/saved')):
            bb.utils.mkdirhier(self.d.expand('${T}/saved'))

        self.from_feeds = (self.d.getVar('BUILD_IMAGES_FROM_FEEDS') or "") == "1"
        if self.from_feeds:
            self._create_custom_config()
        else:
            self._create_config()

        self.indexer = OpkgIndexer(self.d, self.deploy_dir)

    """
    This function will change a package's status in /var/lib/opkg/status file.
    If 'packages' is None then the new_status will be applied to all
    packages
    """
    def mark_packages(self, status_tag, packages=None):
        status_file = os.path.join(self.opkg_dir, "status")

        with open(status_file, "r") as sf:
            with open(status_file + ".tmp", "w+") as tmp_sf:
                if packages is None:
                    tmp_sf.write(re.sub(r"Package: (.*?)\n((?:[^\n]+\n)*?)Status: (.*)(?:unpacked|installed)",
                                        r"Package: \1\n\2Status: \3%s" % status_tag,
                                        sf.read()))
                else:
                    if type(packages).__name__ != "list":
                        raise TypeError("'packages' should be a list object")

                    status = sf.read()
                    for pkg in packages:
                        status = re.sub(r"Package: %s\n((?:[^\n]+\n)*?)Status: (.*)(?:unpacked|installed)" % pkg,
                                        r"Package: %s\n\1Status: \2%s" % (pkg, status_tag),
                                        status)

                    tmp_sf.write(status)

        os.rename(status_file + ".tmp", status_file)

    def _create_custom_config(self):
        bb.note("Building from feeds activated!")

        with open(self.config_file, "w+") as config_file:
            priority = 1
            for arch in self.pkg_archs.split():
                config_file.write("arch %s %d\n" % (arch, priority))
                priority += 5

            for line in (self.d.getVar('IPK_FEED_URIS') or "").split():
                feed_match = re.match("^[ \t]*(.*)##([^ \t]*)[ \t]*$", line)

                if feed_match is not None:
                    feed_name = feed_match.group(1)
                    feed_uri = feed_match.group(2)

                    bb.note("Add %s feed with URL %s" % (feed_name, feed_uri))

                    config_file.write("src/gz %s %s\n" % (feed_name, feed_uri))

            """
            Allow to use package deploy directory contents as quick devel-testing
            feed. This creates individual feed configs for each arch subdir of those
            specified as compatible for the current machine.
            NOTE: Development-helper feature, NOT a full-fledged feed.
            """
            if (self.d.getVar('FEED_DEPLOYDIR_BASE_URI') or "") != "":
                for arch in self.pkg_archs.split():
                    cfg_file_name = os.path.join(self.target_rootfs,
                                                 self.d.getVar("sysconfdir"),
                                                 "opkg",
                                                 "local-%s-feed.conf" % arch)

                    with open(cfg_file_name, "w+") as cfg_file:
                        cfg_file.write("src/gz local-%s %s/%s" %
                                       (arch,
                                        self.d.getVar('FEED_DEPLOYDIR_BASE_URI'),
                                        arch))

                        if self.d.getVar('OPKGLIBDIR') != '/var/lib':
                            # There is no command line option for this anymore, we need to add
                            # info_dir and status_file to config file, if OPKGLIBDIR doesn't have
                            # the default value of "/var/lib" as defined in opkg:
                            # libopkg/opkg_conf.h:#define OPKG_CONF_DEFAULT_LISTS_DIR     VARDIR "/lib/opkg/lists"
                            # libopkg/opkg_conf.h:#define OPKG_CONF_DEFAULT_INFO_DIR      VARDIR "/lib/opkg/info"
                            # libopkg/opkg_conf.h:#define OPKG_CONF_DEFAULT_STATUS_FILE   VARDIR "/lib/opkg/status"
                            cfg_file.write("option info_dir     %s\n" % os.path.join(self.d.getVar('OPKGLIBDIR'), 'opkg', 'info'))
                            cfg_file.write("option lists_dir    %s\n" % os.path.join(self.d.getVar('OPKGLIBDIR'), 'opkg', 'lists'))
                            cfg_file.write("option status_file  %s\n" % os.path.join(self.d.getVar('OPKGLIBDIR'), 'opkg', 'status'))


    def _create_config(self):
        with open(self.config_file, "w+") as config_file:
            priority = 1
            for arch in self.pkg_archs.split():
                config_file.write("arch %s %d\n" % (arch, priority))
                priority += 5

            config_file.write("src oe file:%s\n" % self.deploy_dir)

            for arch in self.pkg_archs.split():
                pkgs_dir = os.path.join(self.deploy_dir, arch)
                if os.path.isdir(pkgs_dir):
                    config_file.write("src oe-%s file:%s\n" %
                                      (arch, pkgs_dir))

            if self.d.getVar('OPKGLIBDIR') != '/var/lib':
                # There is no command line option for this anymore, we need to add
                # info_dir and status_file to config file, if OPKGLIBDIR doesn't have
                # the default value of "/var/lib" as defined in opkg:
                # libopkg/opkg_conf.h:#define OPKG_CONF_DEFAULT_LISTS_DIR     VARDIR "/lib/opkg/lists"
                # libopkg/opkg_conf.h:#define OPKG_CONF_DEFAULT_INFO_DIR      VARDIR "/lib/opkg/info"
                # libopkg/opkg_conf.h:#define OPKG_CONF_DEFAULT_STATUS_FILE   VARDIR "/lib/opkg/status"
                config_file.write("option info_dir     %s\n" % os.path.join(self.d.getVar('OPKGLIBDIR'), 'opkg', 'info'))
                config_file.write("option lists_dir    %s\n" % os.path.join(self.d.getVar('OPKGLIBDIR'), 'opkg', 'lists'))
                config_file.write("option status_file  %s\n" % os.path.join(self.d.getVar('OPKGLIBDIR'), 'opkg', 'status'))

    def insert_feeds_uris(self, feed_uris, feed_base_paths, feed_archs):
        if feed_uris == "":
            return

        rootfs_config = os.path.join('%s/etc/opkg/base-feeds.conf'
                                  % self.target_rootfs)

        feed_uris = self.construct_uris(feed_uris.split(), feed_base_paths.split())
        archs = self.pkg_archs.split() if feed_archs is None else feed_archs.split()

        with open(rootfs_config, "w+") as config_file:
            uri_iterator = 0
            for uri in feed_uris:
                if archs:
                    for arch in archs:
                        if (feed_archs is None) and (not os.path.exists(oe.path.join(self.deploy_dir, arch))):
                            continue
                        bb.note('Adding opkg feed url-%s-%d (%s)' %
                            (arch, uri_iterator, uri))
                        config_file.write("src/gz uri-%s-%d %s/%s\n" %
                                          (arch, uri_iterator, uri, arch))
                else:
                    bb.note('Adding opkg feed url-%d (%s)' %
                        (uri_iterator, uri))
                    config_file.write("src/gz uri-%d %s\n" %
                                      (uri_iterator, uri))

                uri_iterator += 1

    def update(self):
        self.deploy_dir_lock()

        cmd = "%s %s update" % (self.opkg_cmd, self.opkg_args)

        try:
            subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            self.deploy_dir_unlock()
            bb.fatal("Unable to update the package index files. Command '%s' "
                     "returned %d:\n%s" % (cmd, e.returncode, e.output.decode("utf-8")))

        self.deploy_dir_unlock()

    def install(self, pkgs, attempt_only=False):
        if not pkgs:
            return

        cmd = "%s %s install %s" % (self.opkg_cmd, self.opkg_args, ' '.join(pkgs))

        os.environ['D'] = self.target_rootfs
        os.environ['OFFLINE_ROOT'] = self.target_rootfs
        os.environ['IPKG_OFFLINE_ROOT'] = self.target_rootfs
        os.environ['OPKG_OFFLINE_ROOT'] = self.target_rootfs
        os.environ['INTERCEPT_DIR'] = os.path.join(self.d.getVar('WORKDIR'),
                                                   "intercept_scripts")
        os.environ['NATIVE_ROOT'] = self.d.getVar('STAGING_DIR_NATIVE')

        try:
            bb.note("Installing the following packages: %s" % ' '.join(pkgs))
            bb.note(cmd)
            output = subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT).decode("utf-8")
            bb.note(output)
        except subprocess.CalledProcessError as e:
            (bb.fatal, bb.warn)[attempt_only]("Unable to install packages. "
                                              "Command '%s' returned %d:\n%s" %
                                              (cmd, e.returncode, e.output.decode("utf-8")))

    def remove(self, pkgs, with_dependencies=True):
        if with_dependencies:
            cmd = "%s %s --force-remove --force-removal-of-dependent-packages remove %s" % \
                (self.opkg_cmd, self.opkg_args, ' '.join(pkgs))
        else:
            cmd = "%s %s --force-depends remove %s" % \
                (self.opkg_cmd, self.opkg_args, ' '.join(pkgs))

        try:
            bb.note(cmd)
            output = subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT).decode("utf-8")
            bb.note(output)
        except subprocess.CalledProcessError as e:
            bb.fatal("Unable to remove packages. Command '%s' "
                     "returned %d:\n%s" % (e.cmd, e.returncode, e.output.decode("utf-8")))

    def write_index(self):
        self.deploy_dir_lock()

        result = self.indexer.write_index()

        self.deploy_dir_unlock()

        if result is not None:
            bb.fatal(result)

    def remove_packaging_data(self):
        bb.utils.remove(self.opkg_dir, True)
        # create the directory back, it's needed by PM lock
        bb.utils.mkdirhier(self.opkg_dir)

    def remove_lists(self):
        if not self.from_feeds:
            bb.utils.remove(os.path.join(self.opkg_dir, "lists"), True)

    def list_installed(self):
        return OpkgPkgsList(self.d, self.target_rootfs, self.config_file).list_pkgs()

    def handle_bad_recommendations(self):
        bad_recommendations = self.d.getVar("BAD_RECOMMENDATIONS") or ""
        if bad_recommendations.strip() == "":
            return

        status_file = os.path.join(self.opkg_dir, "status")

        # If status file existed, it means the bad recommendations has already
        # been handled
        if os.path.exists(status_file):
            return

        cmd = "%s %s info " % (self.opkg_cmd, self.opkg_args)

        with open(status_file, "w+") as status:
            for pkg in bad_recommendations.split():
                pkg_info = cmd + pkg

                try:
                    output = subprocess.check_output(pkg_info.split(), stderr=subprocess.STDOUT).strip().decode("utf-8")
                except subprocess.CalledProcessError as e:
                    bb.fatal("Cannot get package info. Command '%s' "
                             "returned %d:\n%s" % (pkg_info, e.returncode, e.output.decode("utf-8")))

                if output == "":
                    bb.note("Ignored bad recommendation: '%s' is "
                            "not a package" % pkg)
                    continue

                for line in output.split('\n'):
                    if line.startswith("Status:"):
                        status.write("Status: deinstall hold not-installed\n")
                    else:
                        status.write(line + "\n")

                # Append a blank line after each package entry to ensure that it
                # is separated from the following entry
                status.write("\n")

    '''
    The following function dummy installs pkgs and returns the log of output.
    '''
    def dummy_install(self, pkgs):
        if len(pkgs) == 0:
            return

        # Create an temp dir as opkg root for dummy installation
        temp_rootfs = self.d.expand('${T}/opkg')
        opkg_lib_dir = self.d.getVar('OPKGLIBDIR')
        if opkg_lib_dir[0] == "/":
            opkg_lib_dir = opkg_lib_dir[1:]
        temp_opkg_dir = os.path.join(temp_rootfs, opkg_lib_dir, 'opkg')
        bb.utils.mkdirhier(temp_opkg_dir)

        opkg_args = "-f %s -o %s " % (self.config_file, temp_rootfs)
        opkg_args += self.d.getVar("OPKG_ARGS")

        cmd = "%s %s update" % (self.opkg_cmd, opkg_args)
        try:
            subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
        except subprocess.CalledProcessError as e:
            bb.fatal("Unable to update. Command '%s' "
                     "returned %d:\n%s" % (cmd, e.returncode, e.output.decode("utf-8")))

        # Dummy installation
        cmd = "%s %s --noaction install %s " % (self.opkg_cmd,
                                                opkg_args,
                                                ' '.join(pkgs))
        try:
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True)
        except subprocess.CalledProcessError as e:
            bb.fatal("Unable to dummy install packages. Command '%s' "
                     "returned %d:\n%s" % (cmd, e.returncode, e.output.decode("utf-8")))

        bb.utils.remove(temp_rootfs, True)

        return output

    def backup_packaging_data(self):
        # Save the opkglib for increment ipk image generation
        if os.path.exists(self.saved_opkg_dir):
            bb.utils.remove(self.saved_opkg_dir, True)
        shutil.copytree(self.opkg_dir,
                        self.saved_opkg_dir,
                        symlinks=True)

    def recover_packaging_data(self):
        # Move the opkglib back
        if os.path.exists(self.saved_opkg_dir):
            if os.path.exists(self.opkg_dir):
                bb.utils.remove(self.opkg_dir, True)

            bb.note('Recover packaging data')
            shutil.copytree(self.saved_opkg_dir,
                            self.opkg_dir,
                            symlinks=True)

    """
    Returns a dictionary with the package info.
    """
    def package_info(self, pkg):
        cmd = "%s %s info %s" % (self.opkg_cmd, self.opkg_args, pkg)
        pkg_info = super(OpkgPM, self).package_info(pkg, cmd)

        pkg_arch = pkg_info[pkg]["arch"]
        pkg_filename = pkg_info[pkg]["filename"]
        pkg_info[pkg]["filepath"] = \
                os.path.join(self.deploy_dir, pkg_arch, pkg_filename)

        return pkg_info

    """
    Returns the path to a tmpdir where resides the contents of a package.

    Deleting the tmpdir is responsability of the caller.
    """
    def extract(self, pkg):
        pkg_info = self.package_info(pkg)
        if not pkg_info:
            bb.fatal("Unable to get information for package '%s' while "
                     "trying to extract the package."  % pkg)

        tmp_dir = super(OpkgPM, self).extract(pkg, pkg_info)
        bb.utils.remove(os.path.join(tmp_dir, "data.tar.gz"))

        return tmp_dir

class DpkgPM(OpkgDpkgPM):
    def __init__(self, d, target_rootfs, archs, base_archs, apt_conf_dir=None):
        super(DpkgPM, self).__init__(d)
        self.target_rootfs = target_rootfs
        self.deploy_dir = self.d.getVar('DEPLOY_DIR_DEB')
        if apt_conf_dir is None:
            self.apt_conf_dir = self.d.expand("${APTCONF_TARGET}/apt")
        else:
            self.apt_conf_dir = apt_conf_dir
        self.apt_conf_file = os.path.join(self.apt_conf_dir, "apt.conf")
        self.apt_get_cmd = bb.utils.which(os.getenv('PATH'), "apt-get")
        self.apt_cache_cmd = bb.utils.which(os.getenv('PATH'), "apt-cache")

        self.apt_args = d.getVar("APT_ARGS")

        self.all_arch_list = archs.split()
        all_mlb_pkg_arch_list = (self.d.getVar('ALL_MULTILIB_PACKAGE_ARCHS') or "").split()
        self.all_arch_list.extend(arch for arch in all_mlb_pkg_arch_list if arch not in self.all_arch_list)

        self._create_configs(archs, base_archs)

        self.indexer = DpkgIndexer(self.d, self.deploy_dir)

    """
    This function will change a package's status in /var/lib/dpkg/status file.
    If 'packages' is None then the new_status will be applied to all
    packages
    """
    def mark_packages(self, status_tag, packages=None):
        status_file = self.target_rootfs + "/var/lib/dpkg/status"

        with open(status_file, "r") as sf:
            with open(status_file + ".tmp", "w+") as tmp_sf:
                if packages is None:
                    tmp_sf.write(re.sub(r"Package: (.*?)\n((?:[^\n]+\n)*?)Status: (.*)(?:unpacked|installed)",
                                        r"Package: \1\n\2Status: \3%s" % status_tag,
                                        sf.read()))
                else:
                    if type(packages).__name__ != "list":
                        raise TypeError("'packages' should be a list object")

                    status = sf.read()
                    for pkg in packages:
                        status = re.sub(r"Package: %s\n((?:[^\n]+\n)*?)Status: (.*)(?:unpacked|installed)" % pkg,
                                        r"Package: %s\n\1Status: \2%s" % (pkg, status_tag),
                                        status)

                    tmp_sf.write(status)

        os.rename(status_file + ".tmp", status_file)

    """
    Run the pre/post installs for package "package_name". If package_name is
    None, then run all pre/post install scriptlets.
    """
    def run_pre_post_installs(self, package_name=None):
        info_dir = self.target_rootfs + "/var/lib/dpkg/info"
        ControlScript = collections.namedtuple("ControlScript", ["suffix", "name", "argument"])
        control_scripts = [
                ControlScript(".preinst", "Preinstall", "install"),
                ControlScript(".postinst", "Postinstall", "configure")]
        status_file = self.target_rootfs + "/var/lib/dpkg/status"
        installed_pkgs = []

        with open(status_file, "r") as status:
            for line in status.read().split('\n'):
                m = re.match("^Package: (.*)", line)
                if m is not None:
                    installed_pkgs.append(m.group(1))

        if package_name is not None and not package_name in installed_pkgs:
            return

        os.environ['D'] = self.target_rootfs
        os.environ['OFFLINE_ROOT'] = self.target_rootfs
        os.environ['IPKG_OFFLINE_ROOT'] = self.target_rootfs
        os.environ['OPKG_OFFLINE_ROOT'] = self.target_rootfs
        os.environ['INTERCEPT_DIR'] = os.path.join(self.d.getVar('WORKDIR'),
                                                   "intercept_scripts")
        os.environ['NATIVE_ROOT'] = self.d.getVar('STAGING_DIR_NATIVE')

        failed_pkgs = []
        for pkg_name in installed_pkgs:
            for control_script in control_scripts:
                p_full = os.path.join(info_dir, pkg_name + control_script.suffix)
                if os.path.exists(p_full):
                    try:
                        bb.note("Executing %s for package: %s ..." %
                                 (control_script.name.lower(), pkg_name))
                        output = subprocess.check_output([p_full, control_script.argument],
                                stderr=subprocess.STDOUT).decode("utf-8")
                        bb.note(output)
                    except subprocess.CalledProcessError as e:
                        bb.note("%s for package %s failed with %d:\n%s" %
                                (control_script.name, pkg_name, e.returncode,
                                    e.output.decode("utf-8")))
                        failed_pkgs.append(pkg_name)
                        break

        if len(failed_pkgs):
            self.mark_packages("unpacked", failed_pkgs)

    def update(self):
        os.environ['APT_CONFIG'] = self.apt_conf_file

        self.deploy_dir_lock()

        cmd = "%s update" % self.apt_get_cmd

        try:
            subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            bb.fatal("Unable to update the package index files. Command '%s' "
                     "returned %d:\n%s" % (e.cmd, e.returncode, e.output.decode("utf-8")))

        self.deploy_dir_unlock()

    def install(self, pkgs, attempt_only=False):
        if attempt_only and len(pkgs) == 0:
            return

        os.environ['APT_CONFIG'] = self.apt_conf_file

        cmd = "%s %s install --force-yes --allow-unauthenticated %s" % \
              (self.apt_get_cmd, self.apt_args, ' '.join(pkgs))

        try:
            bb.note("Installing the following packages: %s" % ' '.join(pkgs))
            subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            (bb.fatal, bb.warn)[attempt_only]("Unable to install packages. "
                                              "Command '%s' returned %d:\n%s" %
                                              (cmd, e.returncode, e.output.decode("utf-8")))

        # rename *.dpkg-new files/dirs
        for root, dirs, files in os.walk(self.target_rootfs):
            for dir in dirs:
                new_dir = re.sub("\.dpkg-new", "", dir)
                if dir != new_dir:
                    os.rename(os.path.join(root, dir),
                              os.path.join(root, new_dir))

            for file in files:
                new_file = re.sub("\.dpkg-new", "", file)
                if file != new_file:
                    os.rename(os.path.join(root, file),
                              os.path.join(root, new_file))


    def remove(self, pkgs, with_dependencies=True):
        if with_dependencies:
            os.environ['APT_CONFIG'] = self.apt_conf_file
            cmd = "%s purge %s" % (self.apt_get_cmd, ' '.join(pkgs))
        else:
            cmd = "%s --admindir=%s/var/lib/dpkg --instdir=%s" \
                  " -P --force-depends %s" % \
                  (bb.utils.which(os.getenv('PATH'), "dpkg"),
                   self.target_rootfs, self.target_rootfs, ' '.join(pkgs))

        try:
            subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            bb.fatal("Unable to remove packages. Command '%s' "
                     "returned %d:\n%s" % (e.cmd, e.returncode, e.output.decode("utf-8")))

    def write_index(self):
        self.deploy_dir_lock()

        result = self.indexer.write_index()

        self.deploy_dir_unlock()

        if result is not None:
            bb.fatal(result)

    def insert_feeds_uris(self, feed_uris, feed_base_paths, feed_archs):
        if feed_uris == "":
            return

        sources_conf = os.path.join("%s/etc/apt/sources.list"
                                    % self.target_rootfs)
        arch_list = []

        if feed_archs is None:
            for arch in self.all_arch_list:
                if not os.path.exists(os.path.join(self.deploy_dir, arch)):
                    continue
                arch_list.append(arch)
        else:
            arch_list = feed_archs.split()

        feed_uris = self.construct_uris(feed_uris.split(), feed_base_paths.split())

        with open(sources_conf, "w+") as sources_file:
            for uri in feed_uris:
                if arch_list:
                    for arch in arch_list:
                        bb.note('Adding dpkg channel at (%s)' % uri)
                        sources_file.write("deb %s/%s ./\n" %
                                           (uri, arch))
                else:
                    bb.note('Adding dpkg channel at (%s)' % uri)
                    sources_file.write("deb %s ./\n" % uri)

    def _create_configs(self, archs, base_archs):
        base_archs = re.sub("_", "-", base_archs)

        if os.path.exists(self.apt_conf_dir):
            bb.utils.remove(self.apt_conf_dir, True)

        bb.utils.mkdirhier(self.apt_conf_dir)
        bb.utils.mkdirhier(self.apt_conf_dir + "/lists/partial/")
        bb.utils.mkdirhier(self.apt_conf_dir + "/apt.conf.d/")
        bb.utils.mkdirhier(self.apt_conf_dir + "/preferences.d/")

        arch_list = []
        for arch in self.all_arch_list:
            if not os.path.exists(os.path.join(self.deploy_dir, arch)):
                continue
            arch_list.append(arch)

        with open(os.path.join(self.apt_conf_dir, "preferences"), "w+") as prefs_file:
            priority = 801
            for arch in arch_list:
                prefs_file.write(
                    "Package: *\n"
                    "Pin: release l=%s\n"
                    "Pin-Priority: %d\n\n" % (arch, priority))

                priority += 5

            pkg_exclude = self.d.getVar('PACKAGE_EXCLUDE') or ""
            for pkg in pkg_exclude.split():
                prefs_file.write(
                    "Package: %s\n"
                    "Pin: release *\n"
                    "Pin-Priority: -1\n\n" % pkg)

        arch_list.reverse()

        with open(os.path.join(self.apt_conf_dir, "sources.list"), "w+") as sources_file:
            for arch in arch_list:
                sources_file.write("deb file:%s/ ./\n" %
                                   os.path.join(self.deploy_dir, arch))

        base_arch_list = base_archs.split()
        multilib_variants = self.d.getVar("MULTILIB_VARIANTS");
        for variant in multilib_variants.split():
            localdata = bb.data.createCopy(self.d)
            variant_tune = localdata.getVar("DEFAULTTUNE_virtclass-multilib-" + variant, False)
            orig_arch = localdata.getVar("DPKG_ARCH")
            localdata.setVar("DEFAULTTUNE", variant_tune)
            variant_arch = localdata.getVar("DPKG_ARCH")
            if variant_arch not in base_arch_list:
                base_arch_list.append(variant_arch)

        with open(self.apt_conf_file, "w+") as apt_conf:
            with open(self.d.expand("${STAGING_ETCDIR_NATIVE}/apt/apt.conf.sample")) as apt_conf_sample:
                for line in apt_conf_sample.read().split("\n"):
                    match_arch = re.match("  Architecture \".*\";$", line)
                    architectures = ""
                    if match_arch:
                        for base_arch in base_arch_list:
                            architectures += "\"%s\";" % base_arch
                        apt_conf.write("  Architectures {%s};\n" % architectures);
                        apt_conf.write("  Architecture \"%s\";\n" % base_archs)
                    else:
                        line = re.sub("#ROOTFS#", self.target_rootfs, line)
                        line = re.sub("#APTCONF#", self.apt_conf_dir, line)
                        apt_conf.write(line + "\n")

        target_dpkg_dir = "%s/var/lib/dpkg" % self.target_rootfs
        bb.utils.mkdirhier(os.path.join(target_dpkg_dir, "info"))

        bb.utils.mkdirhier(os.path.join(target_dpkg_dir, "updates"))

        if not os.path.exists(os.path.join(target_dpkg_dir, "status")):
            open(os.path.join(target_dpkg_dir, "status"), "w+").close()
        if not os.path.exists(os.path.join(target_dpkg_dir, "available")):
            open(os.path.join(target_dpkg_dir, "available"), "w+").close()

    def remove_packaging_data(self):
        bb.utils.remove(os.path.join(self.target_rootfs,
                                     self.d.getVar('opkglibdir')), True)
        bb.utils.remove(self.target_rootfs + "/var/lib/dpkg/", True)

    def fix_broken_dependencies(self):
        os.environ['APT_CONFIG'] = self.apt_conf_file

        cmd = "%s %s -f install" % (self.apt_get_cmd, self.apt_args)

        try:
            subprocess.check_output(cmd.split(), stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            bb.fatal("Cannot fix broken dependencies. Command '%s' "
                     "returned %d:\n%s" % (cmd, e.returncode, e.output.decode("utf-8")))

    def list_installed(self):
        return DpkgPkgsList(self.d, self.target_rootfs).list_pkgs()

    """
    Returns a dictionary with the package info.
    """
    def package_info(self, pkg):
        cmd = "%s show %s" % (self.apt_cache_cmd, pkg)
        pkg_info = super(DpkgPM, self).package_info(pkg, cmd)

        pkg_arch = pkg_info[pkg]["pkgarch"]
        pkg_filename = pkg_info[pkg]["filename"]
        pkg_info[pkg]["filepath"] = \
                os.path.join(self.deploy_dir, pkg_arch, pkg_filename)

        return pkg_info

    """
    Returns the path to a tmpdir where resides the contents of a package.

    Deleting the tmpdir is responsability of the caller.
    """
    def extract(self, pkg):
        pkg_info = self.package_info(pkg)
        if not pkg_info:
            bb.fatal("Unable to get information for package '%s' while "
                     "trying to extract the package."  % pkg)

        tmp_dir = super(DpkgPM, self).extract(pkg, pkg_info)
        bb.utils.remove(os.path.join(tmp_dir, "data.tar.xz"))

        return tmp_dir

def generate_index_files(d):
    classes = d.getVar('PACKAGE_CLASSES').replace("package_", "").split()

    indexer_map = {
        "rpm": (RpmIndexer, d.getVar('DEPLOY_DIR_RPM')),
        "ipk": (OpkgIndexer, d.getVar('DEPLOY_DIR_IPK')),
        "deb": (DpkgIndexer, d.getVar('DEPLOY_DIR_DEB'))
    }

    result = None

    for pkg_class in classes:
        if not pkg_class in indexer_map:
            continue

        if os.path.exists(indexer_map[pkg_class][1]):
            result = indexer_map[pkg_class][0](d, indexer_map[pkg_class][1]).write_index()

            if result is not None:
                bb.fatal(result)

if __name__ == "__main__":
    """
    We should be able to run this as a standalone script, from outside bitbake
    environment.
    """
    """
    TBD
    """
