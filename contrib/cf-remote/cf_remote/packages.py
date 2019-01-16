from os.path import basename, splitext
from cf_remote.web import get_json
from cf_remote.utils import pretty, is_in_past, canonify
import cf_remote.log as log
import re


def get_releases():
    data = get_json("https://cfengine.com/release-data/enterprise/releases.json")
    return data["releases"]


def get_latest_releases():
    releases = get_releases()
    for release in releases:
        if "status" in release and release["status"] == "unsupported":
            continue
        if "latest_on_branch" not in release or not release["latest_on_branch"]:
            continue
        yield release


def get_latest_stable():
    releases = get_releases()
    for release in releases:
        if "latest_stable" in release and release["latest_stable"]:
            return release
    return None


def get_latest_stable_data():
    return get_release_data(get_latest_stable())


def get_release_data(release):
    assert "URL" in release
    return get_json(release["URL"])


def get_default_version(edition=False):
    numeric = get_latest_stable()["version"]
    if edition:
        return "CFEngine Enterprise " + numeric
    return numeric


def get_default_version_data():
    return get_latest_stable_data()


class Artifact:
    def __init__(self, data, filename=None):
        if filename and not data:
            data = {}
            data["URL"] = "./" + filename
            data["Title"] = ""
            data["Arch"] = None
            if "sparc" in filename.lower():
                data["Arch"] = "spar"
        self.data = data
        self.url = data["URL"]
        self.title = data["Title"]
        self.arch = canonify(data["Arch"])

        self.filename = basename(self.url)
        self.file_extension = splitext(self.filename)[1]

        self.tags = ["any"]
        self.create_tags()

    def create_tags(self):
        self.add_tag(self.arch)
        self.add_tag(self.file_extension[1:])

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
            try:
                _ = int(part)
            except ValueError:
                self.add_tag(part)

    def __str__(self):
        return self.filename + " ({})".format(" ".join(self.tags))


class Release:
    def __init__(self, data):
        self.data = data
        self.version = data["version"]
        self.url = data["URL"]
        self.lts = data["lts_branch"] if "lts_branch" in data else None
        self.extended_data = get_json(self.url)
        artifacts = self.extended_data["artifacts"]
        self.artifacts = []
        for header in artifacts:
            for blob in artifacts[header]:
                artifact = Artifact(blob)
                artifact.add_tag(header)
                self.artifacts.append(artifact)
        self.default = False

    def find(self, tags):
        artifacts = self.artifacts
        for tag in tags:
            tag = canonify(tag)
            remaining = filter(lambda a: tag in a.tags, artifacts)
            remaining = list(remaining)
            artifacts = remaining
        return artifacts

    def __str__(self):
        string = self.version
        if self.lts:
            string += " LTS"
        # string = "{}: {}".format(version, self.url)
        if self.default:
            string += " (default)"
        return string

    def download(self, path):
        data = self.extended_data
        log.debug(pretty(data))


class Releases:
    def __init__(self):
        self.url = "https://cfengine.com/release-data/enterprise/releases.json"
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
            if "status" in release and release["status"] == "unsupported":
                continue
            if "lts_branch" not in release:
                continue
            if release["lts_branch"] not in self.supported_branches:
                continue
            if "latest_on_branch" not in release:
                continue
            if release["latest_on_branch"] != True:
                continue
            rel = Release(release)
            if "latestLTS" in release and release["latestLTS"] == True:
                self.default = rel
                rel.default = True
            self.releases.append(rel)

    def __str__(self):
        lines = [str(x) for x in self.releases]
        return "\n".join(lines)
