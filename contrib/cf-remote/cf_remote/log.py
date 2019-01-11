levels = {"CRITICAL": 50, "ERROR": 40, "WARNING": 30, "INFO": 20, "DEBUG": 10}

level = levels["WARNING"]


def set_level(lvl):
    lvl = lvl.upper()
    global level
    level = levels[lvl]
    log("Set the log level to {}".format(lvl), "INFO")


def log(msg, lvl):
    lvl = lvl.upper()
    global level
    if levels[lvl] >= level:
        print("[{}] {}".format(lvl, msg))


def critical(msg):
    log(msg, "CRITICAL")


def error(msg):
    log(msg, "ERROR")


def warning(msg):
    log(msg, "WARNING")


def info(msg):
    log(msg, "INFO")


def debug(msg):
    log(msg, "DEBUG")
