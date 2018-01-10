import sys
import json
from collections import OrderedDict

def dict_sort(old_dict):
    if isinstance(old_dict, list):
        return [dict_sort(x) for x in old_dict]
    if not isinstance(old_dict, dict):
        return old_dict
    new_dict = OrderedDict()
    for key in list(sorted(old_dict)):
        value = old_dict[key]
        new_dict[key] = dict_sort(value)
    return new_dict

for filename in sys.argv[1:]:
    with open(filename, 'r') as fp:
        file_string = fp.read()
    old_json = json.loads(file_string, object_pairs_hook=OrderedDict)
    new_json = dict_sort(old_json)

    file_string = json.dumps(new_json, indent=2)
    with open(filename+".out.json", 'w') as fp:
        fp.write(file_string)
