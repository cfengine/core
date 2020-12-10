import sys
import os
from cfengine import PromiseModule, ValidationError, Result


class GitPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("git_promise_module", "0.0.2")

    def validate_promise(self, promiser, attributes):
        if not promiser.startswith("/"):
            raise ValidationError(f"File path '{promiser}' must be absolute")
        for name, value in attributes.items():
            if name != "repo":
                raise ValidationError(f"Unknown attribute '{name}' for git promises")
            if name == "repo" and type(value) is not str:
                raise ValidationError(f"'repo' must be string for git promise types")

    def evaluate_promise(self, promiser, attributes):
        folder = promiser
        url = attributes["repo"]

        safe_promiser = promiser.replace(",", "_")

        if os.path.exists(folder):
            return (Result.KEPT, [f"{safe_promiser}_cloned_already"])

        self.log_info(f"Cloning '{url}' -> '{folder}'...")
        os.system(f"git clone {url} {folder} 2>/dev/null")

        if os.path.exists(folder):
            self.log_info(f"Successfully cloned '{url}' -> '{folder}'")
            return (Result.REPAIRED, [f"{safe_promiser}_cloned"])
        else:
            self.log_error(f"Failed to clone '{url}' -> '{folder}'")
            return (Result.NOT_KEPT, [f"{safe_promiser}_clone_failed"])


if __name__ == "__main__":
    GitPromiseTypeModule().start()
