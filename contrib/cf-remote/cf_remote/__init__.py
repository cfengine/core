import os

files = os.listdir(__path__[0])
modules = (x.replace(".py", "") for x in files if x.endswith(".py") and not x.startswith("__"))
for module in modules:
    __import__("cf_remote." + module)
