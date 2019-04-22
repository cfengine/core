#!/usr/bin/env python3
"""agentsay - print messages as an agent"""

__copyright__ = "Northern.tech"
__authors__ = ["Ole Herman Schumacher Elgesem"]

# Install (as root):
# curl -L https://raw.githubusercontent.com/cfengine/core/master/contrib/agentsay/agentsay.py -o /usr/local/bin/agentsay && chmod ugo+x /usr/local/bin/agentsay && agentsay install success

import sys


def add_padding(msg, length, padding=" "):
    if len(msg) >= length:
        return msg
    while len(msg) < length:
        msg = padding + msg + padding
    while len(msg) > length:
        msg = msg[0:-1]
        if len(msg) == length:
            break
        if len(msg) > length:
            msg = msg[1:]
    return msg


def sentence_split(msg, max_length):

    if len(msg) < max_length:
        return [msg]

    messages = []
    strings = msg.split(" ")
    current = []
    while len(strings) > 0:
        while len(" ".join(current)) < max_length:
            current.append(strings[0])
            del strings[0]
            if len(strings) == 0:
                messages.append(" ".join(current))
                return messages
        assert len(current) > 0
        if len(current) == 1:
            messages.append(current[0])
            del current[0]
        messages.append(" ".join(current[0:-1]))
        del current[0:-1]
    if len(current) > 0:
        messages.append(" ".join(current))
    return messages


def speak(msg):
    msg = msg.upper()
    agent = [
        "     ● ● ●     ",
        "     ● ● ●     ",
        "               ",
        " ●   ● ● ●   ● ",
        " ●   ● ● ●   ● ",
        " ●   ● ● ●   ● ",
        " ●           ● ",
        "     ● ● ●     ",
        "     ●   ●     ",
        "     ●   ●     ",
        "     ●   ●     "
    ]
    width = max(len(msg) + 2, 31)
    width = min(width, 75)
    messages = sentence_split(msg, 2 * width / 3)

    print("/{}\\".format(add_padding("", width, "-=")))
    print("|{}|".format(add_padding("", width, " ")))
    for msg in messages:
        print("|{}|".format(add_padding(msg, width, " ")))
    print("|{}|".format(add_padding("", width, " ")))
    for line in agent:
        line = add_padding(line, width, " ")
        print("|{}|".format(line))
    print("|{}|".format(add_padding("", width, " ")))
    print("\\{}/".format(add_padding("", width, "-=")))


if __name__ == "__main__":
    msg = " ".join(sys.argv[1:])
    if len(sys.argv) == 1:
        import getpass
        msg = "HELLO, " + getpass.getuser()
    speak(msg)
