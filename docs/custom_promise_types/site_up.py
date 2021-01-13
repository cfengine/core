import re
import ssl
import urllib.request
import urllib.error
from cfengine import PromiseModule, ValidationError, Result


class SiteUpPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("site_up_promise_module", "0.0.1")

    def validate_promise(self, promiser, attributes):
        if not self.is_url_valid(promiser):
            raise ValidationError(f"URL '{promiser}' is invalid")

    def evaluate_promise(self, url, attributes):
        ssl_ctx = ssl.create_default_context()
        if (
            "skip_ssl_verification" in attributes
            and attributes["skip_ssl_verification"] == "true"
        ):
            ssl_ctx = ssl._create_unverified_context()

        error = None
        try:
            code = urllib.request.urlopen(url, context=ssl_ctx).getcode()
            self.log_info(f"Site '{url}' is UP!")
            return Result.KEPT
        except urllib.error.HTTPError as e:
            # HTTPError exception returns response code and useful when handling exotic HTTP errors
            error = f"Site '{url}' is DOWN! Response code: '{e.code}'"
        except urllib.error.URLError as e:
            # URLError is the base exception class that returns generic info
            error = f"Site '{url}' is DOWN! Reason: '{e.reason}'"
        except Exception as e:
            error = str(e)

        assert error is not None
        self.log_error(error)
        return Result.NOT_KEPT

    def is_url_valid(self, url):
        regex = re.compile(
            r"https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()!@:%_\+.~#?&\/\/=]*)",
            re.IGNORECASE,
        )
        return re.match(regex, url) is not None


if __name__ == "__main__":
    SiteUpPromiseTypeModule().start()
