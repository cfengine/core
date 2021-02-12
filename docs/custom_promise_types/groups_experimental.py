import re
import json
from subprocess import Popen, PIPE
from cfengine import PromiseModule, ValidationError, Result


class GroupsExperimentalPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("groups_experimental_promise_module", "0.0.1")
        self._name_regex = re.compile(r"^[a-z_][a-z0-9_-]*[$]?$")
        self._name_maxlen = 32


    def validate_promise(self, promiser, attributes):
        # check promiser type
        if type(promiser) is not str:
            raise ValidationError("Invalid type for promiser: expected string")

        # check promiser value
        if self._name_regex.match(promiser) is None:
            self.log_warning(f"Promiser groupname '{promiser}' should match regular expression '[a-z_][a-z0-9_-]*[$]?'")

        # check promiser length not too long
        if len(promiser) > self._name_maxlen:
            raise ValidationError(f"Promiser '{promiser}' is too long: ({len(promiser)} > {self._name_maxlen})")

        # check attribute policy if present
        if "policy" in attributes:
            policy = attributes["policy"]

            # check attribute policy type
            if type(policy) is not str:
                raise ValidationError("Invalid type for attribute policy: expected string")

            # check attribute policy value
            if policy not in ("present", "absent"):
                raise ValidationError(f"Invalid value '{policy}' for attribute policy: must be 'present' or 'absent'")

            # check attributes gid and members are not used with policy absent
            if policy == "absent":
                if "gid" in attributes:
                    self.log_warning(f"Cannot assign gid to absent group '{promiser}'")
                if "members" in attributes:
                    self.log_warning(f"Cannot assign members to absent group '{promiser}'")

        # check attribute gid if present
        if "gid" in attributes:
            gid = attributes["gid"]

            # check attribute gid type
            if type(gid) not in (str, int):
                raise ValidationError("Invalid type for attribute gid: expected string or int")

            # check attribute gid value
            if type(gid) == str:
                try: 
                    int(gid)
                except ValueError:
                    raise ValidationError(f"Invalid value '{gid}' for attribute gid: expected integer literal")

        # check attribute members if present
        if "members" in attributes:
            # remove escape chars followed by parsing json
            members = json.loads(json.loads('"' + attributes["members"] + '"'))
            attributes["members"] = members
            
            # check attribute only not used with attributes include or exclude
            if "only" in members and ("include" in members or "exclude" in members):
                raise ValidationError("Attribute 'only' may not be used with attributes 'include' or 'exclude'")
            
            # check attributes of attibutes in members
            for attr in members:
                if attr not in ("only", "include", "exclude"):
                    raise ValidationError(f"Invalid value '{attr}' in attribute members: must be 'only', 'exclude' or 'include'")
            
            # make sure users aren't both included and excluded
            if "include" in members and "exclude" in members:
                duplicates = set(members["include"]).intersection(set(members["exclude"]))
                if duplicates != set():
                    raise ValidationError(f"Users {duplicates} both included and excluded from group '{promiser}'")


    def evaluate_promise(self, promiser, attributes):
        # keep track of any repairs or failed repairs
        failed_repairs = 0
        repairs = 0

        # get group from '/etc/group' if present
        group = None
        try:
            group = Group.lookup(promiser)
        except GroupException as e:
            self.log_error(f"Failed to lookup group '{promiser}': {e}")
            failed_repairs += 1
        
        # get promised gid if present
        promised_gid = attributes.get("gid")

        # parse json in attribute members
        if "members" in attributes:
            # remove escape chars followed by parsing json
            members = json.loads(json.loads('"' + attributes["members"] + '"'))
            attributes["members"] = members

        # set policy to present by default, if not specified
        if "policy" not in attributes:
            self.log_verbose(f"Policy not specified, defaults to present")
            attributes["policy"] = "present"

        # create group if policy present and group absent
        if attributes["policy"] == "present" and group is None:
            self.log_debug(f"Group '{promiser}' should be present, but does not exist")
            try:
                group = Group.create(promiser, promised_gid)
            except GroupException as e:
                self.log_error(f"Failed to create group '{promiser}': {e}")
                failed_repairs += 1
            else:
                self.log_info(f"Created group '{promiser}'")
                repairs += 1

        # delete group if policy absent and group present
        elif attributes["policy"] == "absent" and group is not None:
            self.log_debug(f"Group '{promiser}' should be absent, but does exist")
            try:
                # group is set to None here
                group = group.delete()
            except GroupException as e:
                self.log_error(f"Failed to delete group '{promiser}': {e}")
                failed_repairs += 1
            else:
                self.log_info(f"Deleted group '{promiser}'")
                repairs += 1

        # if group is now present, check attributes 'gid' and 'members'
        if group is not None:
            # check gid if present
            if promised_gid is not None and promised_gid != group.gid:
                self.log_error(f"There is an existing group '{promiser}' with a different GID ({group.gid}) than promised ({promised_gid})")
                # We will not try to repair this, as this might grant permissions to group
                failed_repairs += 1
            
            # check members if present
            if "members" in attributes:
                members = attributes["members"]
                set_members_repairs, set_members_failed_repairs = self._set_members(group, members)
                repairs += set_members_repairs
                failed_repairs += set_members_failed_repairs

        self.log_debug(f"'{repairs}' repairs and '{failed_repairs}' failed repairs to promiser '{promiser}'")
        if failed_repairs > 0:
            self.log_error(f"Promise '{promiser}' not kept")
            return Result.NOT_KEPT

        if repairs > 0:
            self.log_info(f"Promise '{promiser}' repaired")
            return Result.REPAIRED

        self.log_verbose(f"Promise '{promiser}' kept")
        return Result.KEPT


    def _set_members(self, group, members):
        repairs = 0
        failed_repairs = 0

        for attribute in members:
            if attribute == "include":
                users = members["include"]
                include_users_repairs, include_users_failed_repairs = self._include_users(group, users)
                repairs += include_users_repairs
                failed_repairs += include_users_failed_repairs

            elif attribute == "exclude":
                users = members["exclude"]
                exclude_users_repairs, exclude_users_failed_repairs = self._exclude_users(group, users)
                repairs += exclude_users_repairs
                failed_repairs += exclude_users_failed_repairs

            elif attribute == "only":
                users = members["only"]
                only_users_repairs, only_users_failed_repairs = self._only_users(group, users)
                repairs += only_users_repairs
                failed_repairs += only_users_failed_repairs

        return repairs, failed_repairs

    
    def _include_users(self, group, users):
        repairs = 0
        failed_repairs = 0

        for user in users:
            self.log_debug(f"User '{user}' should be included in group '{group.name}'")
            if user in group.members:
                self.log_debug(f"User '{user}' already included in group '{group.name}'")
            else:
                self.log_debug(f"User '{user}' not included in group '{group.name}'")
                try:
                    group.add_member(user)
                except GroupException as e:
                    self.log_error(f"Failed to add user '{user}' to group '{group.name}': {e}")
                    failed_repairs += 1
                else:
                    self.log_info(f"Added user '{user}' to group '{group.name}'")
                    repairs += 1
        
        return repairs, failed_repairs

    
    def _exclude_users(self, group, users):
        repairs = 0
        failed_repairs = 0

        for user in users:
            self.log_debug(f"User '{user}' should be excluded from group '{group.name}'")
            if user in group.members:
                self.log_debug(f"User '{user}' not excluded from group '{group.name}'")
                try:
                    group.remove_member(user)
                except GroupException as e:
                    self.log_error(f"Failed to remove user '{user}' from group '{group.name}': {e}")
                    failed_repairs += 1
                else:
                    self.log_info(f"Removed user '{user}' from group '{group.name}'")
                    repairs += 1
            else:
                self.log_debug(f"User '{user}' already excluded from group '{group.name}'")

        return repairs, failed_repairs

    
    def _only_users(self, group, users):
        repairs = 0
        failed_repairs = 0

        self.log_debug(f"Group '{group.name}' should only contain members {users}")
        if (set(users) != set(group.members)):
            self.log_debug(f"Group '{group.name}' does not only contain members {users}")
            try:
                group.set_members(users)
            except GroupException as e:
                self.log_error(f"Failed to set members of group '{group.name}' to only users {users}: {e}")
                failed_repairs += 1
            else:
                self.log_info(f"Members of group '{group.name}' set to only users {users}")
                repairs += 1
        else:
            self.log_debug(f"Group '{group.name}' does only contain members {users}")
        
        return repairs, failed_repairs


class GroupException(Exception):
    def __init__(self, message):
        self.message = message


class Group:
    def __init__(self, name, gid, members):
        self.name = name
        self.gid = gid
        self.members = members

    
    @staticmethod
    def lookup(group):
        try:
            with open("/etc/group") as f:
                for line in f:
                    if line.startswith(group + ':'):
                        entry = line.strip().split(':')
                        name = entry[0]
                        gid = entry[2]
                        members = entry[3].split(',') if entry[3] else []
                        return Group(name, gid, members)
        except Exception as e:
            raise GroupException(e)
        return None
        

    @staticmethod
    def create(name, gid=None):
        command = ["groupadd", name]
        if (gid):
            command += ["--gid", gid]
        process = Popen(command, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()

        if process.returncode != 0:
            # we'll only use the first line of stderr output, 
            # as remaining lines dwell into too much detail
            msg = stderr.decode("utf-8").strip().split('\n')[0]
            raise GroupException(msg)

        return Group.lookup(name)


    def delete(self):
        command = ["groupdel", self.name]
        process = Popen(command, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()

        if process.returncode != 0:
            # we'll only use the first line of stderr output, 
            # as remaining lines dwell into too much detail
            msg = stderr.decode("utf-8").strip().split('\n')[0]
            raise GroupException(msg)

        return None


    def add_member(self, user):
        command = ["gpasswd", "--add", user, self.name]
        process = Popen(command, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()

        if process.returncode != 0:
            # we'll only use the first line of stderr output, 
            # as remaining lines dwell into too much detail
            msg = stderr.decode("utf-8").strip().split('\n')[0]
            raise GroupException(msg)


    def remove_member(self, user):
        command = ["gpasswd", "--delete", user, self.name]
        process = Popen(command, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()

        if process.returncode != 0:
            # we'll only use the first line of stderr output, 
            # as remaining lines dwell into too much detail
            msg = stderr.decode("utf-8").strip().split('\n')[0]
            raise GroupException(msg)


    def set_members(self, users):
        command = ["gpasswd", "--members", ",".join(users), self.name]
        process = Popen(command, stdout=PIPE, stderr=PIPE)
        stdout, stderr = process.communicate()

        if process.returncode != 0:
            # we'll only use the first line of stderr output, 
            # as remaining lines may dwell into too much detail
            msg = stderr.decode("utf-8").strip().split('\n')[0]
            raise GroupException(msg)


if __name__ == "__main__":
    GroupsExperimentalPromiseTypeModule().start()
