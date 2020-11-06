from os.path import basename, splitext, expanduser, abspath
from cf_remote.web import get_json
from cf_remote.utils import is_in_past, canonify
from cf_remote import log
import re


class Artifact:
    def __init__(self, data, filename=None):
        if filename and not data:
            data = {}
            data["URL"] = abspath(expanduser(filename))
            data["Title"] = ""
            data["Arch"] = None
            if "sparc" in filename.lower():
                data["Arch"] = "spar"
        self.data = data
        self.url = data["URL"]
        self.title = data["Title"]
        self.arch = canonify(data["Arch"]) if data["Arch"] else None

        self.filename = basename(self.url)
        self.extension = splitext(self.filename)[1]

        self.tags = ["any"]
        self.create_tags()

    def create_tags(self):
        if self.arch:
            self.add_tag(self.arch)
        self.add_tag(self.extension[1:])

        look_for_tags = [
            "Windows", "CentOS", "Red Hat", "Debian", "Ubuntu", "SLES", "Solaris", "AIX", "HPUX",
            "HP-UX", "nova", "community", "masterfiles", "OSX", "OS X", "OS-X"
        ]
        for tag in look_for_tags:
            tag = tag.lower()
            filename = self.filename
            if tag in filename:
                self.add_tag(tag)
        if "hp-ux" in self.tags:
            self.add_tag("hpux")
            self.tags.remove("hp-ux")
        if "os-x" in self.tags:
            self.add_tag("osx")
            self.tags.remove("os-x")
        if "os_x" in self.tags:
            self.add_tag("osx")
            self.tags.remove("os_x")
        if "x86" in self.tags:
            self.add_tag("32")
        if "x86_64" in self.tags:
            self.add_tag("64")
        if "i686" in self.tags:
            self.add_tag("32")

        self.add_tags_from_filename(self.filename)

    def add_tag(self, string):
        assert type(string) is str
        canonified = canonify(string)
        if canonified not in self.tags:
            self.tags.append(canonified)

    def add_tags_from_filename(self, filename):
        parts = re.split(r"[-_.]", filename)
        #parts = self.filename.split("-_.")
        for part in parts:
            part = part.strip()
            if part == "x86":
                continue
            if part == "amd64" or part == "x86_64":
                self.add_tag("64")
            try:
                _ = int(part)
            except ValueError:
                self.add_tag(part)
        if "hub" in self.tags:
            self.add_tag("policy_server")
        else:
            self.add_tag("client")
            self.add_tag("agent")
        parts = filename.split(".")
        if "x86_64" in parts:
            self.add_tag("x86_64")
            self.add_tag("64")
        if "amd64" in parts:
            self.add_tag("amd64")
            self.add_tag("64")

    def __str__(self):
        return self.filename + " ({})".format(" ".join(self.tags))

    def __repr__(self):
        return str(self)

def filter_artifacts(artifacts, tags, extension):
    if extension:
        artifacts = [a for a in artifacts if a.extension == extension]
    log.debug("Looking for tags: {}".format(tags))
    log.debug("In artifacts: {}".format(artifacts))
    for tag in tags or []:
        tag = canonify(tag)
        new_artifacts = [a for a in artifacts if tag in a.tags]
        # Have to force evaluation using list comprehension,
        # since we are overwriting artifacts
        if len(new_artifacts) > 0:
            artifacts = new_artifacts

    log.debug("Found artifacts: {}".format(artifacts))
    return artifacts

class Release:
    def __init__(self, data):
        self.data = data
        self.version = data["version"]
        self.url = data["URL"]
        self.lts = data["lts_branch"] if "lts_branch" in data else None
        self.extended_data = None
        self.artifacts = None
        self.default = False

    def init_download(self):
        self.extended_data = get_json(self.url)
        artifacts = self.extended_data["artifacts"]
        self.artifacts = []
        for header in artifacts:
            for blob in artifacts[header]:
                artifact = Artifact(blob)
                artifact.add_tag(header)
                self.artifacts.append(artifact)

    def find(self, tags, extension=None):
        if not self.extended_data:
            self.init_download()
        return filter_artifacts(self.artifacts, tags, extension)

    def __str__(self):
        string = self.version
        if self.lts:
            string += " LTS"
        # string = "{}: {}".format(version, self.url)
        if self.default:
            string += " (default)"
        return string


class Releases:
    def __init__(self, edition="enterprise"):
        assert edition in ["community", "enterprise"]
        self.url = "https://cfengine.com/release-data/{}/releases.json".format(edition)
        self.data = get_json(self.url)
        self.supported_branches = []
        for branch in self.data["lts_branches"]:
            expires = branch["supported_until"]
            expires += "-25"
            # Branch expires 25th day of month
            # This is exact enough and avoids problems with leap years
            if not is_in_past(expires):
                self.supported_branches.append(branch["branch_name"])
            else:
                log.info("LTS branch {} expired on {}".format(branch["branch_name"], expires))

        self.releases = []
        for release in self.data["releases"]:
            rel = Release(release)
            if "status" in release and release["status"] == "unsupported":
                continue
            if (
                (release["version"] != "master")
                and ("lts_branch" not in release)
                and ("latest_stable" not in release)
            ):
                continue
            if "lts_branch" in release and release["lts_branch"] not in self.supported_branches:
                continue
            if "latestLTS" in release and release["latestLTS"] == True:
                self.default = rel
                rel.default = True
            self.releases.append(rel)

    def pick_version(self, version):
        for release in self.data["releases"]:
            if "version" in release and version == release["version"]:
                return Release(release)
            if "lts_branch" in release and version == release["lts_branch"]:
                return Release(release)
        return None

    def __str__(self):
        return ", ".join(str(x.version) for x in self.releases)
