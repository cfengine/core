import os
import json

from cf_remote import log
from cf_remote.paths import cf_remote_dir
from cf_remote.utils import save_file
from cf_remote.ssh import scp, ssh_sudo, ssh_cmd, auto_connect


@auto_connect
def agent_run(data, *, connection=None):
    host = data["ssh_host"]
    agent = data["agent"]
    print("Triggering an agent run on: '{}'".format(host))
    command = "{} -Kf update.cf".format(agent)
    ssh_func = ssh_cmd if data["os"] == "windows" else ssh_sudo
    output = ssh_func(connection, command)
    log.debug(output)
    command = "{} -K".format(agent)
    output = ssh_func(connection, command)
    log.debug(output)


def disable_password_dialog(host):
    print("Disabling password change on hub: '{}'".format(host))
    api = "https://{}/api/user/admin".format(host)
    d = json.dumps({"password": "password"})
    creds = "admin:admin"
    header = "Content-Type: application/json"
    c = "curl -X POST  -k {} -u {}  -H '{}' -d '{}'".format(api, creds, header, d)
    log.debug(c)
    os.system(c)


def def_json(call_collect=False):
    d = {
        "classes": {
            "mpf_augments_control_enabled": ["any"],
            "services_autorun": ["any"],
            "cfengine_internal_purge_policies": ["any"],
            "cfengine_mp_fr_dependencies_auto_install" : ["any"]
        },
        "vars": {
            "acl": ["0.0.0.0/0",
                    "::/0"],
            "default_data_select_host_monitoring_include": [".*"],
            "default_data_select_policy_hub_monitoring_include": [".*"],
            "control_executor_splaytime": "1",
            "control_executor_schedule": ["any"],
            "control_hub_hub_schedule": ["any"]
        }
    }

    if call_collect:
        d["classes"]["client_initiated_reporting_enabled"] = ["any"]
        d["vars"]["control_server_call_collect_interval"] = "1"
        d["vars"]["mpf_access_rules_collect_calls_admit_ips"] = ["0.0.0.0/0"]
        d["vars"]["control_hub_exclude_hosts"] = ["0.0.0.0/0"]

    return d


@auto_connect
def install_def_json(host, *, connection=None, call_collect=False):
    print("Transferring def.json to hub: '{}'".format(host))
    data = json.dumps(def_json(call_collect), indent=2)
    path = os.path.join(cf_remote_dir("json"), "def.json")
    save_file(path, data)
    scp(path, host, connection=connection)
    ssh_sudo(connection, "cp def.json /var/cfengine/masterfiles/def.json")
