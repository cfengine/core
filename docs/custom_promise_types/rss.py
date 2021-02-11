import requests, html, re, os, random
import xml.etree.ElementTree as ET
from cfengine import PromiseModule, ValidationError, Result


class RssPromiseTypeModule(PromiseModule):
    def __init__(self):
        super().__init__("rss_promise_module", "0.0.2")


    def validate_promise(self, promiser, attributes):
        # check promiser type
        if type(promiser) is not str:
            raise ValidationError("invalid type for promiser: expected string")

        # check that promiser is a valid file path
        if not self._is_unix_file(promiser) and not self._is_win_file(promiser):
            raise ValidationError(f"invalid value '{promiser}' for promiser: must be a filepath")

        # check that required attribute feed is present
        if "feed" not in attributes:
            raise ValidationError("Missing required attribute feed")

        # check that attribute feed has a valid type
        feed = attributes['feed']
        if type(feed) is not str:
            raise ValidationError("Invalid type for attribute feed: expected string")

        # check that attribute feed is a valid file path or url
        if not (self._is_unix_file(feed) or self._is_win_file(feed) or self._is_url(feed)):
            raise ValidationError(f"Invalid value '{feed}' for attribute feed: must be a file path or url")

        # additional checks if optional attribute select is present
        if "select" in attributes:
            select = attributes['select']

            # check that attribute select has a valid type
            if type(select) is not str:
                raise ValidationError(f"Invalid type for attribute select: expected string")

            # check that attribute select has a valid value
            if select != 'newest' and select != 'oldest' and select != 'random':
                raise ValidationError(f"Invalid value '{select}' for attribute select: must be newest, oldest or random")


    def evaluate_promise(self, promiser, attributes):
        # get attriute feed
        feed = attributes['feed']

        # fetch resource
        resource = self._get_resource(feed)
        if not resource:
            return Result.NOT_KEPT

        # parse resource and extract description fields
        items = self._get_items(resource, feed)
        if not items:
            return Result.NOT_KEPT

        # pick item based on optional select attribute (defaults to newest)
        item = self._pick_item(items, attributes)

        # write selected item to promiser file
        result = self._write_promiser(item, promiser)

        return result


    def _get_resource(self, path):
        if self._is_url(path):
            # fetch from url
            self.log_verbose(f"Fetching feed from url '{path}'")
            response = requests.get(path)
            if response.ok:
                return response.content
            self.log_error(f"Failed to fetch feed from url '{path}'': status code '{response.status_code}'")
            return None

        # fetch from file
        try:
            self.log_verbose(f"Reading feed from file '{path}'")
            with open(path, 'r', encoding='utf-8') as f:
                resource = f.read()
                return resource
        except Exception as e:
            self.log_error(f"Failed to open file '{path}' for reading: {e}")
            return None


    def _get_items(self, res, path):
        # extract descriptions in /channel/item
        try:
            self.log_verbose(f"Parsing feed '{path}'")
            items = []
            root = ET.fromstring(res)
            for item in root.findall('./channel/item'):
                for child in item:
                    if child.tag == 'description':
                        items.append(child.text)
            return items
        except Exception as e:
            self.log_error(f"Failed to parse feed '{path}': {e}")
            return None


    def _pick_item(self, items, attributes):
        # Pick newest item as default
        item = items[0]

        # Select item from feed
        if "select" in attributes:
            select = attributes['select']
            if select == 'random':
                self.log_verbose("Selecting random item from feed")
                item = random.choice(items)
            elif select == 'oldest':
                self.log_verbose("Selecting oldest item from feed")
                item = items[- 1]
            else:
                self.log_verbose("Selecting newest item from feed")
        else:
            self.log_verbose("Selecting newest item as default")
        return item


    def _write_promiser(self, item, promiser):
        file_exist = os.path.isfile(promiser)

        if file_exist:
            try:
                with open(promiser, 'r', encoding='utf-8') as f:
                    if f.read() == item:
                        self.log_verbose(f"File '{promiser}' exists and is up to date, no changes needed")
                        return Result.KEPT
            except Exception as e:
                self.log_error(f"Failed to open file '{promiser}' for reading: {e}")
                return Result.NOT_KEPT

        try:
            with open(promiser, 'w', encoding='utf-8') as f:
                if file_exist:
                    self.log_info(f"File '{promiser}' exists but contents differ, updating content")
                else:
                    self.log_info(f"File '{promiser}' does not exist, creating file")
                f.write(item)
                return Result.REPAIRED
        except Exception as e:
            self.log_error(f"Failed to open file '{promiser}' for writing: {e}")
            return Result.NOT_KEPT


    def _is_win_file(self, path):
        return re.search(r"^[a-zA-Z]:\\[\\\S|*\S]?.*$", path) != None


    def _is_unix_file(self, path):
        return re.search(r"^(/[^/ ]*)+/?$", path) != None


    def _is_url(self, path):
        return re.search(r"^http[s]?://(?:[a-zA-Z]|[0-9]|[$-_@.&+]|[!*\(\),]|(?:%[0-9a-fA-F][0-9a-fA-F]))+", path) != None


if __name__ == "__main__":
    RssPromiseTypeModule().start()
